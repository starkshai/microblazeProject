import threading
import serial
import time

# 全局串口对象
ser = None
running = True


def uart_reader_thread():
    """后台线程：持续读串口并打印"""
    global ser, running
    while running:
        try:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting)
                text = data.decode('ascii', errors='ignore')
                # 直接打印，前面加个标记，方便区分
                print(text, end='', flush=True)
            else:
                time.sleep(0.02)
        except Exception as e:
            print(f"\n[串口读取错误] {e}")
            break


def parse_hex_input(s):
    s = s.replace("0x", "").replace("0X", "")
    s = s.replace(",", " ").replace(";", " ")
    if " " not in s and len(s) % 2 == 0 and len(s) > 0:
        tokens = [s[i:i+2] for i in range(0, len(s), 2)]
    else:
        tokens = s.split()
    try:
        data = [int(t, 16) & 0xFF for t in tokens if t]
        return data
    except ValueError:
        return None


def main():
    global ser, running

    # 打开串口
    ser = serial.Serial('COM4', 115200, timeout=0.1)
    time.sleep(0.2)

    # 启动后台读取线程
    t = threading.Thread(target=uart_reader_thread, daemon=True)
    t.start()

    print("===== SPI Bridge Debug Console =====")
    print("输入 3 字节 HEX 数据，如：80 05 00")
    print("命令：q 退出 | d dump | r N 读 N 字节")
    print("=" * 40)

    try:
        while True:
            line = input(">> ").strip()
            if not line:
                continue
            if line.lower() in ('q', 'exit', 'quit'):
                break
            # 解析 HEX
            tx_bytes = parse_hex_input(line)
            if tx_bytes is None:
                print("HEX 格式无效")
                continue
            ser.write(bytes(tx_bytes))
            ser.flush()
            hex_str = " ".join(f"{b:02X}" for b in tx_bytes)
            print(f"[发送] {hex_str}")
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        running = False
        time.sleep(0.1)
        ser.close()
        print("退出。")


if __name__ == "__main__":
    main()