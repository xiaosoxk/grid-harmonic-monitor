# 基于STM32F407 + Linux 的电网谐波监测系统 — 代码索引

## 目录结构
```
code/
  stm32/            STM32F407 固件(CMSIS-DSP, HAL)
    pq_config.h       全局参数(采样率/FFT点数/谐波次数)
    adc_dma.c/.h      TIM3触发ADC双通道扫描 + DMA循环双缓冲
    sync_pll.c/.h     软件锁相同步采样(频谱反馈式PLL)
    pq_math.c/.h      加窗FFT + 谐波/THD/RMS/功率因数(CMSIS-DSP)
    uart_frame.c/.h   成帧 + CRC16-MODBUS + DMA发送
    main.c            超级循环调度 + CPU占用率测量
  host/             Linux(Ubuntu/WSL2) 上位机与网关
    protocol.py       帧协议解析(状态机, CRC校验, 断线恢复)
    analyzer.py       NumPy 参考分析器(与固件算法同构, 交叉验证)
    host_app.py       PyQt5+pyqtgraph 实时上位机(4面板+CSV存档)
    gateway.py        Modbus-TCP + MQTT 工业网关
    sim_frame_sender.py  无硬件仿真器(pty 虚拟串口)
```

## 快速开始
```bash
# 1) 无硬件全链路联调(先跑通软件)
cd code/host
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python3 sim_frame_sender.py            # 窗口1: 记下 /dev/pts/N
python3 host_app.py /dev/pts/N         # 窗口2: 打开上位机

# 2) 接硬件
python3 host_app.py /dev/ttyUSB0       # 或 Windows: python host_app.py COM5

# 3) 网关模式(与上位机二选一, 串口互斥)
python3 gateway.py /dev/ttyUSB0 --modbus 0.0.0.0:5020 --mqtt mqtt://192.168.1.10
```

## STM32 固件编译
用 STM32CubeMX 生成 F407VET6 空工程(168MHz, 仅需启用 USART2),
把 `code/stm32/*.c/h` 加入工程, 链接 CMSIS-DSP 库(arm_rfft_fast_f32 等),
Keil AC6 或 CMake+arm-none-eabi-gcc 均可。中文注释为 UTF-8 编码,
Keil 中请设置 Edit -> Configuration -> Encoding 为 UTF-8。

## 自检
```bash
python3 - <<'EOF'
from protocol import crc16_modbus, FrameParser
from struct import pack
p = pack('<3f', 50.0, 1.0, 2.0)
frame = bytes([0xAA,0x55,0x02]) + pack('<H', len(p)) + bytes([0]) + p
frame += pack('<H', crc16_modbus(frame[2:]))
out = []
fp = FrameParser(on_result=lambda r: out.append(r))
for b in frame:                        # 逐字节喂入, 验证粘包/断帧恢复
    fp.feed(bytes([b]))
print('parsed frames:', len(out), 'freq =', out[0].freq)
EOF
```
