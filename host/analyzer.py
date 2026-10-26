# -*- coding: utf-8 -*-
"""
analyzer.py — NumPy 参考分析器（与 STM32 端算法同构, 用于交叉验证）

用途:
  1) 对波形帧(2:1 抽取后的 I/U 序列)独立重算谐波/THD/功率, 与 STM32 的
     RESULT 帧逐项比对 —— "双机一致性校验"是本项目工程化亮点;
  2) 离线分析 CSV 存档数据, 复现论文图表。
注意: 波形帧经 2:1 抽取, 调用时传实际采样率 fs_eff = fs_track / 2。
"""
from __future__ import annotations
import math
import numpy as np

HARM_MAX = 50
NOMINAL_F = 50.0
THD_HARM_MAX = 40


def hann_window(n: int) -> np.ndarray:
    return 0.5 * (1.0 - np.cos(2.0 * np.pi * np.arange(n) / n))


def estimate_frequency(x: np.ndarray, fs: float, f_guess: float = NOMINAL_F) -> float:
    """谱峰搜索 + 抛物线插值测频(与 STM32 sync_pll.c 同构)"""
    spec = np.fft.rfft(x * np.hanning(len(x)))
    mag = np.abs(spec)
    n = len(x)
    b1 = max(2, int(round(f_guess * n / fs)) - 2)
    b2 = min(len(mag) - 2, int(round(f_guess * n / fs)) + 2)
    bp = b1 + int(np.argmax(mag[b1:b2 + 1]))
    ym, y0, yp = mag[bp - 1], mag[bp], mag[bp + 1]
    den = ym - 2.0 * y0 + yp
    delta = 0.5 * (ym - yp) / den if abs(den) > 1e-12 else 0.0
    return (bp + delta) * fs / n


def analyze_window(i: np.ndarray, u: np.ndarray, fs: float) -> dict:
    """对一个完整窗口做全量分析, 返回与 RESULT 帧字段一致的字典"""
    n = len(i)
    i = i - i.mean()
    u = u - u.mean()

    irms = float(np.sqrt(np.mean(i * i)))
    urms = float(np.sqrt(np.mean(u * u)))
    p = float(np.mean(i * u))                       # 有功(时域定义, 含谐波功率)
    s = irms * urms
    pf = p / s if s > 1e-9 else 0.0

    f0 = estimate_frequency(u, fs)
    # 判定窗口是否整周期(同步): k = f0 * n / fs 应接近整数
    sync = abs(f0 * n / fs - round(f0 * n / fs)) < 0.02
    win = np.ones(n) if sync else hann_window(n)    # 同步->矩形窗, 失锁->汉宁窗
    cg = 1.0 if sync else 0.5                       # 窗相干增益

    spec_i = np.fft.rfft(i * win)
    spec_u = np.fft.rfft(u * win)
    bin1 = f0 * n / fs                              # 基波 bin(浮点)
    norm = 2.0 / (n * cg)

    i_harm, u_harm, i_phase = [0.0] * HARM_MAX, [0.0] * HARM_MAX, [0.0] * HARM_MAX
    for h in range(1, HARM_MAX + 1):
        bh = bin1 * h                               # 谐波 bin(可能是浮点 -> 就近取整)
        k = int(round(bh))
        if k >= len(spec_i):
            break
        ai = abs(spec_i[k]) * norm
        au = abs(spec_u[k]) * norm
        # 非整 bin 时按谱线能量占比折减(粗修正), 同步态 k 恰为整数无需修正
        w = 1.0 - abs(bh - k) if sync else 1.0
        i_harm[h - 1] = ai * w
        u_harm[h - 1] = au * w
        i_phase[h - 1] = math.atan2(spec_i[k].imag, spec_i[k].real)

    si = math.sqrt(sum(v * v for v in i_harm[1:THD_HARM_MAX]))
    su = math.sqrt(sum(v * v for v in u_harm[1:THD_HARM_MAX]))
    thdi = si / i_harm[0] * 100 if i_harm[0] > 1e-9 else 0.0
    thdu = su / u_harm[0] * 100 if u_harm[0] > 1e-9 else 0.0

    phi_i1, phi_u1 = i_phase[0], math.atan2(spec_u[int(round(bin1))].imag,
                                            spec_u[int(round(bin1))].real)
    dphi = phi_u1 - phi_i1
    q = u_harm[0] * i_harm[0] * math.sin(dphi)
    return dict(freq=f0, irms=irms, urms=urms, p=p, q=q, s=s, pf=pf,
                dpf=math.cos(dphi), thdi=thdi, thdu=thdu,
                i_harm=i_harm, u_harm=u_harm, locked=sync)


def compare_with_stm32(res_stm32: dict, res_np: dict) -> dict:
    """双机一致性校验: 返回各指标相对偏差(%)"""
    def rel(a, b):
        return abs(a - b) / (abs(b) + 1e-12) * 100.0
    return {k: rel(res_stm32[k], res_np[k])
            for k in ("freq", "irms", "urms", "p", "thdi", "thdu")}
