#include "xparameters.h"
#include "xspi.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xuartlite_l.h"

/* ------------------------------------------------------------------ */
/* 参数定义                                                            */
/* ------------------------------------------------------------------ */
#define UART_BASEADDR       XPAR_MINSOC_AXI_UARTLITE_0_BASEADDR
#define SPI_DEVICE_ID       XPAR_SPI_1_DEVICE_ID
#define INTC_DEVICE_ID      XPAR_INTC_0_DEVICE_ID
#define SPI_IRPT_INTR       XPAR_INTC_0_SPI_1_VEC_ID
#define FRAME_SIZE          3

/* ------------------------------------------------------------------ */
/* 全局变量                                                            */
/* ------------------------------------------------------------------ */
static XSpi  SpiInstance;
static XIntc IntcInstance;

volatile int TransferInProgress = FALSE;
int Error = 0;

u8 TxBuffer[FRAME_SIZE];
u8 RxBuffer[FRAME_SIZE];

/* ------------------------------------------------------------------ */
/* 函数声明                                                            */
/* ------------------------------------------------------------------ */
static int  SpiSetupIntrSystem(XIntc *IntcInstancePtr,
                               XSpi  *SpiInstancePtr,
                               u16    SpiIntrId);
void SpiIntrHandler(void *CallBackRef, u32 StatusEvent);

/* ------------------------------------------------------------------ */
/* UART 辅助                                                           */
/* ------------------------------------------------------------------ */
u8 uart_recv_byte(void)
{
    while (XUartLite_IsReceiveEmpty(UART_BASEADDR));
    return XUartLite_RecvByte(UART_BASEADDR);
}

void uart_send_byte(u8 data)
{
    XUartLite_SendByte(UART_BASEADDR, data);
}

/* ------------------------------------------------------------------ */
/* SPI 中断回调                                                        */
/* ------------------------------------------------------------------ */
void SpiIntrHandler(void *CallBackRef, u32 StatusEvent)
{
    TransferInProgress = FALSE;
    if (StatusEvent != XST_SPI_TRANSFER_DONE) {
        Error++;
        xil_printf("SPI event error: %d\r\n", StatusEvent);
    }
}

/* ------------------------------------------------------------------ */
/* 中断系统初始化（严格照官方示例）                                    */
/* ------------------------------------------------------------------ */
static int SpiSetupIntrSystem(XIntc *IntcInstancePtr,
                              XSpi  *SpiInstancePtr,
                              u16    SpiIntrId)
{
    int Status;

    Status = XIntc_Initialize(IntcInstancePtr, INTC_DEVICE_ID);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    Status = XIntc_Connect(IntcInstancePtr, SpiIntrId,
                           (XInterruptHandler)XSpi_InterruptHandler,
                           (void *)SpiInstancePtr);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    Status = XIntc_Start(IntcInstancePtr, XIN_REAL_MODE);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    XIntc_Enable(IntcInstancePtr, SpiIntrId);

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                 (Xil_ExceptionHandler)XIntc_InterruptHandler,
                                 (void *)IntcInstancePtr);
    Xil_ExceptionEnable();

    return XST_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    int Status;
    XSpi_Config *ConfigPtr;

    xil_printf("\r\n===== SPI Bridge (Interrupt Mode) =====\r\n");

    /* 1. LookupConfig + CfgInitialize（官方示例的做法） */
    ConfigPtr = XSpi_LookupConfig(SPI_DEVICE_ID);
    if (ConfigPtr == NULL) {
        xil_printf("SPI LookupConfig failed\r\n");
        return XST_FAILURE;
    }

    Status = XSpi_CfgInitialize(&SpiInstance, ConfigPtr,
                                ConfigPtr->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI CfgInitialize failed\r\n");
        return XST_FAILURE;
    }

    /* 2. 自检 */
    Status = XSpi_SelfTest(&SpiInstance);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI SelfTest failed\r\n");
        return XST_FAILURE;
    }

    /* 3. 中断系统 */
    Status = SpiSetupIntrSystem(&IntcInstance, &SpiInstance, SPI_IRPT_INTR);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI setup intc failed\r\n");
        return XST_FAILURE;
    }

    /* 4. 注册回调（必须在 Start 之前） */
    XSpi_SetStatusHandler(&SpiInstance, &SpiInstance,
                          (XSpi_StatusHandler)SpiIntrHandler);

    /* 5. 主机模式，自动片选 */
    Status = XSpi_SetOptions(&SpiInstance, XSP_MASTER_OPTION);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI SetOptions failed\r\n");
        return XST_FAILURE;
    }

    /* 6. 最后才 Start —— 内部会自动使能全局中断 */
    Status = XSpi_Start(&SpiInstance);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI Start failed\r\n");
        return XST_FAILURE;
    }

    /* 7. ★ 指定从设备（即使自动片选也必须设，否则报 XST_SPI_NO_SLAVE=1155） */
    XSpi_SetSlaveSelect(&SpiInstance, 0x01);   /* 0x01 = 选中 SS0 */

    xil_printf("SPI mode = %d (1 = INTR)\r\n", SpiInstance.SpiMode);
    xil_printf("Send 3 bytes (CMD1 CMD2 DATA) to trigger SPI transfer.\r\n");

    /* ---------------- 主循环 ---------------- */
    while (1) {
        int i;

        for (i = 0; i < FRAME_SIZE; i++) {
            TxBuffer[i] = uart_recv_byte();
        }
        xil_printf("TX: %02X %02X %02X\r\n",
                   TxBuffer[0], TxBuffer[1], TxBuffer[2]);

        TransferInProgress = TRUE;
        Status = XSpi_Transfer(&SpiInstance, TxBuffer, RxBuffer, FRAME_SIZE);
        if (Status != XST_SUCCESS) {
            xil_printf("SPI transfer start failed! Status=%d IsBusy=%d\r\n",
                       Status, SpiInstance.IsBusy);
            TransferInProgress = FALSE;
            continue;
        }

        while (TransferInProgress);

        xil_printf("RX: %02X %02X %02X  (Error=%d)\r\n",
                   RxBuffer[0], RxBuffer[1], RxBuffer[2], Error);
    }

    return XST_SUCCESS;
}

