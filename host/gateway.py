# -*- coding: utf-8 -*-
"""
gateway.py — 工业网关: 串口 -> Modbus-TCP 寄存器服务器 + MQTT 发布

串口与上位机二选一占用(COM 口互斥), 网关模式适合无人值守长期运行:
  python gateway.py /dev/ttyUSB0 --modbus 0.0.0.0:5020 --mqtt mqtt://broker.local
寄存器映射(功能码 03 读保持寄存器, 全部 16 位定点, 缩放见下表):
  0    : f0 x 100        (5000 = 50.00 Hz)
  1    : Irms x 1000     (mA)
  2    : Urms x 10
  3    : P x 10   (W, 有符号)     4 : Q x 10 (var, 有符号)
  5    : S x 10                   6 : PF x 1000 (有符号)
  7    : DPF x 1000 (有符号)      8 : THDi x 100   9 : THDu x 100
  10   : [锁定|SEQ|保留]          11+: I 谐波 1..50 次 x 1000
  61+  : U 谐波 1..50 次 x 1000
"""
from __future__ import annotations
import argparse, json, threading, time
import serial

from protocol import FrameParser, ResultFrame

# ---------------- Modbus 数据存储(pymodbus 3.x) ----------------
def build_modbus_context():
    from pymodbus.datastore import (ModbusSequentialDataBlock,
                                    ModbusSlaveContext, ModbusServerContext)
    hr = ModbusSequentialDataBlock(0, [0] * 128)
    slave = ModbusSlaveContext(hr=hr, zero_mode=True)
    return ModbusServerContext(slaves=slave, single=True), slave


class RegisterUpdater(threading.Thread):
    """后台线程: 帧解析回调 -> 定点化写入寄存器 + MQTT 发布"""
    def __init__(self, slave, mqtt_cli, mqtt_topic):
        super().__init__(daemon=True)
        self.slave, self.mqtt, self.topic = slave, mqtt_cli, mqtt_topic
        self.cond = threading.Event()

    def on_result(self, r: ResultFrame):
        regs = [0] * 128
        def s16(v): return max(-32768, min(32767, int(v)))
        regs[0] = s16(r.freq * 100)
        regs[1] = s16(r.irms * 1000)
        regs[2] = s16(r.urms * 10)
        regs[3], regs[4] = s16(r.p * 10), s16(r.q * 10)
        regs[5], regs[6], regs[7] = s16(r.s * 10), s16(r.pf * 1000), s16(r.dpf * 1000)
        regs[8], regs[9] = s16(r.thdi * 100), s16(r.thdu * 100)
        regs[10] = s16(r.seq)
        for k in range(50):
            regs[11 + k] = s16(r.i_harm[k] * 1000)
            regs[61 + k] = s16(r.u_harm[k] * 1000)
        self.slave.setValues(3, 0, regs)          # 功能码3=保持寄存器区
        if self.mqtt is not None:
            payload = json.dumps({k: getattr(r, k) for k in
                                  ("freq", "irms", "urms", "p", "q", "s",
                                   "pf", "dpf", "thdi", "thdu")})
            self.mqtt.publish(self.topic, payload, qos=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--modbus", default="0.0.0.0:5020", help="监听地址:端口")
    ap.add_argument("--mqtt", default=None, help="如 mqtt://192.168.1.10:1883")
    ap.add_argument("--topic", default="pq/gw01/result")
    args = ap.parse_args()

    mqtt_cli = None
    if args.mqtt:
        import paho.mqtt.client as mqtt
        host = args.mqtt.split("//")[-1].split(":")[0]
        port = int(args.mqtt.split(":")[-1]) if ":" in args.mqtt.split("//")[-1] else 1883
        mqtt_cli = mqtt.Client(client_id="pq-gateway", protocol=mqtt.MQTTv311)
        mqtt_cli.connect(host, port, keepalive=30)
        mqtt_cli.loop_start()

    ctx, slave = build_modbus_context()
    updater = RegisterUpdater(slave, mqtt_cli, args.topic)

    parser = FrameParser(on_result=updater.on_result)
    ser = serial.Serial(args.port, args.baud, timeout=0.5)

    from pymodbus.server import StartTcpServer
    host, port_s = args.modbus.split(":")
    server_thread = threading.Thread(
        target=lambda: StartTcpServer(context=ctx, address=(host, int(port_s))),
        daemon=True)
    server_thread.start()
    print("Modbus-TCP 服务: %s | MQTT: %s | 串口: %s" %
          (args.modbus, args.mqtt, args.port))

    while True:                                 # 主线程专职收串口
        data = ser.read(4096)
        if data:
            parser.feed(data)


if __name__ == "__main__":
    main()
