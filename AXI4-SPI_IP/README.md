AXI4-SPI IP 是基于 AXI4-Lite 接口的自定义 SPI 主机控制器（spi_master_axi_lite_v1_0）：
软件通过 AXI 总线读写寄存器，控制四线 SPI 接口（spi_sclk、spi_mosi、spi_miso、spi_cs_n）完成字节级半双工收发，可驱动 AD9627 等 SPI 从机。

HDL 包含两个模块：AXI 从机与寄存器接口模块 spi_master_axi_lite_v1_0_S00_AXI，以及 SPI 字节收发控制器 spi_master。
在 IP 顶层 spi_master_axi_lite_v1_0 加 SPI 四线端口（spi_sclk、spi_mosi、spi_miso、spi_cs_n）；
S00_AXI 从机逻辑将 4 个寄存器（CTRL / CLK_DIV / STATUS / TX_DATA）映射到用户逻辑，并例化 spi_master 完成字节发送。

SDK 侧通过 Xil_In32 / Xil_Out32 直接访问寄存器：按“清 start → 写 TX_DATA → 置 start”的顺序触发一次发送，并轮询 STATUS.done 完成字节级读写；
在此基础上封装了 AD9627 寄存器读写（16 位指令 + 8 位数据，CS 全程有效），写寄存器后按需写同步寄存器 0xFF 触发同步。
