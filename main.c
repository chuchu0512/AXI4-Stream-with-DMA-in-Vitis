
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

#include "xaxidma.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "sleep.h"

#define BASE_ADDR				XPAR_PSU_DDR_0_S_AXI_BASEADDR + 0x01000000
#define TX_ADDR0				BASE_ADDR + 0x00100000
#define TX_ADDR1				BASE_ADDR + 0x00200000
#define RX_ADDR					BASE_ADDR + 0x00300000
#define RESET_TIMEOUT_COUNTER	10000

volatile int RxDone ;
volatile int TxDone ;
volatile int Error ;

u8 *TxBufferPtr0 = (u8*) TX_ADDR0 ;
u8 *TxBufferPtr1 = (u8*) TX_ADDR1 ;
u8 *RxBufferPtr = (u8*) RX_ADDR ;

//===== DMA interrupt =====//
static void handler_tx(void* Callback){
	//xil_printf("handler_rx\n\r") ;
	XAxiDma* AxiDmaInst = (XAxiDma*)Callback ;
	u32 IrqStatus ;
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DMA_TO_DEVICE) ;
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DMA_TO_DEVICE) ;
	if(!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)){
		return ;
	}
	if((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)){
		Error = 1 ;
		XAxiDma_Reset(AxiDmaInst) ;
		int TimeOut = RESET_TIMEOUT_COUNTER ;
		while(TimeOut){
			if(XAxiDma_ResetIsDone(AxiDmaInst)){
				break ;
			}
			TimeOut -= 1 ;
		}
		return ;
	}
	if((IrqStatus & XAXIDMA_IRQ_IOC_MASK)){
		TxDone = 1 ;
	}
}

static void handler_rx(void* Callback){
	//xil_printf("handler_rx\n\r") ;
	XAxiDma* AxiDmaInst = (XAxiDma*)Callback ;
	u32 IrqStatus ;
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA) ;
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DEVICE_TO_DMA) ;
	if(!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)){
		return ;
	}
	if((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)){
		Error = 1 ;
		XAxiDma_Reset(AxiDmaInst) ;
		int TimeOut = RESET_TIMEOUT_COUNTER ;
		while(TimeOut){
			if(XAxiDma_ResetIsDone(AxiDmaInst)){
				break ;
			}
			TimeOut -= 1 ;
		}
		return ;
	}
	if((IrqStatus & XAXIDMA_IRQ_IOC_MASK)){
		RxDone = 1 ;
	}
}

int main()
{
    init_platform();

    XAxiDma axi_dma ;
    XScuGic intc ;

    //===== DMA Settings =====//
    XAxiDma_Config* dma_config = NULL ;
    dma_config = XAxiDma_LookupConfig(XPAR_AXI_DMA_0_DEVICE_ID) ;
    if(dma_config == NULL){
    	xil_printf("XAxiDma_LoopupConfig failed!!\n\r") ;
    }
    int status = -1 ;
    status = XAxiDma_CfgInitialize(&axi_dma, dma_config) ;
    if(status != XST_SUCCESS){
    	xil_printf("XAxiDma_CfgInitialize failed!!\n\r") ;
    }

    //===== Interrupt Settings =====//
    XScuGic_Config* intc_config = NULL ;
    intc_config = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID) ;
    if(intc_config == NULL){
    	xil_printf("XScuGic_LookupConfig failed!!\n\r") ;
    }
    status = XScuGic_CfgInitialize(&intc, intc_config, intc_config->CpuBaseAddress) ;
    if(status != XST_SUCCESS){
    	xil_printf("XScuGic_CfgInitialize failed!!\n\r") ;
    }
    XScuGic_SetPriorityTriggerType(&intc, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR, 0xA0, 0x3) ;
    XScuGic_SetPriorityTriggerType(&intc, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR, 0xA0, 0x3) ;
    status = XScuGic_Connect(&intc, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR, (Xil_InterruptHandler)handler_rx, &axi_dma) ;
    if(status != XST_SUCCESS){
    	xil_printf("XScuGic_Connect Rx failed!!\n\r") ;
    }
    status = XScuGic_Connect(&intc, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR, (Xil_InterruptHandler)handler_tx, &axi_dma) ;
    if(status != XST_SUCCESS){
    	xil_printf("XScuGic_Connect Tx failed!!\n\r") ;
    }
    XScuGic_Enable(&intc, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR) ;
    XScuGic_Enable(&intc, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR) ;

    Xil_ExceptionInit() ;
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, &intc) ;
    Xil_ExceptionEnable() ;


    XAxiDma_IntrDisable(&axi_dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA) ;
    XAxiDma_IntrEnable(&axi_dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA) ;
    XAxiDma_IntrDisable(&axi_dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE) ;
    XAxiDma_IntrEnable(&axi_dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE) ;

    RxDone = 0 ;
    TxDone = 0 ;
    Error = 0 ;

    //===== GPIO =====//
    XGpio en ;
    XGpio DMA_VALID ;
    XGpio command ;
    XGpio_Initialize(&en, XPAR_AXI_GPIO_0_DEVICE_ID) ;
    XGpio_Initialize(&DMA_VALID, XPAR_AXI_GPIO_0_DEVICE_ID) ;
    XGpio_Initialize(&command, XPAR_AXI_GPIO_1_DEVICE_ID) ;
    XGpio_SetDataDirection(&en, 1, 0) ;
    XGpio_SetDataDirection(&DMA_VALID, 2, 0) ;
    XGpio_SetDataDirection(&command, 1, 0) ;

    //===== input test data =====//
    int in_0[32] = {786, 91, 4357, 9, 97, 84, 5138, 776, 1370, 106, 5017, 48, 7, 6, 4788, 39, 101, 7467, 10, 4, 7456, 7, 10, 174, 6516, 1734, 9839, 27, 46, 5, 310, 1} ;
    int in_1[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32} ;
    for(int i=0; i<32; ++i){
    	Xil_Out32(TX_ADDR0 + 4*i, in_0[i]) ;
    	Xil_Out32(TX_ADDR1 + 4*i, in_1[i]) ;
    }

    //===== start IP test =====//
    // DEFAULT => IN
    XGpio_DiscreteWrite(&en, 1, 1) ; //en = 1
    XGpio_DiscreteWrite(&command, 1, 0) ; //command = 0

    Xil_DCacheInvalidateRange((UINTPTR)TxBufferPtr0, (32*4)) ; // IN_A + IN_B = 512
    usleep(1) ;
    status = XAxiDma_SimpleTransfer(&axi_dma, (UINTPTR)TxBufferPtr0, (32*4), XAXIDMA_DMA_TO_DEVICE) ;
    if(status != XST_SUCCESS){
     	xil_printf("XAxiDma_SimpleTransfer XAXIDMA_DMA_TO_DEVICE failed!!\n\r") ;
       	return XST_FAILURE ;
    }
    usleep(1) ;
    XGpio_DiscreteWrite(&DMA_VALID, 2, 1) ; // DMA_VALID = 1

    // IN => W1 (first input data)
    while(TxDone == 0){
    	;
    }
    TxDone = 0 ;
    if(Error == 1){
    	xil_printf("handler_tx error\n\r") ;
    }
    Xil_DCacheFlushRange((UINTPTR)TxBufferPtr0, (4*32)) ;
    usleep(1) ;

    // W1 => W2
    XGpio_DiscreteWrite(&DMA_VALID, 2, 0) ; // DMA_VALID = 0
    usleep(1) ;

    // W2 => IDLE (command = 0)
    XGpio_DiscreteWrite(&command, 1, 0) ; // command = 0
    usleep(1) ;
    XGpio_DiscreteWrite(&DMA_VALID, 2, 1) ; // DMA_VALID = 1
    usleep(1) ;

    // IDLE => OUT
    Xil_DCacheInvalidateRange((UINTPTR)RxBufferPtr, (32*4)) ;
    status = XAxiDma_SimpleTransfer(&axi_dma, (UINTPTR)RxBufferPtr, 32*4, XAXIDMA_DEVICE_TO_DMA) ;
    if(status != XST_SUCCESS){
    	xil_printf("XAxiDma_SimpleTransfer XAXIDMA_DEVICE_TO_DMA failed!!\n\r") ;
    	return XST_FAILURE ;
    }
    usleep(1) ;
    XGpio_DiscreteWrite(&command, 1, 2) ; // command = 2

    // OUT => W3
    while(RxDone == 0){
    	;
    }
    if(Error == 1){
    	xil_printf("handler_rx error\n\r") ;
    }
    Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, (32*4)) ;
    usleep(1) ;
    // output test
    for(int i=0; i<32; ++i){
    	xil_printf("Rx Data %d = %d\n\r", i, Xil_In32(RX_ADDR + 4*i)) ;
    }

    // W3 => W4
    XGpio_DiscreteWrite(&DMA_VALID, 2, 0) ; // DMA_VALID = 0
    usleep(1) ;

    // W4 => IDLE (command = 0)
    XGpio_DiscreteWrite(&command, 1, 0) ; // command = 0
    usleep(1) ;
    XGpio_DiscreteWrite(&DMA_VALID, 2, 1) ; // DMA_VALID = 1
    usleep(1) ;

    // IDLE => IN (input new data)
    Xil_DCacheInvalidateRange((UINTPTR)TxBufferPtr1, (32*4)) ;
    usleep(1) ;
    status = XAxiDma_SimpleTransfer(&axi_dma, (UINTPTR)TxBufferPtr1, (32*4), XAXIDMA_DMA_TO_DEVICE) ;
    if(status != XST_SUCCESS){
     	xil_printf("XAxiDma_SimpleTransfer XAXIDMA_DMA_TO_DEVICE failed!!\n\r") ;
       	return XST_FAILURE ;
    }
    usleep(1) ;
    XGpio_DiscreteWrite(&command, 1, 1) ; // command = 1

    // IN => W1
    while(TxDone == 0){
    	;
    }
    TxDone = 0 ;
    if(Error == 1){
    	xil_printf("handler_tx error\n\r") ;
    }
    Xil_DCacheFlushRange((UINTPTR)TxBufferPtr1, (4*32)) ;
    usleep(1) ;

    // W1 => W2
    XGpio_DiscreteWrite(&DMA_VALID, 2, 0) ; // DMA_VALID = 0
    usleep(1) ;

    // W2 => IDLE (command = 0)
    XGpio_DiscreteWrite(&command, 1, 0) ; // command = 0
    usleep(1) ;
    XGpio_DiscreteWrite(&DMA_VALID, 2, 1) ; // DMA_VALID = 1
    usleep(1) ;

    // IDLE => OUT (output new data)
    Xil_DCacheInvalidateRange((UINTPTR)RxBufferPtr, (32*4)) ;
    status = XAxiDma_SimpleTransfer(&axi_dma, (UINTPTR)RxBufferPtr, 32*4, XAXIDMA_DEVICE_TO_DMA) ;
    if(status != XST_SUCCESS){
    	xil_printf("XAxiDma_SimpleTransfer XAXIDMA_DEVICE_TO_DMA failed!!\n\r") ;
    	return XST_FAILURE ;
    }
    usleep(1) ;
    XGpio_DiscreteWrite(&command, 1, 2) ; // command = 2

    // OUT => W3
    while(RxDone == 0){
    	;
    }
    if(Error == 1){
    	xil_printf("handler_rx error\n\r") ;
    }
    Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, (32*4)) ;
    usleep(1) ;
    // output test
    for(int i=0; i<32; ++i){
    	xil_printf("Rx Data %d = %d\n\r", i, Xil_In32(RX_ADDR + 4*i)) ;
    }

    // W3 => W4
    XGpio_DiscreteWrite(&DMA_VALID, 2, 0) ; // DMA_VALID = 0
    usleep(1) ;

    // W4 => IDLE (command = 0)
    XGpio_DiscreteWrite(&command, 1, 0) ; // command = 0
    usleep(1) ;
    XGpio_DiscreteWrite(&DMA_VALID, 2, 1) ; // DMA_VALID = 1
    usleep(1) ;


    // IDLE => DEFAULT
    XGpio_DiscreteWrite(&en, 1, 0) ; // en = 0
    XGpio_DiscreteWrite(&DMA_VALID, 2, 0) ; // DMA_VALID = 0




    cleanup_platform();
    return 0;
}
