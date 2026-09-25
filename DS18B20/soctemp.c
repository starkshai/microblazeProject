#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xil_printf.h"
#include "xstatus.h"

/* 根据 xparameters.h 中的实际名称修改 */
#define GPIO_DEVICE_ID      XPAR_GPIO_0_DEVICE_ID
#define TIMER_DEVICE_ID     XPAR_TMRCTR_0_DEVICE_ID
#define TIMER_CHANNEL       0

/* 定时器输入时钟频率，必须与 Vivado 中 AXI Timer 的时钟一致 */
/* 例如 100MHz 写 100000000，50MHz 写 50000000 */
#define TIMER_FREQ_HZ       50000000U

/* 10 秒计数值 = 时钟频率 × 10 */
#define TIMER_LOAD_VALUE    (TIMER_FREQ_HZ * 10U)

XGpio Gpio;
XTmrCtr Tmr;

/* 读取 GPIO 并解析温度，通过串口发送 */
void read_and_send_temp(void)
{
    u32 temp_raw;
    u16 temperature;
    u8  sign_bit;

    /* 读取 17 位数据：低 16 位温度，第 17 位符号 */
    temp_raw = XGpio_DiscreteRead(&Gpio, 1);

    temperature = temp_raw & 0xFFFF;
    sign_bit    = (temp_raw >> 16) & 0x01;

    /* DS18B20 12 位分辨率，小数部分精度 0.0625°C */
    /* 你的模块已将负数转为原码，所以直接根据 sign_bit 显示正负 */
    if (sign_bit) {
        xil_printf("Temperature: -%d.%04d C\r\n",
                   temperature / 16, (temperature % 16) * 625);
    } else {
        xil_printf("Temperature: +%d.%04d C\r\n",
                   temperature / 16, (temperature % 16) * 625);
    }
}

int main(void)
{
    int Status;

    /* 1. 初始化 GPIO */
    Status = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        xil_printf("GPIO Initialization Failed\r\n");
        return XST_FAILURE;
    }

    /* 设置通道 1 全部为输入，17 位掩码 0x0001FFFF */
    XGpio_SetDataDirection(&Gpio, 1, 0x0001FFFF);

    /* 2. 初始化 AXI Timer */
    Status = XTmrCtr_Initialize(&Tmr, TIMER_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        xil_printf("Timer Initialization Failed\r\n");
        return XST_FAILURE;
    }

    /* 设置定时器：递减计数 + 自动重装，不使能中断 */
    XTmrCtr_SetOptions(&Tmr, TIMER_CHANNEL,
                       XTC_DOWN_COUNT_OPTION | XTC_AUTO_RELOAD_OPTION);

    /* 读取最新稳定温度并发送 */
    read_and_send_temp();

    /* 设置重装值，递减到 0 溢出，实际周期 = (LOAD_VALUE) / 时钟频率 */
    XTmrCtr_SetResetValue(&Tmr, TIMER_CHANNEL, TIMER_LOAD_VALUE - 1);

    /* 启动定时器 */
    XTmrCtr_Start(&Tmr, TIMER_CHANNEL);

    xil_printf("DS18B20 Timer Reader Started...\r\n");
    xil_printf("Will send temperature every 10 seconds.\r\n");

    /* 3. 主循环：轮询定时器溢出标志 */
    while (1) {
        if (XTmrCtr_IsExpired(&Tmr, TIMER_CHANNEL)) {
        	/* 写 1 清除 TINT 中断标志位 */
        	u32 csr = XTmrCtr_ReadReg(Tmr.BaseAddress, TIMER_CHANNEL, XTC_TCSR_OFFSET);
        	XTmrCtr_WriteReg(Tmr.BaseAddress, TIMER_CHANNEL, XTC_TCSR_OFFSET,
        	                         csr | XTC_CSR_INT_OCCURED_MASK);

            /* 读取最新稳定温度并发送 */
            read_and_send_temp();
        }
    }

    return XST_SUCCESS;
}
