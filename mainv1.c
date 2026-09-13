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

#define SPI_BASEADDR    XPAR_SPI_1_BASEADDR
#define BUFFER_SIZE     3

u8 Buffer[BUFFER_SIZE];
u8 RxBuffer[BUFFER_SIZE];

int main(void)
{
	int Status;

		/*
		 * Run the example, specify the Base Address that is generated in
		 * xparameters.h
		 */
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
	Control &= ~(XSP_CR_MANUAL_SS_MASK | XSP_CR_LOOPBACK_MASK); // 确保手动片选和回环都关闭
	Control |= XSP_CR_TRANS_INHIBIT_MASK;
	XSpi_WriteReg(BaseAddress, XSP_CR_OFFSET, Control);


	/*
	 * Initialize the buffer with some data.
	 */
	Buffer[0] = 0x80;//80:read,00:write
	Buffer[1] = 0x00;
	Buffer[2] = 0x00;

	/* 3. 自动片选模式下，选择从机 SS0（低有效，bit0=0） */
	XSpi_WriteReg(BaseAddress, XSP_SSR_OFFSET, 0xFFFFFFFE);

	/*
	 * Enable the device.
	 */
	Control = XSpi_ReadReg(BaseAddress, XSP_CR_OFFSET);
	Control |= XSP_CR_ENABLE_MASK;
	Control &= ~XSP_CR_TRANS_INHIBIT_MASK;
	XSpi_WriteReg(BaseAddress, XSP_CR_OFFSET, Control);

	/* 3. 写入两个数据到发送 FIFO */
	for (i = 0; i < BUFFER_SIZE; i++) {
	    while (XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_TX_FULL_MASK);
	        XSpi_WriteReg(BaseAddress, XSP_DTR_OFFSET, Buffer[i]);
	}

	/*
	 * Wait for the transmit FIFO to transition to empty before checking
	 * the receive FIFO, this prevents a fast processor from seeing the
	 * receive FIFO as empty
	 */
	while (!(XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_TX_EMPTY_MASK));

	/* 7. 从 RX FIFO 读出所有回传的数据 */
	for (i = 0; i < BUFFER_SIZE; i++) {
	    while (XSpi_ReadReg(BaseAddress, XSP_SR_OFFSET) & XSP_SR_RX_EMPTY_MASK);
	        RxBuffer[i] = XSpi_ReadReg(BaseAddress, XSP_DRR_OFFSET) & 0xFF;
	}

	return XST_SUCCESS;
}

