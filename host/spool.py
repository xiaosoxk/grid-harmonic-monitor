# -*- coding: utf-8 -*-
"""
spool.py — 网关断网补传缓存 (store-and-forward)

设计:
  1) MQ 不通/发布失败时, 每条记录以 JSON 行追加到本地 spool 文件
     (append-only, 崩溃安全 —— 写一半的行重放时跳过);
  2) MQTT 恢复后 replay(): 逐行读出并重发, 全部成功才清空文件;
  3) 文件容量上限保护: 超过 max_bytes 时丢弃最旧的一半(企业级做法是
     按时间滚动分片, 此处保留最小可用实现并注明);
  4)  thread-safe: gateway 主线程写, 重连线程重放, 用锁互斥。
接入(gateway.py 两处):
  mqtt 失败 -> spool.enqueue(record)
  连接成功回调 -> spool.replay(lambda r: client.publish(topic, json.dumps(r), qos=1))
"""
from __future__ import annotations
import json, os, threading


class SpoolWriter:
    def __init__(self, path: str, max_bytes: int = 64 * 1024 * 1024):
        self.path = path
        self.max_bytes = max_bytes
        self.lock = threading.Lock()
        self.pending = 0
        if os.path.exists(path):
            with open(path, "rb") as f:
                self.pending = sum(1 for _ in f)

    def enqueue(self, record: dict) -> None:
        with self.lock:
            try:
                if os.path.exists(self.path) and \
                        os.path.getsize(self.path) > self.max_bytes:
                    self._drop_oldest_half()
                with open(self.path, "a", encoding="utf-8") as f:
                    f.write(json.dumps(record, ensure_ascii=False,
                                       separators=(",", ":")) + "\n")
                self.pending += 1
            except OSError:
                pass                      # 磁盘满等: 宁丢缓存不阻塞采集线程

    def replay(self, send_fn) -> int:
        """逐行重放, send_fn(record)->bool; 全部成功才清空, 部分失败则保留剩余"""
        sent = 0
        with self.lock:
            if not os.path.exists(self.path):
                return 0
            with open(self.path, "r", encoding="utf-8") as f:
                lines = f.readlines()
            keep = []
            for ln in lines:
                ln = ln.strip()
                if not ln:
                    continue
                try:
                    rec = json.loads(ln)
                except json.JSONDecodeError:
                    continue              # 崩溃写坏的残行: 跳过
                try:
                    if send_fn(rec):
                        sent += 1
                    else:
                        keep.append(ln)
                except OSError:
                    keep.append(ln)       # 仍不可用: 留待下次
            if not keep:
                try:
                    os.remove(self.path)
                except OSError:
                    pass
                self.pending = 0
            else:
                with open(self.path, "w", encoding="utf-8") as f:
                    f.write("\n".join(keep) + "\n")
                self.pending = len(keep)
        return sent

    def _drop_oldest_half(self) -> None:
        """容量保护: 丢最旧一半(生产级应做按天滚动分片, 见文档第5.4节)"""
        with open(self.path, "r", encoding="utf-8") as f:
            lines = f.readlines()
        with open(self.path, "w", encoding="utf-8") as f:
            f.writelines(lines[len(lines) // 2:])
