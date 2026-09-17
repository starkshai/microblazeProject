#include "xparameters.h"
#include "xspi.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xuartlite.h"
#include "xuartlite_l.h"   /* 低层 API：XUartLite_IsReceiveEmpty / RecvByte / SendByte */

/* ------------------------------------------------------------------ */
/* 参数                                                                */
/* ------------------------------------------------------------------ */
#define UART_DEVICE_ID      XPAR_MINSOC_AXI_UARTLITE_0_DEVICE_ID
#define SPI_DEVICE_ID       XPAR_SPI_1_DEVICE_ID
#define INTC_DEVICE_ID      XPAR_INTC_0_DEVICE_ID
#define UART_IRPT_INTR      XPAR_INTC_0_UARTLITE_0_VEC_ID
#define SPI_IRPT_INTR       XPAR_INTC_0_SPI_1_VEC_ID

#define FRAME_SIZE          3
#define RX_RING_SIZE        64     /* 环形缓冲区大小，必须是 2 的幂便于将来优化 */

/* ------------------------------------------------------------------ */
/* 全局实例                                                            */
/* ------------------------------------------------------------------ */
static XSpi      SpiInstance;
static XIntc     IntcInstance;
static XUartLite UartInstance;

volatile int TransferInProgress = FALSE;
volatile int Error              = 0;

/* UART 接收环形缓冲区（中断写入，主循环读取） */
static volatile u8  RxRing[RX_RING_SIZE];
static volatile int RxHead = 0;
static volatile int RxTail = 0;

u8 TxBuffer[FRAME_SIZE];
u8 RxBuffer[FRAME_SIZE];

/* ------------------------------------------------------------------ */
/* 函数声明                                                            */
/* ------------------------------------------------------------------ */
static int  SetupIntrSystem(XIntc *IntcInstancePtr,
                            XSpi  *SpiInstancePtr,
                            XUartLite *UartInstancePtr,
                            u16 SpiIntrId, u16 UartIntrId);
void        SpiIntrHandler(void *CallBackRef, u32 StatusEvent);
void        UartRecvHandler(void *CallBackRef, unsigned int EventData);

/* ------------------------------------------------------------------ */
/* UART 接收中断回调                                                   */
/* ------------------------------------------------------------------ */
void UartRecvHandler(void *CallBackRef, unsigned int EventData)
{
    XUartLite *UartPtr = (XUartLite *)CallBackRef;
    u8  byte;
    int next;

    (void)EventData;

    /* 把 FIFO 中所有可用字节全部搬进环形缓冲区 */
    while (!XUartLite_IsReceiveEmpty(UartPtr->RegBaseAddress)) {
        byte = XUartLite_RecvByte(UartPtr->RegBaseAddress);
        next = (RxHead + 1) % RX_RING_SIZE;
        if (next != RxTail) {
            RxRing[RxHead] = byte;
            RxHead = next;
        }
        /* next == RxTail 表示缓冲区满，丢弃该字节 */
    }
}

/* 从缓冲区取 1 字节，成功返回 1 */
static int uart_try_get_byte(u8 *out)
{
    if (RxHead == RxTail) return 0;
    *out = RxRing[RxTail];
    RxTail = (RxTail + 1) % RX_RING_SIZE;
    return 1;
}

/* 查询当前可用字节数 */
static int uart_available(void)
{
    return (RxHead - RxTail + RX_RING_SIZE) % RX_RING_SIZE;
}

/* ------------------------------------------------------------------ */
/* SPI 中断回调                                                        */
/* ------------------------------------------------------------------ */
void SpiIntrHandler(void *CallBackRef, u32 StatusEvent)
{
    (void)CallBackRef;
    TransferInProgress = FALSE;
    if (StatusEvent != XST_SPI_TRANSFER_DONE) {
        Error++;
        xil_printf("SPI event error: %d\r\n", StatusEvent);
    }
}

/* ------------------------------------------------------------------ */
/* 中断控制器初始化（同时连接 SPI 和 UART）                            */
/* ------------------------------------------------------------------ */
static int SetupIntrSystem(XIntc *IntcInstancePtr,
                           XSpi  *SpiInstancePtr,
                           XUartLite *UartInstancePtr,
                           u16 SpiIntrId, u16 UartIntrId)
{
    int Status;

    Status = XIntc_Initialize(IntcInstancePtr, INTC_DEVICE_ID);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    /* SPI 中断 */
    Status = XIntc_Connect(IntcInstancePtr, SpiIntrId,
                           (XInterruptHandler)XSpi_InterruptHandler,
                           (void *)SpiInstancePtr);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    /* UART 中断 */
    Status = XIntc_Connect(IntcInstancePtr, UartIntrId,
                           (XInterruptHandler)XUartLite_InterruptHandler,
                           (void *)UartInstancePtr);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    Status = XIntc_Start(IntcInstancePtr, XIN_REAL_MODE);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    XIntc_Enable(IntcInstancePtr, SpiIntrId);
    XIntc_Enable(IntcInstancePtr, UartIntrId);

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
    int Status, i;
    XSpi_Config *SpiCfg;

    xil_printf("\r\n===== SPI Bridge (UART IRQ + SPI IRQ) =====\r\n");

    /* ---------- UART 初始化 ---------- */
    Status = XUartLite_Initialize(&UartInstance, UART_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        xil_printf("UART Initialize failed\r\n");
        return XST_FAILURE;
    }

    /* 注册接收回调 + 打开 UART 自身的中断位 */
    XUartLite_SetRecvHandler(&UartInstance, UartRecvHandler, &UartInstance);
    XUartLite_EnableInterrupt(&UartInstance);

    /* ---------- SPI 初始化 ---------- */
    SpiCfg = XSpi_LookupConfig(SPI_DEVICE_ID);
    if (SpiCfg == NULL) {
        xil_printf("SPI LookupConfig failed\r\n");
        return XST_FAILURE;
    }
    Status = XSpi_CfgInitialize(&SpiInstance, SpiCfg, SpiCfg->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI CfgInitialize failed\r\n");
        return XST_FAILURE;
    }
    Status = XSpi_SelfTest(&SpiInstance);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI SelfTest failed\r\n");
        return XST_FAILURE;
    }

    /* ---------- 中断控制器 ---------- */
    Status = SetupIntrSystem(&IntcInstance, &SpiInstance, &UartInstance,
                             SPI_IRPT_INTR, UART_IRPT_INTR);
    if (Status != XST_SUCCESS) {
        xil_printf("Intc setup failed\r\n");
        return XST_FAILURE;
    }

    /* ---------- SPI 回调 + 配置 + 启动 ---------- */
    XSpi_SetStatusHandler(&SpiInstance, &SpiInstance,
                          (XSpi_StatusHandler)SpiIntrHandler);

    Status = XSpi_SetOptions(&SpiInstance, XSP_MASTER_OPTION);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI SetOptions failed\r\n");
        return XST_FAILURE;
    }

    Status = XSpi_Start(&SpiInstance);
    if (Status != XST_SUCCESS) {
        xil_printf("SPI Start failed\r\n");
        return XST_FAILURE;
    }

    XSpi_SetSlaveSelect(&SpiInstance, 0x01);

    xil_printf("Ready. SpiMode=%d, SlaveSelect=0x%08X\r\n",
               SpiInstance.SpiMode, SpiInstance.SlaveSelectReg);
    xil_printf("Send 3 bytes (CMD1 CMD2 DATA).\r\n\r\n");

    /* ---------- 主循环 ---------- */
    while (1) {

        /* 缓冲区不足 3 字节时先干等；UART 中断会不断填充 */
        if (uart_available() < FRAME_SIZE) {
            continue;
        }

        /* 从环形缓冲区取 3 字节 */
        for (i = 0; i < FRAME_SIZE; i++) {
            (void)uart_try_get_byte(&TxBuffer[i]);
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

        xil_printf("RX: %02X %02X %02X  (Error=%d)\r\n\r\n",
                   RxBuffer[0], RxBuffer[1], RxBuffer[2], Error);
    }

    return XST_SUCCESS;
}

