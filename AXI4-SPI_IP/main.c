#include "xil_io.h"
#include "xparameters.h"
#include "xil_printf.h"

#define SPI_BASE   XPAR_MINSOC_SPI_MASTER_AXI_LITE_0_S00_AXI_BASEADDR

/* 寄存器偏移（与 S00_AXI.v 中的映射一致） */
#define REG_CTRL     0x0   // bit0=start(上升沿触发), bit1=CS有效(1=拉低nCS), bit2=CPOL, bit3=CPHA
#define REG_CLK_DIV  0x4   // [15:0] SPI半周期分频值
#define REG_STATUS   0x8   // bit0=busy, bit1=done, [15:8]=最近接收字节
#define REG_TX_DATA  0xC   // [7:0] 待发送字节

/* CTRL 寄存器位定义 */
#define CTRL_START   0x01
#define CTRL_CS      0x02
#define CTRL_CPOL    0x04
#define CTRL_CPHA    0x08

static inline void spi_write_reg(u32 offset, u32 data) {
    Xil_Out32(SPI_BASE + offset, data);
}

static inline u32 spi_read_reg(u32 offset) {
    return Xil_In32(SPI_BASE + offset);
}

static u8 spi_send_byte(u8 tx, int cs_active)
{
    u32 status;
    u32 timeout;

    /* 先清 start，确保下一次写 1 时一定有上升沿 */
    spi_write_reg(REG_CTRL, cs_active ? CTRL_CS : 0);

    spi_write_reg(REG_TX_DATA, tx);
    spi_write_reg(REG_CTRL, CTRL_START | (cs_active ? CTRL_CS : 0));

    timeout = 100000;
    do {
        status = spi_read_reg(REG_STATUS);
        if (--timeout == 0) break;
    } while (!(status & 0x2));

    spi_write_reg(REG_CTRL, cs_active ? CTRL_CS : 0);

    return (u8)((status >> 8) & 0xFF);
}

/* AD9627 写寄存器：16位指令 + 8位数据，CS 全程有效 */
static void ad9627_write(u8 addr, u8 data)
{
    u16 instr = ((u16)addr) & 0x1FFF;   /* bit15=0(写), bit14:13=00(1字节), 地址 */

    spi_send_byte((u8)(instr >> 8), 1);  /* 指令高字节，CS 保持低 */
    spi_send_byte((u8)(instr & 0xFF), 1);/* 指令低字节，CS 保持低 */
    spi_send_byte(data, 1);              /* 数据字节，CS 保持低 */
    spi_write_reg(REG_CTRL, 0);   /* CS 拉高，start=0，不触发传输 */
}

/* AD9627 读寄存器：16位指令 + 1字节移位时钟，接收数据 */
static u8 ad9627_read(u8 addr)
{
    u16 instr = 0x8000 | (((u16)addr) & 0x1FFF); /* bit15=1(读) */
    u8  rx;

    spi_send_byte((u8)(instr >> 8), 1);  /* 指令高字节 */
    spi_send_byte((u8)(instr & 0xFF), 1);/* 指令低字节 */
    rx = spi_send_byte(0x00, 1);          /* 发空字节，接收返回数据 */
    spi_write_reg(REG_CTRL, 0);   /* CS 拉高，start=0，不触发传输 */
    return rx;
}

static void ad9627_write_sync(u8 addr, u8 data)
{
    ad9627_write(addr, data);
    if (addr >= 0x08 && addr <= 0x18) {
        ad9627_write(0xFF, 0x01);   /* 触发同步 */
    }
}

static void spi_init(void)
{
    /* clk_div=49：假设 S_AXI_ACLK=100MHz，SPI 频率 = 100M/(2*(49+1)) = 1MHz */
    spi_write_reg(REG_CLK_DIV, 49);
    /* CPOL=0, CPHA=0, CS 无效 */
    spi_write_reg(REG_CTRL, 0);
}

int main(void)
{
    spi_init();

    /* 1. 读 Chip ID 验证（AD9627 地址 0x00 默认值 0x18） */
    u8 id = ad9627_read(0x00);
    xil_printf("AD9627 Chip ID = 0x%02X\r\n", id);

    /* 2. 配置 AD9627 寄存器（示例） */
    ad9627_write_sync(0x05, 0x01);   /* 写地址 0x0D，自动触发同步 */

    /* 3. 回读验证 */
    u8 val = ad9627_read(0x05);
    xil_printf("Reg 0x05 = 0x%02X\r\n", val);

    return 0;
}
