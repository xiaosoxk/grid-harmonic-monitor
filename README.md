# 基于STM32F407与嵌入式Linux的电网谐波监测系统

三层架构：感知层(ZMCT103C前端) — 边缘层(STM32F407 软件锁相同步采样 + CMSIS-DSP FFT) — 网关层(Linux 上位机/Modbus-TCP/MQTT)。

## 目录
```
stm32/           F407 固件(HAL + CMSIS-DSP)
  llc?? no
```
详见仓库内各模块注释与配套设计文档。

## 模块速览
- stm32/adc_dma.c    TIM3触发ADC双通道扫描 + DMA循环双缓冲(零丢点)
- stm32/sync_pll.c   软件锁相同步采样(fs=512×f0), 抛物线插值测频
- stm32/pq_math.c    加窗FFT + 1~50次谐波 + THD/RMS/P/Q/S/PF
- stm32/uart_frame.c AA55|TYPE|LEN|SEQ|PAYLOAD|CRC16 帧协议
- stm32/ui/          LVGL 六页面触摸HMI(TFT) / OLED三页轮播
- stm32/ai/          谐波指纹负载识别(随机森林, 训练脚本在 host/ai/)
- host/protocol.py   解析状态机(抗粘包/假帧头恢复)
- host/analyzer.py   NumPy 同构算法交叉验证
- host/host_app.py   PyQt5+pyqtgraph 四面板上位机
- host/gateway.py    Modbus-TCP + MQTT 工业网关
- host/sim_frame_sender.py  无硬件仿真器(pty)
- host/ai/           边缘AI负载识别(训练/导出/采集脚本)

## 快速开始
见 host/README.md 与仓库文档《保姆级实施手册》。

## 安全提醒
前端涉及电网信号：调试必须经互感器隔离，弱电侧先于强电上电。
