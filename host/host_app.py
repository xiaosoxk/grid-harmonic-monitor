# -*- coding: utf-8 -*-
"""
host_app.py — Linux 上位机（PyQt5 + pyqtgraph 实时谐波监测终端）v1.1

界面布局(2x2):
  左上: 时域波形 I/U     右上: 谐波幅值谱(超限柱红色告警)
  左下: THD 趋势         右下: 电能质量指标面板(超限行变红)
交互增强(v1.1):
  - THDi 告警阈值可配置(--alarm-thdi, 默认15%), 超限后指标行/柱状图/状态栏变红;
  - 冻结显示: 数据继续解析与CSV落盘, 界面暂停刷新, 便于观察和截图;
  - 谐波柱超过单次占比阈值(默认5%)以告警色显示。
运行:
  python host_app.py COM5             # Windows
  python host_app.py /dev/ttyUSB0     # Linux / WSL2
  python host_app.py /dev/pts/3 --alarm-thdi 10   # 无硬件演示 + 自定义阈值
"""
from __future__ import annotations
import sys, csv, time, argparse
import numpy as np
import serial                                  # pyserial

from PyQt5.QtCore import QThread, pyqtSignal
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QGridLayout,
                             QLabel, QCheckBox, QHBoxLayout)
import pyqtgraph as pg

from protocol import FrameParser, ResultFrame, WaveFrame

RESULT_KEYS = ("freq", "irms", "urms", "p", "q", "s", "pf", "dpf", "thdi", "thdu")

# ---- 视觉规范(与设计文档第2章一致) ----
C_CH_I, C_CH_U = "#1B6B7A", "#C49B72"          # 通道色: 电流/电压
C_ALARM, C_OK, C_TEXT = "#B0433B", "#7BA68A", "#24333A"
ALARM_HARM_PCT = 5.0                            # 单次谐波告警阈值(相对基波%)
STYLE_NORMAL = "font-family: Consolas, monospace; font-size: 14px; color: %s;" % C_TEXT
STYLE_ALARM = "font-family: Consolas, monospace; font-size: 14px; font-weight: bold; color: %s;" % C_ALARM


class SerialThread(QThread):
    """串口收数线程: 阻塞读 -> 帧解析 -> 信号发往UI线程(Qt跨线程安全)"""
    result_ready = pyqtSignal(object)      # ResultFrame
    wave_ready = pyqtSignal(object)        # WaveFrame
    status = pyqtSignal(str, int, int)     # 连接状态, 收帧数, CRC错帧数

    def __init__(self, port: str, baud: int = 921600):
        super().__init__()
        self.port, self.baud = port, baud
        self._running = True
        self.parser = FrameParser(on_result=lambda f: self.result_ready.emit(f),
                                  on_wave=lambda f: self.wave_ready.emit(f))

    def run(self):
        while self._running:
            try:
                with serial.Serial(self.port, self.baud, timeout=0.5) as ser:
                    self.status.emit("已连接 " + self.port, 0, 0)
                    while self._running:
                        data = ser.read(4096)
                        if data:
                            self.parser.feed(data)
                            self.status.emit("已连接 " + self.port,
                                             self.parser.frames, self.parser.crc_errors)
            except (serial.SerialException, OSError) as e:
                self.status.emit("等待设备: %s" % e, self.parser.frames,
                                 self.parser.crc_errors)
                time.sleep(2.0)            # 断线自动重连(固定2s退避)

    def stop(self):
        self._running = False
        self.wait(2000)


class MainWindow(QMainWindow):
    def __init__(self, port: str, csv_log: bool, alarm_thdi: float):
        super().__init__()
        self.setWindowTitle("电网谐波监测上位机 — STM32F407 + Linux 网关")
        self.resize(1280, 800)
        self.csv_log = csv_log
        self.csv_file = None
        self.alarm_thdi = alarm_thdi
        self.frozen = False
        self.last_result: ResultFrame | None = None

        central = QWidget(); self.setCentralWidget(central)
        grid = QGridLayout(central)

        # ---- 顶部控制条 ----
        bar = QHBoxLayout()
        self.lbl_status = QLabel("未连接")
        self.chk_freeze = QCheckBox("冻结显示")
        self.chk_freeze.stateChanged.connect(
            lambda s: setattr(self, "frozen", bool(s)))
        self.chk_log = QCheckBox("CSV 记录")
        self.chk_log.setChecked(csv_log)
        bar.addWidget(self.lbl_status); bar.addStretch()
        bar.addWidget(self.chk_freeze); bar.addWidget(self.chk_log)
        grid.addLayout(bar, 0, 0, 1, 2)

        pg.setConfigOptions(antialias=True)
        # ---- 左上: 时域波形 ----
        self.plot_wave = pg.PlotWidget(title="时域波形(波形帧, 2:1抽取)")
        self.plot_wave.addLegend(offset=(10, 10))
        self.plot_wave.showGrid(x=True, y=True, alpha=0.3)
        self.curve_i = self.plot_wave.plot(pen=pg.mkPen(C_CH_I, width=1.5), name="I 电流")
        self.curve_u = self.plot_wave.plot(pen=pg.mkPen(C_CH_U, width=1.5), name="U 电压")
        grid.addWidget(self.plot_wave, 1, 0)

        # ---- 右上: 谐波幅值谱(超限柱告警色) ----
        self.plot_spec = pg.PlotWidget(title="谐波幅值谱(相对基波%%, 红=超%.0f%%)" % ALARM_HARM_PCT)
        self.plot_spec.showGrid(x=True, y=True, alpha=0.3)
        self.plot_spec.setLabel("bottom", "谐波次数 h")
        self.plot_spec.setLabel("left", "幅值 (相对基波 %)")
        self.bars_i = pg.BarGraphItem(x=[], height=[], width=0.6, brush=C_CH_I)
        self.plot_spec.addItem(self.bars_i)
        grid.addWidget(self.plot_spec, 1, 1)

        # ---- 左下: THD 趋势 ----
        self.plot_thd = pg.PlotWidget(title="THD 趋势(%)")
        self.plot_thd.addLegend(offset=(10, 10))
        self.plot_thd.showGrid(x=True, y=True, alpha=0.3)
        self.plot_thd.setLabel("bottom", "窗口序号")
        self.curve_thdi = self.plot_thd.plot(pen=pg.mkPen("#B87472", width=1.5), name="THDi")
        self.curve_thdu = self.plot_thd.plot(pen=pg.mkPen(C_OK, width=1.5), name="THDu")
        self.thd_t, self.thdi_v, self.thdu_v = [], [], []
        grid.addWidget(self.plot_thd, 2, 0)

        # ---- 右下: 指标面板 ----
        self.lbl_metrics = QLabel("等待数据帧 ...")
        self.lbl_metrics.setStyleSheet(STYLE_NORMAL)
        grid.addWidget(self.lbl_metrics, 2, 1)

        self.serial = SerialThread(port)
        self.serial.result_ready.connect(self.on_result)
        self.serial.wave_ready.connect(self.on_wave)
        self.serial.status.connect(self.on_status)
        self.serial.start()
        self.frame_seq_prev, self.lost = -1, 0

    # ---------------- 数据到达回调(UI线程) ----------------
    def on_result(self, r: ResultFrame):
        # 1) SEQ 连续性 -> 丢帧统计
        if self.frame_seq_prev >= 0:
            self.lost += (r.seq - self.frame_seq_prev - 1) % 256
        self.frame_seq_prev = r.seq

        # 2) CSV 落盘不受冻结影响(数据完整性优先)
        if self.chk_log.isChecked():
            if self.csv_file is None:
                name = time.strftime("pq_log_%Y%m%d_%H%M%S.csv")
                self.csv_file = open(name, "w", newline="", encoding="utf-8")
                w = csv.writer(self.csv_file)
                w.writerow(["t"] + list(RESULT_KEYS)
                           + ["i_h%d" % k for k in range(1, 51)]
                           + ["u_h%d" % k for k in range(1, 51)])
            self.csv_file.writerows([[time.time()] + [getattr(r, k) for k in RESULT_KEYS]
                                     + r.i_harm + r.u_harm])

        self.last_result = r
        if self.frozen:                          # 冻结: 界面保持, 数据照收
            return

        # 3) 谐波柱状图: 超限柱告警色
        h = np.arange(2, 26)
        amp = np.array(r.i_harm[1:25]) / (r.i_harm[0] + 1e-12) * 100.0
        brushes = [C_ALARM if a > ALARM_HARM_PCT else C_CH_I for a in amp]
        self.bars_i.setOpts(x=h, height=amp, width=0.6, brushes=brushes)

        # 4) THD 趋势
        self.thd_t.append(len(self.thd_t))
        self.thdi_v.append(r.thdi); self.thdu_v.append(r.thdu)
        if len(self.thd_t) > 600:
            self.thd_t, self.thdi_v, self.thdu_v = (self.thd_t[-600:],
                                                    self.thdi_v[-600:], self.thdu_v[-600:])
        self.curve_thdi.setData(self.thd_t, self.thdi_v)
        self.curve_thdu.setData(self.thd_t, self.thdu_v)

        # 5) 指标面板: THDi 超阈值整行变红加粗
        self.lbl_metrics.setStyleSheet(
            STYLE_ALARM if r.thdi > self.alarm_thdi else STYLE_NORMAL)
        self.lbl_metrics.setText(
            "f0   = %7.3f Hz\nIrms = %7.4f   Urms = %7.4f\n"
            "P    = %7.2f    Q    = %7.2f    S = %7.2f\n"
            "PF   = %7.4f    DPF  = %7.4f\nTHDi = %7.2f %%  (阈值%.0f%%)\n"
            "帧 %d  丢帧 %d  CRC错 %d"
            % (r.freq, r.irms, r.urms, r.p, r.q, r.s, r.pf, r.dpf, r.thdi,
               self.alarm_thdi, self.serial.parser.frames, self.lost,
               self.serial.parser.crc_errors))

    def on_wave(self, w: WaveFrame):
        if self.frozen:
            return
        n = len(w.i)
        t = np.arange(n) * (0.08 / n)      # 波形帧时间轴: 半窗 40ms
        self.curve_i.setData(t, np.array(w.i) / 32767.0)
        self.curve_u.setData(t, np.array(w.u) / 32767.0)

    def on_status(self, text: str, frames: int, crc: int):
        self.lbl_status.setText("%s | 帧 %d | CRC错 %d" % (text, frames, crc))

    def closeEvent(self, e):
        self.serial.stop()
        if self.csv_file:
            self.csv_file.close()
        super().closeEvent(e)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="串口路径, 如 /dev/ttyUSB0 或 COM5")
    ap.add_argument("--no-csv", action="store_true")
    ap.add_argument("--alarm-thdi", type=float, default=15.0,
                    help="THDi 告警阈值(%%), 默认15")
    args = ap.parse_args()
    app = QApplication(sys.argv)
    win = MainWindow(args.port, csv_log=not args.no_csv, alarm_thdi=args.alarm_thdi)
    win.show()
    sys.exit(app.exec_())
