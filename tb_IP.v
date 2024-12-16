`timescale 1ns / 1ps

module tb_IP(
output reg clk,
output reg rst,
//===== axis master =====//
input [31:0] m_data,
input m_valid,
input m_tlast,
output reg m_ready,
//===== axis slave =====//
output reg [31:0] s_data,
output reg s_valid,
input s_ready,
//===== control =====//
output reg en,
output reg  DMA_VALID,
output reg [1:0] command
    );
    //===== module connection =====//
    IP m0(.clk(clk), .rst(rst),
        .m_data(m_data), .m_valid(m_valid), .m_tlast(m_tlast), .m_ready(m_ready),
        .s_data(s_data), .s_valid(s_valid), .s_ready(s_ready),
        .en(en), .DMA_VALID(DMA_VALID), .command(command)
    ) ;
    
    //===== clk ======//
    parameter cycle = 20 ;
    always #(cycle/2) clk = ~clk ;
    
    integer i ;
    initial clk = 0 ;
    initial rst = 0 ;
    initial m_ready = 0 ;
    initial s_data = 0 ;
    initial s_valid = 0 ;
    initial en = 0 ;
    initial DMA_VALID = 0 ;
    initial command = 0 ;
    initial i = 0 ;
    
    initial begin
        #(5*cycle) rst = 1 ;
        #(5*cycle) en = 1 ;
        #(5*cycle) DMA_VALID = 1 ; // DEFAULT => IN
        #(5*cycle) s_valid = 1 ;
        repeat(32)begin
            @(negedge clk) s_data = i ;
            @(posedge clk) i=i+1 ;
        end // IN => W1
        @(negedge clk) s_data = 0 ;
        #(5*cycle)  ;
        s_valid = 0 ;
        DMA_VALID = 0 ; // W1 => W2
        #(5*cycle) ;
        DMA_VALID = 1 ; // W2 => IDLE
        #(5*cycle) command = 2 ; // IDLE => OUT
        #(5*cycle) ;
        m_ready = 1 ;
        command = 0 ;
        #(50*cycle) ; // OUT => W3
        m_ready = 0 ;
        DMA_VALID = 0 ; // W3 => W4
        #(5*cycle) ;
        DMA_VALID = 1 ; // W4 => IDLE
        #(5*cycle) ;
        // read s_data again
        command = 1 ;
        #(5*cycle) ;
        s_valid = 1 ;
        command = 0 ;
        repeat(32)begin
            @(negedge clk) s_data = i ;
            @(posedge clk) i=i+1 ;
        end // IN => W1
        @(negedge clk) s_data = 0 ;
        #(5*cycle)  ;
        s_valid = 0 ;
        DMA_VALID = 0 ; // W1 => W2
        #(5*cycle) ;
        DMA_VALID = 1 ; // W2 => IDLE
        #(20*cycle) ;
        // write m_data again
        #(5*cycle) command = 2 ; // IDLE => OUT
        #(5*cycle) ;
        m_ready = 1 ;
        command = 0 ;
        #(50*cycle) ; // OUT => W3
        m_ready = 0 ;
        DMA_VALID = 0 ; // W3 => W4
        #(5*cycle) ;
        DMA_VALID = 1 ; // W4 => IDLE
        //
        en = 0 ;
        DMA_VALID = 0 ; // IDLE => DEFAULT
        #(5*cycle) ; 
        $finish ;
    end
    
endmodule
