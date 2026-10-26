# -*- coding: utf-8 -*-
"""
gen_synthetic_dataset.py — 谐波指纹负载识别：合成数据集生成器
用途: 硬件/采集未就绪时先跑通"采集->训练->导出C->部署"全流程。
每类负载的谐波"指纹"参数来自典型文献量级(答辩注明为仿真数据, 实测集后替换)。
输出: data/ai_dataset/<CLASS>_<session>.csv, 每行: h1,h2..h25,thdi,pf
"""
import sys, os
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "ai_dataset")
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(42)

# 类别 -> (各次谐波相对基波幅值典型值, PF典型值, 波动)
CLASSES = {
    #                 h2    h3    h5    h7    h9   h11   h13   h15   其余≈0
    "RESISTIVE":  ({3: 0.005, 5: 0.004},                    0.999, 0.001),  # 白炽灯/电热
    "LED_LAMP":   ({3: 0.28, 5: 0.15, 7: 0.09, 9: 0.06, 11: 0.05, 13: 0.04, 15: 0.03}, 0.45, 0.10),
    "CHARGER":    ({3: 0.55, 5: 0.32, 7: 0.12, 9: 0.05},    0.55, 0.08),   # 手机充电器
    "LAPTOP_PSU": ({3: 0.40, 5: 0.45, 7: 0.22, 11: 0.08, 13: 0.05}, 0.62, 0.06),
    "FAN_DIMMER": ({3: 0.18, 5: 0.35, 7: 0.20, 9: 0.10, 11: 0.06}, 0.80, 0.09),
    "INVERTER_AC":({5: 0.30, 7: 0.25, 11: 0.12, 13: 0.08},  0.92, 0.05),   # 变频空调(5/7/11次)
}

SESSIONS_PER_CLASS = 4     # 每类4个采集会话(按会话划分训练/测试, 防泄漏)
WINDOWS_PER_SESSION = 120  # 每会话120窗

for cls, (harm, pf0, pf_jit) in CLASSES.items():
    for ses in range(SESSIONS_PER_CLASS):
        rows = []
        base_jit = {h: 1 + rng.normal(0, 0.06) for h in harm}   # 会话级器件偏差
        for _ in range(WINDOWS_PER_SESSION):
            h = np.zeros(26)                                     # h[1..25]
            h[1] = 1.0
            for k, rel in harm.items():
                h[k] = max(0.0, rel * base_jit[k] * (1 + rng.normal(0, 0.04)))
            thdi = float(np.sqrt(np.sum(h[2:] ** 2)) * 100.0)
            pf = float(np.clip(pf0 + rng.normal(0, pf_jit / 3), 0.1, 1.0))
            rows.append(np.concatenate([h[1:26], [thdi, pf]]))
        name = "%s_s%d.csv" % (cls, ses)
        np.savetxt(os.path.join(OUT, name), np.array(rows), delimiter=",",
                   header="h1," + ",".join("h%d" % i for i in range(2, 26)) + ",thdi,pf",
                   comments="", fmt="%.6f")
print("synthetic dataset ->", OUT)
print("classes:", len(CLASSES), " sessions:", SESSIONS_PER_CLASS,
      " windows/session:", WINDOWS_PER_SESSION)
