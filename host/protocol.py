# -*- coding: utf-8 -*-
"""
protocol.py — STM32 <-> Linux 串口帧协议(接收端解析)

帧结构: AA 55 | TYPE(1) | LEN(2,LE) | SEQ(1) | PAYLOAD(LEN) | CRC16(2,LE)
CRC16-MODBUS 覆盖 TYPE..PAYLOAD。解析器为字节流状态机, 任意时刻断线/粘包均可恢复。
"""
from __future__ import annotations
import struct
from dataclasses import dataclass
from typing import Callable, Optional

SYNC = b"\xaa\x55"
TYPE_WAVE, TYPE_RESULT, TYPE_HELLO = 0x01, 0x02, 0x03
TYPE_TIME, TYPE_EVENT = 0x04, 0x05         # v1.1: 对时帧 / 事件帧
HARM_MAX = 50
RESULT_FLOATS = 10 + 2 * HARM_MAX          # 10 个指标 + I/U 各 50 次谐波
HEADER_LEN = 6                              # SYNC(2)+TYPE(1)+LEN(2)+SEQ(1)
MAX_PAYLOAD = 8192                          # 协议最大载荷(波形帧 4096B 的2倍裕量)


def crc16_modbus(data: bytes) -> int:
    """CRC16-MODBUS: 初值 0xFFFF, 多项式 0xA001(反转 0x8005), 低字节在前"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


@dataclass
class ResultFrame:
    seq: int
    freq: float; irms: float; urms: float
    p: float; q: float; s: float
    pf: float; dpf: float
    thdi: float; thdu: float
    i_harm: list  # 下标 0 对应 1 次谐波
    u_harm: list

    @classmethod
    def unpack(cls, seq: int, payload: bytes) -> "ResultFrame":
        vals = struct.unpack("<%df" % RESULT_FLOATS, payload)
        return cls(seq=seq, freq=vals[0], irms=vals[1], urms=vals[2],
                   p=vals[3], q=vals[4], s=vals[5], pf=vals[6], dpf=vals[7],
                   thdi=vals[8], thdu=vals[9],
                   i_harm=list(vals[10:10 + HARM_MAX]),
                   u_harm=list(vals[10 + HARM_MAX:]))


@dataclass
class WaveFrame:
    seq: int
    i: list  # int16, 2:1 抽取后的电流窗口
    u: list

    @classmethod
    def unpack(cls, seq: int, payload: bytes) -> "WaveFrame":
        n = len(payload) // 4                # 每点对 4 字节(I,U 各 int16)
        vals = struct.unpack("<%dh" % (2 * n), payload)
        return cls(seq=seq, i=list(vals[0::2]), u=list(vals[1::2]))


@dataclass
class EventFrame:
    """v1.1 事件帧: THD 越限触发的元数据, 波形紧随其后以 WAVE 帧发送"""
    seq: int
    utc: int        # 设备侧 UTC 秒(事件发生时刻)
    thdi_trig: float
    irms: float
    win_seq: int
    locked: int

    @classmethod
    def unpack(cls, seq: int, payload: bytes) -> "EventFrame":
        utc, thdi, irms = struct.unpack_from("<Iff", payload, 0)
        win_seq = payload[12] | (payload[13] << 8)
        return cls(seq=seq, utc=utc, thdi_trig=thdi, irms=irms,
                   win_seq=win_seq, locked=payload[14])


class FrameParser:
    """字节流 -> 完整帧。feed() 逐块喂入, 通过回调返回解析结果。
    抗异常: CRC 错误计错并丢弃本帧; 同步字扫描从错误处自动恢复。
    未知帧类型(未来版本新增)按长度跳过 —— 向后兼容的关键。"""

    def __init__(self,
                 on_result: Callable[[ResultFrame], None],
                 on_wave: Optional[Callable[[WaveFrame], None]] = None,
                 on_hello: Optional[Callable[[bytes], None]] = None,
                 on_time: Optional[Callable[[int], None]] = None,
                 on_event: Optional[Callable[[EventFrame], None]] = None):
        self.on_result, self.on_wave, self.on_hello = on_result, on_wave, on_hello
        self.on_time, self.on_event = on_time, on_event
        self.buf = bytearray()
        self.crc_errors = 0
        self.frames = 0

    def feed(self, data: bytes) -> None:
        self.buf += data
        while self._try_extract():
            pass

    # -- 内部: 从缓冲中尽力提取一帧 --------------------------------------
    def _try_extract(self) -> bool:
        # 1) 找帧头
        idx = self.buf.find(SYNC)
        if idx < 0:
            del self.buf[:-1]              # 保留最后1字节(可能被截半的 0xAA)
            return False
        if idx > 0:
            del self.buf[:idx]
        if len(self.buf) < HEADER_LEN:
            return False
        ftype = self.buf[2]
        length = self.buf[3] | (self.buf[4] << 8)
        seq = self.buf[5]
        total = HEADER_LEN + length + 2    # + CRC16
        if length > MAX_PAYLOAD:
            del self.buf[:1]               # 超协议上限 -> 假帧头, 前进1字节重扫
            return True                    # 继续本轮循环扫描, 不要退出
        if len(self.buf) < total:
            return False                   # 帧未收全, 等下一次 feed
        payload = bytes(self.buf[6:6 + length])
        crc_rx = self.buf[6 + length] | (self.buf[7 + length] << 8)
        del self.buf[:total]
        if crc16_modbus(bytes([ftype]) + length.to_bytes(2, "little")
                        + bytes([seq]) + payload) != crc_rx:
            self.crc_errors += 1
            return True                    # 丢弃坏帧, 继续扫下一帧
        self.frames += 1
        if ftype == TYPE_RESULT and length == RESULT_FLOATS * 4:
            self.on_result(ResultFrame.unpack(seq, payload))
        elif ftype == TYPE_WAVE:
            if self.on_wave:
                self.on_wave(WaveFrame.unpack(seq, payload))
        elif ftype == TYPE_HELLO:
            if self.on_hello:
                self.on_hello(payload)
        elif ftype == TYPE_TIME and length == 4:
            if self.on_time:
                self.on_time(struct.unpack("<I", payload)[0])
        elif ftype == TYPE_EVENT and length == 16:
            if self.on_event:
                self.on_event(EventFrame.unpack(seq, payload))
        # 其余未知类型: 已按长度消费, 直接向后兼容跳过
        return True
