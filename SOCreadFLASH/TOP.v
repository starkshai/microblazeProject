`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2026/09/11 19:36:09
// Design Name: 
// Module Name: TOP
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


module TOP(
/* SPI_1 接口 */
    inout wire         spi_sdio,
    output wire         spi_sck,
    output wire  spi_cs_n,

    /* UART 接口 */
    input         UART_0_rxd,
    output        UART_0_txd,

    /* QSPI Flash 接口 */
    output         flash_cs,
    input          flash_miso,
    output         flash_mosi,

    /* 差分系统时钟 */
    input         sysclk_clk_n,
    input         sysclk_clk_p,
    
    output wire     fan_pwm
    );
    
wire sysclk;
wire         SPI_1_io0_io;
wire SPI_1_io1_io;
wire         SPI_1_sck_io;
wire  SPI_1_ss_io;

wire    flash_sck;

assign fan_pwm=1'b0;

IBUFGDS u_ibufgds(
.O(sysclk),
.I(sysclk_clk_p),
.IB(sysclk_clk_n)
);

STARTUPE2  #(
    .PROG_USR("FALSE"), // Activate program event security feature. Requires encrypted bitstreams.
    .SIM_CCLK_FREQ(0.0) // Set the Configuration Clock Frequency(ns) for simulation
)
STARTUPE2_inst
(
    .CFGCLK(), // 1-bit output: Configuration main clock output
    .CFGMCLK(), // 1-bit output: Configuration internal oscillator clock output
    .EOS(), // 1-bit output: Active high output signal indicating the End Of Startup.
    .PREQ(), // 1-bit output: PROGRAM request to fabric output
    .CLK(0), // 1-bit input: User start-up clock input
    .GSR(0), // 1-bit input: Global Set/Reset input (GSR cannot be used for the port name)
    .GTS(0), // 1-bit input: Global 3-state input (GTS cannot be used for the port name)
    .KEYCLEARB(1), // 1-bit input: Clear AES Decrypter Key input from Battery-Backed RAM (BBRAM)
    .PACK(1), // 1-bit input: PROGRAM acknowledge input
    .USRCCLKO(flash_sck), // 1-bit input: User CCLK input
    .USRCCLKTS(0), // 1-bit input: User CCLK 3-state enable input
    .USRDONEO(1), // 1-bit input: User DONE pin output control
    .USRDONETS(1) // 1-bit input: User DONE 3-state enable output
);
    
system_wrapper u_system_wrapper (
            .mosi       (SPI_1_io0_io),
            .miso       (SPI_1_io1_io),
            .sck       (SPI_1_sck_io),
            .cs        (SPI_1_ss_io),
            
            .flash_cs   (flash_cs),
            .flash_miso (flash_miso),
            .flash_mosi (flash_mosi),
            .flash_sck  (flash_sck),
    
            .UART_0_rxd         (UART_0_rxd),
            .UART_0_txd         (UART_0_txd),

            .sysclk       (sysclk)
        );    


reg spi_sck_dbg;
reg spi_mosi_dbg;
reg spi_miso_dbg;
reg spi_ss_dbg;

always @(posedge sysclk) begin
    spi_sck_dbg  <= SPI_1_sck_io;
    spi_mosi_dbg <= SPI_1_io0_io;
    spi_miso_dbg <= SPI_1_io1_io;
    spi_ss_dbg   <= SPI_1_ss_io;
end

spi4to3 u_spi4to3 (
        .sck_o   (SPI_1_sck_io),     // 来自 IP 核的 SPI 时钟
        .ss_o    (SPI_1_ss_io),   // 来自 IP 核的片选（取第 0 位，因为只配置了 1 个从机）
        .io0_o   (SPI_1_io0_io),     // 来自 IP 核的 MOSI 数据        
        .io1_i   (SPI_1_io1_io),     // 回传给 IP 核的 MISO 数据
        
        .sck     (spi_sck),       // 输出到外部三线设备的 SCK
        .cs_o    (spi_cs_n),      // 输出到外部三线设备的片选（低有效）
        .sdio    (spi_sdio)       // 双向数据线（SDIO）
    );

ila_0 ila_spi (
	.clk(sysclk), // input wire clk
	.probe0(spi_sck_dbg), // input wire [0:0]  probe0  
	.probe1(spi_ss_dbg), // input wire [0:0]  probe1 
	.probe2(spi_mosi_dbg), // input wire [0:0]  probe2 
	.probe3(spi_miso_dbg) // input wire [0:0]  probe3
);

ila_0 ila_flash (
	.clk(sysclk), // input wire clk
	.probe0(flash_cs), // input wire [0:0]  probe0  
	.probe1(flash_miso), // input wire [0:0]  probe1 
	.probe2(flash_mosi), // input wire [0:0]  probe2 
	.probe3(flash_sck) // input wire [0:0]  probe3
);
    
    
endmodule
