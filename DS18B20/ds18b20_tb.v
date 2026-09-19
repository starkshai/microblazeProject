`timescale 1ns / 1ps

module tb_ds18b20_dri;

// 时钟和复位
reg clk;
reg rst_n;

// DQ 总线
wire dq;

// 驱动模块输出
wire [15:0] temp_data;
wire sign;

// 从机控制信号：1 表示从机拉低 dq，0 表示释放
reg dq_slave_low;

// 实例化驱动模块，默认 100MHz
ds18b20_dri #(
    .CLKFREQ(100_000_000)
) dut (
    .clk        (clk),
    .rst_n      (rst_n),
    .dq         (dq),
    .temp_data  (temp_data),
    .sign       (sign)
);

// 上拉电阻，保证总线空闲为高
pullup(dq);

// 从机驱动 DQ（开漏）
assign dq = dq_slave_low ? 1'b0 : 1'bz;

// 时钟生成：100MHz，周期 10ns
initial begin
    clk = 1'b0;
    forever #5 clk = ~clk;
end

// 复位信号
initial begin
    rst_n = 1'b0;
    #100;
    rst_n = 1'b1;
end

// 从机模型
initial begin
    dq_slave_low = 1'b0; // 初始释放总线

    // ---------------- 第一次复位 ----------------
    @(negedge dq); // 等待主机拉低
    @(posedge dq); // 等待主机释放
    #30000;         // 等待 30us
    dq_slave_low = 1'b1; // 发送存在脉冲：拉低
    #120000;
    dq_slave_low = 1'b0; // 释放

    // 接收 Skip ROM (0xCC)
    receive_byte(8'hCC);
    // 接收 Convert T (0x44)
    receive_byte(8'h44);

    // ---------------- 第二次复位 ----------------
    @(negedge dq);
    @(posedge dq);
    #30000;
    dq_slave_low = 1'b1;
    #120000;
    dq_slave_low = 1'b0;

    // 接收 Skip ROM (0xCC)
    receive_byte(8'hCC);
    // 接收 Read Scratchpad (0xBE)
    receive_byte(8'hBE);

    // 发送 16 位温度数据，LSB 先出
    // 温度 +25.0625°C 对应 16'h0191
    send_temp(16'h0191);

    // 仿真结束
    #100000;
    $display("Simulation finished. temp_data = 0x%h, sign = %b", temp_data, sign);
    $stop;
end

// 接收一个字节的任务
task receive_byte;
    input [7:0] expected;
    integer i;
    reg bit_val;
    reg [7:0] data;
    begin
        data = 8'h00;
        for (i = 0; i < 8; i = i + 1) begin
            @(negedge dq);   // 等待主机拉低开始写时隙
            #30000;           // 在采样窗口中心采样
            bit_val = dq;
            data = data | (bit_val << i); // LSB 先入
            wait (dq == 1'b1); // 等待时隙结束（主机释放）
            #5000;            // 恢复时间
        end
        if (data !== expected)
            $display("Error: received 0x%h, expected 0x%h", data, expected);
    end
endtask

// 发送 16 位温度数据的任务
task send_temp;
    input [15:0] temp;
    integer i;
    begin
        for (i = 0; i < 16; i = i + 1) begin
            @(negedge dq); // 等待主机拉低开始读时隙
            // 立即输出当前位
            if (temp[i] == 1'b0)
                dq_slave_low = 1'b1; // 发送 0：拉低
            else
                dq_slave_low = 1'b0; // 发送 1：释放
            #30000; // 保持 30us，主机在 ~15us 采样，裕量充足
            dq_slave_low = 1'b0; // 释放总线
            // 不再固定延时，由循环顶部的 @(negedge dq) 同步下一个时隙
        end
    end
endtask

// 加速仿真：当驱动进入 delay 状态时，强制 cnt_1us 快速达到 800000
initial begin
    wait (dut.cur_state == 3'd5); // 等待进入 delay 状态
    force dut.cnt_1us = 20'd800000;
    @(posedge dut.clk_1us);       // 等待一个 1MHz 时钟上升沿
    release dut.cnt_1us;
end

endmodule