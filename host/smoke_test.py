# -*- coding: utf-8 -*-
"""smoke_test.py — 协议解析 + NumPy 分析器 + v1.1帧 + 补传缓存 功能自检(无硬件可跑)"""
import sys, os, tempfile
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.path.insert(0, '.')
from struct import pack
import numpy as np
from protocol import (crc16_modbus, FrameParser, TYPE_RESULT, TYPE_TIME, TYPE_EVENT,
                      RESULT_FLOATS)


def build(ftype, payload, seq=7):
    head = bytes([0xAA, 0x55, ftype]) + pack('<H', len(payload)) + bytes([seq])
    return head + payload + pack('<H', crc16_modbus(head[2:] + payload))


# --- 1) RESULT 帧解析(任意分段喂入) ---
floats = [50.01, 0.85, 0.62, 120.0, 30.0, 125.0, 0.96, 0.97, 23.4, 2.1] + [0.0] * (RESULT_FLOATS - 10)
floats[12] = 0.85 * 0.18
floats[14] = 0.85 * 0.11
frame_r = build(TYPE_RESULT, pack('<%df' % RESULT_FLOATS, *floats))

# --- 2) v1.1 TIME / EVENT 帧 ---
import struct
frame_t = build(TYPE_TIME, pack('<I', 1761321600), seq=8)
frame_e = build(TYPE_EVENT, pack('<Iff', 1761321600, 22.7, 0.85)
                + bytes([9, 0, 1, 0]), seq=9)

out = {'result': [], 'time': [], 'event': []}
fp = FrameParser(on_result=lambda r: out['result'].append(r),
                 on_time=lambda t: out['time'].append(t),
                 on_event=lambda e: out['event'].append(e))
stream = b'\x12\x34' + frame_r + frame_t + frame_e + frame_r   # 垃圾前缀+4帧粘包
for i in range(0, len(stream), 13):                            # 任意分段喂入
    fp.feed(stream[i:i + 13])
r = out['result'][0]
print('frames parsed: %d (result=%d time=%d event=%d) crc_err=%d'
      % (fp.frames, len(out['result']), len(out['time']), len(out['event']), fp.crc_errors))
print('result: f0=%.3f thdi=%.2f i_h3=%.3f' % (r.freq, r.thdi, r.i_harm[2]))
print('time  : utc=%d' % out['time'][0])
e = out['event'][0]
print('event : utc=%d thdi=%.2f irms=%.2f win_seq=%d locked=%d'
      % (e.utc, e.thdi_trig, e.irms, e.win_seq, e.locked))
assert fp.frames == 4 and fp.crc_errors == 0, 'v1.1 frame regression FAILED'
assert abs(e.thdi_trig - 22.7) < 1e-4 and e.win_seq == 9

# --- 3) spool 断网补传缓存 ---
from spool import SpoolWriter
with tempfile.TemporaryDirectory() as td:
    sp = SpoolWriter(os.path.join(td, 'spool.jsonl'))
    for k in range(5):
        sp.enqueue({'utc': 1761321600 + k, 'thdi': 22.0 + k})
    sent = []
    ok = sp.replay(lambda rec: (sent.append(rec), True)[1])
    print('spool replay: sent=%d pending=%d' % (ok, sp.pending))
    assert ok == 5 and sp.pending == 0

# --- 4) NumPy 分析器: 合成 3/5/7 次谐波精度 ---
from analyzer import analyze_window
fs, n = 25600, 2048
t = np.arange(n) / fs
sig_i = (1.0 * np.sin(2 * np.pi * 50 * t) + 0.18 * np.sin(2 * np.pi * 150 * t)
         + 0.11 * np.sin(2 * np.pi * 250 * t) + 0.065 * np.sin(2 * np.pi * 350 * t))
sig_u = np.sin(2 * np.pi * 50 * t + 0.3)
res = analyze_window(sig_i, sig_u, fs)
expect = np.sqrt(0.18 ** 2 + 0.11 ** 2 + 0.065 ** 2) * 100
print('np: f0=%.4f Hz  thdi=%.3f%% (期望 %.3f%%)  pf=%.4f'
      % (res['freq'], res['thdi'], expect, res['pf']))
print('ALL SMOKE TESTS PASSED')
