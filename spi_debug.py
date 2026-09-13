#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MicroBlaze SPI Bridge 串口调试工具
用途：向 FPGA 发送 3 字节 HEX 命令 (CMD1 CMD2 DATA)，触发一次 SPI 传输，
      并解析串口返回的 TX/RX 数据。
"""

import serial
import serial.tools.list_ports
import time
import sys
import re


# ============================================================
# 配置区
# ============================================================
BAUDRATE = 115200
TIMEOUT = 1.0          # 串口读取超时（秒）
WAIT_AFTER_SEND = 0.3  # 发送后等待 FPGA 处理的时间（秒）


# ============================================================
# 列出可用串口
# ============================================================
def list_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("[错误] 未检测到任何串口设备！")
        return []
    print("可用串口列表：")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device}  -  {p.description}")
    return ports


def select_port(ports):
    while True:
        try:
            idx = input("请选择串口号（输入序号）: ").strip()
            idx = int(idx)
            if 0 <= idx < len(ports):
                return ports[idx].device
            else:
                print("序号超出范围，请重新输入。")
        except (ValueError, KeyboardInterrupt):
            print("输入无效，请重新输入。")


# ============================================================
# 解析 HEX 输入
# ============================================================
def parse_hex_input(s):
    """
    支持多种输入格式：
      "80 05 00"
      "80,05,00"
      "800500"
      "0x80 0x05 0x00"
    返回字节列表 [0x80, 0x05, 0x00]，出错返回 None
    """
    # 去除 0x 前缀，统一分隔符
    s = s.replace("0x", "").replace("0X", "")
    s = s.replace(",", " ").replace(";", " ")

    # 如果无空格且长度为偶数，按每 2 字符切分
    if " " not in s and len(s) % 2 == 0 and len(s) > 0:
        tokens = [s[i:i+2] for i in range(0, len(s), 2)]
    else:
        tokens = s.split()

    try:
        data = [int(t, 16) & 0xFF for t in tokens if t]
        return data
    except ValueError:
        return None


# ============================================================
# 解析 FPGA 返回的文本
# ============================================================
def parse_response(text):
    """
    从 FPGA 返回的文本中提取 TX 和 RX 数据。
    例如：
        TX: 80 05 00
        RX: 80 05 5A
    返回 (tx_list, rx_list)，未匹配到则返回 (None, None)
    """
    tx_match = re.search(r"TX:\s*((?:[0-9A-Fa-f]{2}\s*)+)", text)
    rx_match = re.search(r"RX:\s*((?:[0-9A-Fa-f]{2}\s*)+)", text)

    tx = [int(b, 16) for b in tx_match.group(1).split()] if tx_match else None
    rx = [int(b, 16) for b in rx_match.group(1).split()] if rx_match else None
    return tx, rx


# ============================================================
# 主程序
# ============================================================
def main():
    # 1. 选择串口
    ports = list_ports()
    if not ports:
        sys.exit(1)

    port_name = None
    if len(ports) == 1:
        port_name = ports[0].device
        print(f"自动选择唯一串口：{port_name}")
    else:
        port_name = select_port(ports)

    # 2. 打开串口
    try:
        ser = serial.Serial(
            port=port_name,
            baudrate=BAUDRATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=TIMEOUT,
        )
    except serial.SerialException as e:
        print(f"[错误] 无法打开串口 {port_name}: {e}")
        sys.exit(1)

    print(f"\n串口 {port_name} 已打开 @ {BAUDRATE} bps")
    print("=" * 60)
    print("使用说明：")
    print("  - 输入 3 字节 HEX 数据，例如：80 05 00")
    print("  - 支持格式：'80 05 00' / '80,05,00' / '800500' / '0x80 0x05 0x00'")
    print("  - 命令：")
    print("      r <N>    连续读取 N 个字节（不发数据）")
    print("      d        显示从程序启动到现在累积的接收缓冲区")
    print("      q / exit 退出程序")
    print("=" * 60)

    # 3. 清空串口缓冲区
    time.sleep(0.2)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # 4. 显示 FPGA 启动信息
    time.sleep(0.5)
    if ser.in_waiting:
        startup = ser.read(ser.in_waiting).decode("ascii", errors="ignore")
        print(startup, end="")
    print()

    # 5. 主循环
    while True:
        try:
            line = input(">> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n退出。")
            break

        if not line:
            continue

        # 退出
        if line.lower() in ("q", "exit", "quit"):
            print("退出。")
            break

        # 直接显示累积缓冲区
        if line.lower() == "d":
            if ser.in_waiting:
                data = ser.read(ser.in_waiting)
                print("[dump]", data)
            else:
                print("[dump] 缓冲区为空")
            continue

        # 直接读取 N 字节
        if line.lower().startswith("r "):
            try:
                n = int(line[2:].strip())
            except ValueError:
                print("格式错误，应为 'r <N>'")
                continue
            data = ser.read(n)
            print(f"[读取 {n} 字节] {data.hex(' ').upper()}  ({data})")
            continue

        # 解析 HEX 输入
        tx_bytes = parse_hex_input(line)
        if tx_bytes is None:
            print("HEX 格式无效，请重新输入。")
            continue
        if len(tx_bytes) != 3:
            print(f"警告：你输入了 {len(tx_bytes)} 字节，本工程期望 3 字节。")
            print("继续发送剩余字节（FPGA 只会取前 3 个）...")

        # 6. 发送
        ser.reset_input_buffer()
        ser.write(bytes(tx_bytes))
        ser.flush()
        hex_str = " ".join(f"{b:02X}" for b in tx_bytes)
        print(f"[发送] {hex_str}")

        # 7. 等待响应
        time.sleep(WAIT_AFTER_SEND)
        raw = ser.read(ser.in_waiting or 1)
        # 再等一点，确保所有回显到齐
        time.sleep(0.1)
        raw += ser.read(ser.in_waiting)

        text = raw.decode("ascii", errors="ignore")
        print(f"[原始返回]\n{text}")

        # 8. 解析 TX/RX
        tx_echo, rx_echo = parse_response(text)
        if tx_echo is not None:
            print(f"[回显 TX] {' '.join(f'{b:02X}' for b in tx_echo)}")
        if rx_echo is not None:
            print(f"[接收 RX] {' '.join(f'{b:02X}' for b in rx_echo)}")
            if len(rx_echo) >= 3:
                data_byte = rx_echo[2]
                print(f"[数据字节] 0x{data_byte:02X} (0b{data_byte:08b})")
                # 可选：ASCII 显示
                if 32 <= data_byte < 127:
                    print(f"[ASCII]   '{chr(data_byte)}'")
        print("-" * 60)

    ser.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n用户中断。")