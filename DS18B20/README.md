# DS18B20 驱动项目说明

**使用ds18b20v1.v** - 2026.9.25

## 项目简介

本项目基于 Verilog 实现了一个适用于 **DS18B20 数字温度传感器**的 1-Wire 总线驱动模块，并配套了完整的 ModelSim 仿真测试平台。驱动模块默认面向 **100MHz 系统时钟**，支持通过参数 `CLKFREQ` 适配其他时钟频率，适用于 **外部供电、单设备** 的应用场景。

项目包含驱动源码、测试平台、ModelSim 波形脚本以及详细的仿真修改记录，可直接用于 FPGA 工程或作为 1-Wire 协议学习的参考。

## 功能特性

- 支持 DS18B20 标准 1-Wire 协议：复位、存在脉冲、写时隙、读时隙。
- 自动完成温度转换与读取：发送 `Skip ROM (0xCC)` → `Convert T (0x44)` → 延时 → `Skip ROM` → `Read Scratchpad (0xBE)` → 读取 16 位温度数据。
- 输出原始 16 位二进制补码温度值 `temp_data[15:0]` 和符号位 `sign`。
- 读数据采用 **LSB 优先** 接收，符合 DS18B20 协议。
- 参数化时钟频率：`parameter CLKFREQ`，默认 `100_000_000`（100MHz），可自动计算 1MHz 内部时钟分频值。
- 包含完整的 ModelSim 仿真测试平台，模拟从机响应，验证读写时序。
- 提供 `wave.do` 波形脚本，一键加载关键信号并运行仿真。

## 文件说明

| 文件名 | 说明 |
|--------|------|
| `ds18b20z.v` | DS18B20 驱动模块（DUT），实现 1-Wire 通信与温度读取。 |
| `ds18b20_tb.v` | 测试平台（Testbench），模拟 DS18B20 从机行为，验证驱动功能。 |
| `wave.do` | ModelSim 波形脚本，添加测试平台与 DUT 内部关键信号。 |
| `ds18b20仿真修改记录.txt` | 仿真过程中遇到的问题及修改记录，包含常见报错与解决方法。 |

## 驱动模块接口

```verilog
module ds18b20_dri #(
    parameter CLKFREQ = 100_000_000     // 输入时钟频率，单位 Hz
)(
    input              clk        ,     // 时钟信号
    input              rst_n      ,     // 复位信号（低有效）
    inout              dq         ,     // DS18B20 的 DQ 引脚
    output reg [15:0]  temp_data  ,     // 原始温度数据（16 位二进制补码）
    output reg         sign             // 符号位（temp_data[15]）
);
```

- `CLKFREQ`：输入时钟频率，默认 100MHz。若实际时钟为 50MHz 或其他频率，例化时覆盖该参数即可。
- `temp_data`：读取到的原始温度数据，正温度直接为二进制补码，负温度为补码形式。实际温度 = `temp_data * 0.0625` °C。
- `sign`：符号位，`0` 表示正温，`1` 表示负温。

## 仿真方法

### 1. 命令行快速仿真（无 GUI）

```bash
vlog ds18b20z.v ds18b20_tb.v
vsim -c -do "run -all; quit -f" tb_ds18b20_dri
```

仿真结束后，Transcript 窗口会打印：

```
Simulation finished. temp_data = 0x0191, sign = 0
```

表示读取到 +25.0625°C。

### 2. 打开 GUI 波形界面

```bash
vlog ds18b20z.v ds18b20_tb.v
vsim -voptargs=+acc -do wave.do tb_ds18b20_dri
```

> **注意**：必须加 `-voptargs=+acc`，否则 ModelSim 优化器会隐藏内部信号，导致 `add wave` 找不到对象。

### 3. 在已打开的 ModelSim 中重新加载

```tcl
quit -sim
vlog ds18b20z.v ds18b20_tb.v
vsim -voptargs=+acc tb_ds18b20_dri
do wave.do
```

若修改了源码需要重跑：

```tcl
restart -force
do wave.do
```

## 波形观察重点

`wave.do` 会自动添加以下信号：

**测试平台（TB）组：**
- `clk`：100MHz 系统时钟，周期 10ns
- `rst_n`：复位信号，低有效
- `dq`：1-Wire 总线
- `temp_data`：最终读出的 16 位温度原始值
- `sign`：温度符号位
- `dq_slave_low`：从机模型是否拉低 DQ

**DUT 内部组：**
- `cur_state` / `next_state`：状态机
  - 1=init，2=rom_skip，3=wr_byte，4=temp_convert，5=delay，6=rd_temp，7=rd_byte
- `dq_out`：主机驱动 DQ 的值
- `clk_1us`：1MHz 微秒时钟
- `cnt_1us`：微秒计数器
- `wr_data`：正在发送的命令字节（CC/44/BE）
- `wr_cnt`：写位计数（0~7）
- `rd_data`：正在接收的温度数据
- `rd_cnt`：读位计数（0~15）
- `cmd_cnt`：命令序号
- `flow_cnt`：子状态流转计数
- `st_done`：当前子操作完成标志
- `init_done`：初始化完成标志

观察要点：
- 初始化时主机拉低 DQ 约 480µs，释放后从机发出存在脉冲。
- 写字节时 DQ 上出现 8 个写时隙，依次发送 `0xCC`、`0x44`、`0xCC`、`0xBE`。
- 读时隙时主机拉低约 1µs 后释放，从机驱动 DQ，主机在约 15µs 处采样。
- 最终 `temp_data` 应为 `0x0191`，`sign` 为 `0`。

## 仿真修改记录摘要

在 ModelSim SE-64 10.4 仿真过程中，对测试平台和 DUT 进行了以下关键修正：

1. **`receive_byte` 端口方向错误**  
   原 `output reg [7:0] data` 改为 `input [7:0] expected`，因为调用时传入的是字面常量，不能作为 output 实参。

2. **时间单位字面量导致 vopt 优化失败**  
   `#30us`、`#120us` 等改为 `#30000`、`#120000`（单位 ns），避免 ModelSim 将 `us` 误解析为层次名。

3. **`send_temp` 读时隙时序错位**  
   去掉末尾多余的 `#40000` 等待，仅保持 `#30000`，由循环顶部的 `@(negedge dq)` 自动同步下一时隙。

4. **读移位方向错误**  
   DUT 中 `rd_data <= {rd_data[14:0], dq}` 改为 `rd_data <= {dq, rd_data[15:1]}`，使最先收到的 LSB 最终落在最低位，符合 DS18B20 的 LSB 优先协议。

详细修改记录见 `ds18b20仿真修改记录.txt`。

## 常见报错及解决

| 报错 | 原因 | 解决方法 |
|------|------|----------|
| `Illegal output or inout port connection` | 把字面量传给了 `output` 端口 | 将 `receive_byte` 的参数改为 `input` |
| `Failed to find 'us' in hierarchical name` | ModelSim 10.4 vopt 不识别 `us` 字面量 | 将 `#30us` 等换算为 `#30000`（ns） |
| `No objects found matching '/tb_ds18b20_dri/clk'` | 加载设计时未加 `-voptargs=+acc`，信号被优化隐藏 | 使用 `vsim -voptargs=+acc` 重新加载 |
| `No help for wave deletecursor available` | ModelSim 10.4 的 `wave delete` 命令异常 | 删除该命令行，新建仿真时波形窗口本就为空 |

## 使用示例

在 FPGA 工程中例化驱动模块：

```verilog
ds18b20_dri #(
    .CLKFREQ(50_000_000)   // 若系统时钟为 50MHz
) u_ds18b20 (
    .clk        (clk),
    .rst_n      (rst_n),
    .dq         (dq_pin),
    .temp_data  (temp_raw),
    .sign       (temp_sign)
);
```

读取到的 `temp_raw` 为 16 位补码，实际温度计算公式：

```verilog
// 正温度：temp_raw * 0.0625
// 负温度：先取补码再乘 0.0625
```

## 许可证

本项目仅供学习与参考，可自由用于非商业用途。使用前请根据实际硬件条件验证时序与电气特性。

---

如有问题或建议，欢迎提交 Issue 或 Pull Request。
