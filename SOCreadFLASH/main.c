/*
 * main.c
 *
 *  Created on: 2026年9月11日
 *      Author: sjtu
 */

#include "xparameters.h"
#include "xstatus.h"
#include "xspi_l.h"
#include "xil_printf.h"

#define SPI_BASEADDR    XPAR_SPI_0_BASEADDR
#define BUFFER_SIZE     4

u8 Buffer[BUFFER_SIZE];
u8 RxBuffer[BUFFER_SIZE];

int main(void)
{
	int Status;

	xil_printf("\r\n===== SPI Bridge (MicroBlaze) Ready =====\r\n");

	Status = XSpi_LowLevelExample(SPI_BASEADDR);
	if (Status != XST_SUCCESS) {
		xil_printf("Spi lowlevel Example Failed\r\n");
		return XST_FAILURE;
	}

	xil_printf("Successfully ran Spi lowlevel Example\r\n");

		/* 打印 RX FIFO 读到的全部字节 */
	xil_printf("---- RX Buffer (%d bytes) ----\r\n", BUFFER_SIZE);
	for (int i = 0; i < BUFFER_SIZE; i++) {
		xil_printf("RxBuffer[%d] = 0x%02X\r\n", i, RxBuffer[i]);
	}

	return XST_SUCCESS;
}

int XSpi_LowLevelExample(u32 BaseAddress)
{
	u32 Control;
	int i;

	/*
	 * Set up the device in loopback mode and enable master mode.
	 */
	Control = XSpi_ReadReg(BaseAddress, XSP_CR_OFFSET);
	Control |= XSP_CR_MASTER_MODE_MASK;
	Control |= XSP_CR_MANUAL_SS_MASK;      // ★启用手动片选
	Control &= ~XSP_CR_LOOPBACK_MASK; // 回环关闭
	Control |= XSP_CR_TRANS_INHIBIT_MASK;
	XSpi_WriteReg(BaseAddress, XSP_CR_OFFSET, Control);


	/*
	 * Initialize the buffer with some data.
	 */
	Buffer[0] = 0x9F;//80:read,00:write
	for (int j = 1; j < BUFFER_SIZE; j++) {
	    Buffer[j] = 0x00;
	}

	// 3. 手动拉低 SS0（bit0 = 0，其余为 1）
	XSpi_WriteReg(BaseAddress, XSP_SSR_OFFSET, 0xFFFFFFFE);

	/*
	 * Enable the device.
	 */
	Control = XSpi_ReadReg(BaseAddress, XSP_CR_OFFSET);
	Control |= XSP_CR_ENABLE_MASK;
	Control &= ~XSP_CR_TRANS_INHIBIT_MASK;
	XSpi_WriteReg(BaseAddress, XSP_CR_OFFSET, Control);

	/* 3. 写入数据到发送 FIFO */
	for (i = 0; i < BUFFER_SIZE; i++) {
	    while (XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_TX_FULL_MASK);
	    XSpi_WriteReg(BaseAddress, XSP_DTR_OFFSET, Buffer[i]);


	/*
	 * Wait for the transmit FIFO to transition to empty before checking
	 * the receive FIFO, this prevents a fast processor from seeing the
	 * receive FIFO as empty
	 */
	    u32 timeout = 100000;
	    while ((XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_RX_EMPTY_MASK) && timeout--);

	/* 7. 从 RX FIFO 读出所有回传的数据 */
	    if (timeout == 0) {
	        xil_printf("RX timeout at i=%d\r\n", i);
	        break;
	    }
	    RxBuffer[i] = XSpi_ReadReg(BaseAddress, XSP_DRR_OFFSET) & 0xFF;
	}
	// 8. 手动拉高 SS0
	XSpi_WriteReg(BaseAddress, XSP_SSR_OFFSET, 0xFFFFFFFF);

	return XST_SUCCESS;
}

