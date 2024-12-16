`timescale 1ns / 1ps
module IP(
input clk,
input rst,
//===== axis master =====//
output reg [31:0] m_data,
output reg m_valid,
output reg m_tlast,
input m_ready,
//===== axis slave =====//
input [31:0] s_data,
input s_valid,
output reg s_ready,
//===== control =====//
input en,
input DMA_VALID,
input [1:0] command
    );
    //===== memory =====//
    integer i ;
    reg [31:0] mem [31:0] ;
    //===== state =====//
    parameter DEFAULT = 3'b000 ;//0
    parameter IN      = 3'b001 ;//1
    parameter W1      = 3'b010 ;//2
    parameter W2      = 3'b011 ;//3
    parameter IDLE    = 3'b100 ;//4
    parameter OUT     = 3'b101 ;//5
    parameter W3      = 3'b110 ;//6
    parameter W4      = 3'b111 ;//7
    
    reg [2:0] cs, ns ;
    reg [4:0] cnt ; // cnt = 32
    
    always@(posedge clk)begin
        if(!rst) cs <= DEFAULT ;
        else cs <= ns ;
    end
    
    always@(*)begin
        case(cs)
            DEFAULT:begin
                if(en == 1 && DMA_VALID == 1) ns = IN ;
                else ns = DEFAULT ;
            end
            IN:begin
                if(cnt == 31) ns = W1 ;
                else ns = IN ;
            end
            W1:begin
                if(DMA_VALID == 0) ns = W2 ;
                else ns = W1 ;
            end
            W2:begin
                if(DMA_VALID == 1) ns = IDLE ;
                else ns = W2 ;
            end
            IDLE:begin
                if(DMA_VALID == 0 && en == 0) ns = DEFAULT ;
                else begin
                    if(command == 1) ns = IN ;
                    else if(command == 2) ns = OUT ;
                    else ns = IDLE ;
                end
            end
            OUT:begin
                if(cnt == 31) ns = W3 ;
                else ns = OUT ;
            end
            W3:begin
                if(DMA_VALID == 0) ns = W4 ;
                else ns = W3 ;
            end
            W4:begin
                if(DMA_VALID == 1) ns = IDLE ;
                else ns = W4 ;
            end
            default:ns = DEFAULT ;
        endcase
    end
    //===== cnt =====//
    always@(posedge clk)begin
        if(!rst) cnt <= 0 ;
        else begin
            case(cs)
                IN:begin
                    if(s_ready == 1 && s_valid == 1) cnt <= cnt + 1 ;
                    else cnt <= cnt ;
                end
                OUT:begin
                    if(m_ready == 1 && m_valid == 1) cnt <= cnt + 1 ;
                    else cnt <= cnt ;
                end
                default: cnt <= 0 ;
            endcase
        end
    end
    //===== s_ready =====//
    always@(posedge clk)begin
        if(!rst) s_ready <= 0 ;
        else begin
            case(cs)
                IN:begin
                    if(cnt == 31) s_ready <= 0 ;
                    else s_ready <= 1 ;
                end
                default: s_ready <= 0 ;
            endcase
        end
    end
    //===== s_data =====//
    always@(posedge clk)begin
        if(!rst) begin
            for(i=0; i<32; i=i+1)begin
                mem[i] <= 0 ;
            end
        end
        else begin
            case(cs)
                IN:begin
                    if(s_ready == 1 && s_valid == 1) mem[cnt] <= s_data ;
                    else mem[cnt] <= mem[cnt] ;
                end
                default:begin
                    for(i=0; i<32; i=i+1)begin
                        mem[i] <= mem[i] ;
                    end
                end
            endcase        
        end
    end
    //===== m_valid =====//
    always@(posedge clk)begin
        if(!rst) m_valid <= 0 ;
        else begin
            case(cs)
                OUT:begin
                    if(cnt == 31) m_valid <= 0 ;
                    else begin
                        if(m_ready == 1) m_valid <= 1 ;
                        else m_valid <= 0 ;
                    end
                end
                default: m_valid <= 0 ;
            endcase
        end
    end
    //===== m_tlast =====//
    always@(posedge clk)begin
        if(!rst) m_tlast <= 0 ;
        else begin
            case(cs)
                OUT:begin
                    if(cnt == 30) m_tlast <= 1 ;
                    else m_tlast <= 0 ;
                end
                default: m_tlast <= 0 ;
            endcase
        end
    end
    //===== m_data =====//
    always@(posedge clk)begin
        if(!rst) m_data <= 0 ;
        else begin
            case(cs)
                OUT:begin
                    if(m_ready == 0 || m_valid == 0)begin
                        if(cnt == 0) m_data <= mem[cnt] ;
                        else m_data <= m_data ;
                    end
                    else begin
                        if(m_tlast == 1) m_data <= 0 ;
                        else m_data <= mem[cnt + 1] ;                 
                    end
                end
            endcase
        end
    end
    
endmodule
