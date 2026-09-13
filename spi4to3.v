`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2026/09/12 13:59:37
// Design Name: 
// Module Name: spi4to3
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module spi4to3(
    input sck_o,
    input ss_o,
    input io0_o,
    output wire io1_i,
    
    output wire sck,
    output wire cs_o,
    inout wire sdio
    );
    assign sck = sck_o;
    assign cs_o = ss_o;
    
        localparam IDLE       = 3'd0;
        localparam CTRL1      = 3'd1;  // 第 1 字节（含 R/W 位）
        localparam CTRL2      = 3'd2;  // 第 2 字节
        localparam DATA_READ  = 3'd3;  // 第 3 字节：读（接收）
        localparam DATA_WRITE = 3'd4;  // 第 3 字节：写（发送）    
        
    reg [2:0] state;
    reg[2:0] bit_cnt;
    reg is_read;
    wire sdio_dir;
    
    always @(posedge sck_o or posedge ss_o) begin
            if (ss_o) begin
                state   <= IDLE;
                bit_cnt <= 3'd0;
                is_read <= 1'b0;
            end else begin
                case (state)
                            IDLE: begin
                                // 第一个上升沿：锁存 R/W 位，进入第 1 字节
                                is_read <= io0_o;
                                bit_cnt <= 3'd1;
                                state   <= CTRL1;
                            end
            
                            CTRL1: begin
                                if (bit_cnt == 3'd7) begin
                                    bit_cnt <= 3'd0;
                                    state   <= CTRL2;
                                end else begin
                                    bit_cnt <= bit_cnt + 1'b1;
                                end
                            end
            
                            CTRL2: begin
                                if (bit_cnt == 3'd7) begin
                                    bit_cnt <= 3'd0;
                                    // 根据第 1 位决定的 R/W，选择第 3 字节方向
                                    state   <= is_read ? DATA_READ : DATA_WRITE;
                                end else begin
                                    bit_cnt <= bit_cnt + 1'b1;
                                end
                            end
            
                            DATA_READ: begin
                                if (bit_cnt == 3'd7) begin
                                    bit_cnt <= 3'd0;
                                    state   <= IDLE;
                                end else begin
                                    bit_cnt <= bit_cnt + 1'b1;
                                end
                            end
            
                            DATA_WRITE: begin
                                if (bit_cnt == 3'd7) begin
                                    bit_cnt <= 3'd0;
                                    state   <= IDLE;
                                end else begin
                                    bit_cnt <= bit_cnt + 1'b1;
                                end
                            end
            
                            default: state <= IDLE;
                        endcase
            end
        end
    
    assign sdio_dir = (state == DATA_READ) ? 1'b1 : 1'b0;
    assign sdio = (sdio_dir == 1'b0) ? io0_o : 1'bz;
    assign io1_i = sdio;//readback all bits
    
endmodule
