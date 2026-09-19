# 注意：需用 vsim -voptargs=+acc 加载设计，否则内部信号不可见
# 启动命令：vsim -voptargs=+acc -do wave.do tb_ds18b20_dri

# 添加测试平台所有信号
add wave -group TB sim:/tb_ds18b20_dri/clk
add wave -group TB sim:/tb_ds18b20_dri/rst_n
add wave -group TB sim:/tb_ds18b20_dri/dq
add wave -group TB sim:/tb_ds18b20_dri/temp_data
add wave -group TB sim:/tb_ds18b20_dri/sign
add wave -group TB sim:/tb_ds18b20_dri/dq_slave_low

# 添加 DUT 内部关键信号
add wave -group DUT sim:/tb_ds18b20_dri/dut/cur_state
add wave -group DUT sim:/tb_ds18b20_dri/dut/next_state
add wave -group DUT sim:/tb_ds18b20_dri/dut/dq_out
add wave -group DUT sim:/tb_ds18b20_dri/dut/clk_1us
add wave -group DUT sim:/tb_ds18b20_dri/dut/cnt_1us
add wave -group DUT sim:/tb_ds18b20_dri/dut/wr_data
add wave -group DUT sim:/tb_ds18b20_dri/dut/wr_cnt
add wave -group DUT sim:/tb_ds18b20_dri/dut/rd_data
add wave -group DUT sim:/tb_ds18b20_dri/dut/rd_cnt
add wave -group DUT sim:/tb_ds18b20_dri/dut/cmd_cnt
add wave -group DUT sim:/tb_ds18b20_dri/dut/flow_cnt
add wave -group DUT sim:/tb_ds18b20_dri/dut/st_done
add wave -group DUT sim:/tb_ds18b20_dri/dut/init_done

# 运行完整仿真
run -all

# 缩放到全范围
wave zoom full