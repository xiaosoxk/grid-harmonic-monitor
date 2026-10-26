# -*- coding: utf-8 -*-
"""
sim_frame_sender.py — 无硬件仿真器: 生成与 STM32 固件完全一致的帧流

原理: 在本机创建一对伪终端(pty), 主端按 80ms 周期发送 HELLO/RESULT/WAVE 帧,
     从端是一个虚拟串口设备路径 —— 上位机/网关代码无需任何改动即可联调。
运行: python sim_frame_sender.py
输出: 虚拟串口路径, 例如:  python host_app.py /dev/pts/3
用途: 硬件未到位时先打通上位机全链路(答辩演示利器, 也是CI测试钩子)。
"""
from __future__ import annotations
import os, pty, struct, time, math, random

from protocol import (crc16_modbus, TYPE_HELLO, TYPE_RESULT,
                      TYPE_WAVE, HARM_MAX, RESULT_FLOATS)

FS = 25600.0
N = 2048


def build_frame(ftype: int, payload: bytes) -> bytes:
    seq = build_frame.seq = (getattr(build_frame, "seq", 0) + 1) & 0xFF
    head = bytes([0xAA, 0x55, ftype]) + struct.pack("<H", len(payload)) + bytes([seq])
    crc = crc16_modbus(head[2:] + payload)
    return head + payload + struct.pack("<H", crc)


def make_payloads(t):
    """合成 1 个分析窗口的结果帧与波形帧负载"""
    f0 = 50.0 + 0.05 * math.sin(2 * math.pi * 0.1 * t)     # 频率微波动
    amps = {1: 1.0, 3: 0.18, 5: 0.11, 7: 0.065, 9: 0.03, 11: 0.045, 13: 0.02}
    i1 = 0.85 + 0.02 * math.sin(2 * math.pi * 0.05 * t)    # 基波缓变
    thdi = math.sqrt(sum(v * v for k, v in amps.items() if k >= 2)) / i1 * 100

    floats = [f0, i1 * 0.7071, 220.0 * 0.7071,             # freq, Irms, Urms
              0.7071 * i1 * 220 * 0.92, 0.0,               # P, Q
              0.7071 * i1 * 220, 0.92, 0.95,               # S, PF, DPF
              thdi, 2.1]                                   # THDi, THDu
    ih = [0.0] * HARM_MAX
    for k, a in amps.items():
        ih[k - 1] = i1 * a * (1 + random.uniform(-0.01, 0.01))
    uh = [0.0] * HARM_MAX
    uh[0] = 1.0
    floats += ih + uh

    wave = []
    for n in range(N // 2):                                # 波形帧: 2:1 抽取
        ph = 2 * math.pi * f0 * (n * 2) / FS + t
        v = sum(a * math.sin(2 * math.pi * k * f0 * (n * 2) / FS + t * k)
                for k, a in amps.items())
        wave.append((int(v / max(i1, 1e-6) * 20000), int(0.9 * math.sin(ph) * 20000)))
    payload_wave = struct.pack("<%dh" % (2 * len(wave)),
                               *[x for pt in wave for x in pt])
    return (struct.pack("<%df" % RESULT_FLOATS, *floats), payload_wave)


def main():
    master, slave = pty.openpty()
    print("虚拟串口已创建:", os.ttyname(slave))
    print("运行: python host_app.py", os.ttyname(slave))
    t0 = time.time()
    win = 0
    while True:
        t = time.time() - t0
        p_result, p_wave = make_payloads(t)
        os.write(master, build_frame(TYPE_RESULT, p_result))
        if win % 8 == 0:
            os.write(master, build_frame(TYPE_WAVE, p_wave))
        win += 1
        time.sleep(0.08)                                    # 12.5 窗/秒, 与固件一致


if __name__ == "__main__":
    main()
