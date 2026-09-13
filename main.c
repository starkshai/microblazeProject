/*
 * main.c
 *
 *  Created on: 2026年9月13日
 *      Author: sjtu
 */

#include "xparameters.h"
#include "xstatus.h"
#include "xspi_l.h"
#include "xuartlite_l.h"
#include "xil_printf.h"

#define SPI_BASEADDR    XPAR_SPI_1_BASEADDR
#define UART_BASEADDR   XPAR_MINSOC_AXI_UARTLITE_0_BASEADDR
#define FRAME_SIZE      3

u8 TxBuffer[FRAME_SIZE];
u8 RxBuffer[FRAME_SIZE];

// UART接收1字节（阻塞式）
u8 uart_recv_byte(void) {
    while (XUartLite_IsReceiveEmpty(UART_BASEADDR)); // 等待接收FIFO非空
    return XUartLite_RecvByte(UART_BASEADDR);
}

// UART发送1字节（阻塞式）
void uart_send_byte(u8 data) {
    XUartLite_SendByte(UART_BASEADDR, data);
}

int spi_transfer(u32 BaseAddress, u8 *tx, u8 *rx, int len) {
    u32 Control;
    int i;

    // 1. 配置为主机模式、自动片选、禁止回环
    Control = XSpi_ReadReg(BaseAddress, XSP_CR_OFFSET);
    Control |= XSP_CR_MASTER_MODE_MASK;
    Control &= ~(XSP_CR_MANUAL_SS_MASK | XSP_CR_LOOPBACK_MASK);
    Control |= XSP_CR_TRANS_INHIBIT_MASK;
    XSpi_WriteReg(BaseAddress, XSP_CR_OFFSET, Control);

    // 2. 选择从机 SS0
    XSpi_WriteReg(BaseAddress, XSP_SSR_OFFSET, 0xFFFFFFFE);

    // 3. 使能 SPI 并允许传输
    Control = XSpi_ReadReg(BaseAddress, XSP_CR_OFFSET);
    Control |= XSP_CR_ENABLE_MASK;
    Control &= ~XSP_CR_TRANS_INHIBIT_MASK;
    XSpi_WriteReg(BaseAddress, XSP_CR_OFFSET, Control);

    // 4. 写入数据
    for (i = 0; i < len; i++) {
        while (XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_TX_FULL_MASK);
        XSpi_WriteReg(BaseAddress, XSP_DTR_OFFSET, tx[i]);
    }

    // 5. 等待发送完成
    while (!(XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_TX_EMPTY_MASK));

    // 6. 读取数据
    for (i = 0; i < len; i++) {
        while (XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_RX_EMPTY_MASK);
        rx[i] = XSpi_ReadReg(BaseAddress, XSP_DRR_OFFSET) & 0xFF;
    }
    return XST_SUCCESS;
}

int main(void) {
    int i;
    xil_printf("\r\n===== SPI Bridge (MicroBlaze) Ready =====\r\n");
    xil_printf("Send 3 bytes (CMD1 CMD2 DATA) to trigger SPI transfer.\r\n");

    while (1) {
        // 1. 从串口读取3字节到发送缓冲
        for (i = 0; i < FRAME_SIZE; i++) {
            TxBuffer[i] = uart_recv_byte();
        }
        xil_printf("TX: %02X %02X %02X\r\n", TxBuffer[0], TxBuffer[1], TxBuffer[2]);

        // 2. 执行SPI传输
        spi_transfer(SPI_BASEADDR, TxBuffer, RxBuffer, FRAME_SIZE);

        // 3. 将接收到的数据回传到串口
        xil_printf("RX: %02X %02X %02X\r\n", RxBuffer[0], RxBuffer[1], RxBuffer[2]);
    }
    return XST_SUCCESS;
}
