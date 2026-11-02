"""Live DSO152 waveform viewer that reads the stock firmware's RAM over SWD.

No firmware changes: OpenOCD (ST-Link) reads the ADC ring buffer and the
stock settings/measurements while the scope keeps running.

Stock firmware RAM map (DSO152_APP_V0.1, decoded from the app and live logs):
  0x20004104  1024 x u16 ADC samples, DMA1 ch1 ring buffer
  0x2000012C  u16 write position (1024 - CNDTR)
  0x20000112  1 = RUN, 0 = STOP (key 0x04 toggles it)
  0x20000113  x10 probe flag (1 = x10)
  0x20000114  coupling row of the zero table (0/1)
  0x20000120  volts/div index, 0 = 10 mV ... 9 = 10 V
  0x20000121  time/div index, 0 = 10 us ... 23 = 500 s
  0x20000118  s32 trigger level: triggers when (sample - zero) / 4 > level
  0x2000011C  s32 horizontal trigger position (menu item 1)
  0x20000123  trigger mode, 0..2 (key 0x01 cycles it)
  0x20000126  menu cursor: 1 = trig position, 2 = volts, 3 = probe x1/x10,
              4 = time, 5 = coupling, 6 = trigger level
  0x2000010C  pending key code, 0xFF = none. Writing a code here acts like a
              button press: 0x02 = value up, 0x03 = value down, 0x08 = next item,
              0x01 = trigger mode, 0x04 = RUN/STOP
  0x20003C28  u16 zero level [2][11], indexed [coupling][volts index]
  0x20000138  s32 Vave, Vmax (13C), Vmin (140), Vpp (144), Vrms (14C)
Scale (from the stock Vmax code at 0x080073EC):
  value = (sample - zero) * 20;  volts = value / DIVISOR[vidx] / 100 * probe
ADC is triggered by TIM1: sample period = (PSC+1)*(ARR+1) / 80 MHz.
"""
import os
import socket
import subprocess
import time
import tkinter as tk
from tkinter import filedialog, messagebox

OPENOCD = os.environ.get("OPENOCD", "openocd")   # path to openocd.exe if not on PATH
TCL_PORT = 6666
SAMPLES = 0x20004104
N = 1024
WRITE_POS = 0x2000012C
STATE = 0x20000110      # 0x20 bytes: settings
ZERO_TABLE = 0x20003C28
MEAS = 0x20000138       # 6 x s32
TIM1_PSC = 0x40012C28   # PSC, ARR
TIMER_CLK = 80e6
KEY = 0x2000010C
CURSOR = 0x20000126
KEY_UP, KEY_DOWN, KEY_NEXT, KEY_TRIG_MODE, KEY_RUN_STOP = 0x02, 0x03, 0x08, 0x01, 0x04
ITEM_VOLTS, ITEM_TIME, ITEM_TRIG_LEVEL = 2, 4, 6
TRIG_MODES = ["AUTO", "NORMAL", "SINGLE"]   # order assumed from the stock strings

VOLTS = ["10mV", "20mV", "50mV", "100mV", "200mV", "500mV", "1V", "2V", "5V", "10V", "20V"]
VOLTS_PER_DIV = [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20]
DIVISOR = [1600, 800, 400, 200, 100, 40, 20, 8, 4, 2, 1]
TIMES = ["10us", "20us", "50us", "100us", "200us", "500us", "1ms", "2ms", "5ms",
         "10ms", "20ms", "50ms", "100ms", "200ms", "500ms", "1s", "2s", "5s",
         "10s", "20s", "50s", "100s", "200s", "500s"]
SEC_PER_DIV = [10e-6, 20e-6, 50e-6, 100e-6, 200e-6, 500e-6, 1e-3, 2e-3, 5e-3,
               10e-3, 20e-3, 50e-3, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500]
H_DIVS, V_DIVS = 10, 8


def s32(x):
    return x - (1 << 32) if x & 0x80000000 else x


def fmt_time(t):
    for unit, f in (("s", 1), ("ms", 1e-3), ("us", 1e-6)):
        if abs(t) >= f:
            return f"{t / f:.3g}{unit}"
    return f"{t / 1e-9:.3g}ns"


def fmt_freq(f):
    if f >= 1e3:
        return f"{f / 1e3:.3f}kHz"
    return f"{f:.2f}Hz"


class OpenOCD:
    def __init__(self):
        self.proc = subprocess.Popen(
            [OPENOCD, "-f", "interface/stlink.cfg", "-c", "transport select swd",
             "-c", "set CPUTAPID 0", "-f", "target/stm32f1x.cfg",
             "-c", "adapter speed 1800", "-c", "reset_config none",
             "-c", f"tcl_port {TCL_PORT}", "-c", "gdb_port disabled",
             "-c", "telnet_port disabled", "-c", "init"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.sock = None
        for _ in range(50):
            try:
                self.sock = socket.create_connection(("127.0.0.1", TCL_PORT), timeout=2)
                break
            except OSError:
                if self.proc.poll() is not None:
                    raise RuntimeError("OpenOCD exited - is the ST-Link connected and wired?")
                time.sleep(0.2)
        if self.sock is None:
            raise RuntimeError("Could not reach OpenOCD")

    def cmd(self, text):
        self.sock.sendall(text.encode() + b"\x1a")
        buf = b""
        while not buf.endswith(b"\x1a"):
            chunk = self.sock.recv(65536)
            if not chunk:
                raise RuntimeError("OpenOCD connection closed")
            buf += chunk
        return buf[:-1].decode(errors="replace")

    def read(self, addr, width, count):
        out = self.cmd(f"read_memory 0x{addr:08x} {width} {count}")
        return [int(x, 16) for x in out.split()]

    def press(self, key, timeout=1.0, settle=0.3):
        """Inject a key code and wait until the stock firmware consumes it.
        The firmware clears the code before it applies the change, so wait
        `settle` seconds afterwards before reading the new state."""
        self.cmd(f"mwb 0x{KEY:08x} 0x{key:02x}")
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self.read(KEY, 8, 1)[0] == 0xFF:
                time.sleep(settle)
                return True
            time.sleep(0.02)
        return False

    def adjust(self, item, key):
        """Move the menu cursor to `item`, then press up/down once."""
        for _ in range(12):
            if self.read(CURSOR, 8, 1)[0] == item:
                return self.press(key)
            if not self.press(KEY_NEXT):
                return False
        return False

    def close(self):
        try:
            if self.sock:
                self.cmd("shutdown")
        except Exception:
            pass
        self.proc.terminate()


class Frame:
    """One decoded capture: samples (oldest first) in volts plus scope state."""

    def __init__(self, ocd):
        raw = ocd.read(SAMPLES, 16, N)
        pos = ocd.read(WRITE_POS, 16, 1)[0] % N
        st = ocd.read(STATE, 8, 0x20)
        zeros = ocd.read(ZERO_TABLE, 16, 22)
        meas = [s32(x) for x in ocd.read(MEAS, 32, 6)]
        psc, arr = ocd.read(TIM1_PSC, 32, 2)

        self.probe = 10 if st[0x03] == 1 else 1
        self.coupling = min(st[0x04], 1)
        self.vidx = min(st[0x10], len(VOLTS) - 1)
        self.tidx = min(st[0x11], len(TIMES) - 1)
        self.zero = zeros[self.coupling * 11 + min(self.vidx, 10)]
        self.vdiv = VOLTS_PER_DIV[self.vidx] * self.probe
        self.tdiv = SEC_PER_DIV[self.tidx]
        self.dt = (psc + 1) * (arr + 1) / TIMER_CLK

        k = 20 / DIVISOR[self.vidx] / 100 * self.probe       # volts per count
        ordered = raw[pos:] + raw[:pos]
        self.volts = [(s - self.zero) * k for s in ordered]
        self.raw = ordered

        level = s32(st[0x08] | st[0x09] << 8 | st[0x0A] << 16 | st[0x0B] << 24)
        self.trig_level = level * 4 * k
        self.trig_mode = st[0x13]
        self.running = st[0x02] == 1

        to_v = lambda x: x / DIVISOR[self.vidx] / 100 * self.probe
        self.vave, self.vmax, self.vmin, self.vpp = (to_v(m) for m in meas[:4])
        self.vrms = to_v(meas[5])


class Viewer:
    def __init__(self, root):
        self.root = root
        root.title("DSO152 SWD Viewer")
        root.geometry("1040x680")
        root.configure(bg="#12151c")
        self.ocd = None
        self.paused = False
        self.frame = None
        self.frames = 0
        self.fps = 0
        self.t_fps = time.time()
        self.show_all = tk.BooleanVar(value=False)
        self.zero_div = V_DIVS / 3      # 0 V line, in divisions up from the bottom

        bar = tk.Frame(root, bg="#1a1e28")
        bar.pack(fill=tk.X, padx=10, pady=(10, 5))
        tk.Label(bar, text="DSO152", font=("Segoe UI", 13, "bold"),
                 fg="#e2e8f0", bg="#1a1e28").pack(side=tk.LEFT, padx=12, pady=6)
        self.lbl_set = tk.Label(bar, text="connecting...", font=("Consolas", 12, "bold"),
                                fg="#facc15", bg="#1a1e28")
        self.lbl_set.pack(side=tk.LEFT, padx=10)
        self.lbl_fps = tk.Label(bar, text="", font=("Consolas", 10),
                                fg="#64748b", bg="#1a1e28")
        self.lbl_fps.pack(side=tk.LEFT, padx=10)
        tk.Button(bar, text="Export CSV", bg="#334155", fg="white", relief=tk.FLAT,
                  command=self.export).pack(side=tk.RIGHT, padx=6, pady=6)
        self.btn_pause = tk.Button(bar, text="Pause", bg="#475569", fg="white",
                                   relief=tk.FLAT, command=self.toggle)
        self.btn_pause.pack(side=tk.RIGHT, padx=6, pady=6)
        ctl = tk.Frame(root, bg="#1a1e28")
        ctl.pack(fill=tk.X, padx=10, pady=(0, 5))
        for text, item, key in (("V/div ▲", ITEM_VOLTS, KEY_UP),
                                ("V/div ▼", ITEM_VOLTS, KEY_DOWN),
                                ("Time ▲", ITEM_TIME, KEY_UP),
                                ("Time ▼", ITEM_TIME, KEY_DOWN)):
            tk.Button(ctl, text=text, width=9, bg="#1e3a5f", fg="white", relief=tk.FLAT,
                      command=lambda i=item, k=key: self.adjust(i, k)).pack(
                          side=tk.LEFT, padx=4, pady=6)
        for text, key in (("Trig ▲", KEY_UP), ("Trig ▼", KEY_DOWN)):
            tk.Button(ctl, text=text, width=9, bg="#5f4b1e", fg="white", relief=tk.FLAT,
                      command=lambda k=key: self.adjust(ITEM_TRIG_LEVEL, k)).pack(
                          side=tk.LEFT, padx=4, pady=6)
        tk.Button(ctl, text="Trig mode", width=9, bg="#5f4b1e", fg="white", relief=tk.FLAT,
                  command=lambda: self.send_key(KEY_TRIG_MODE)).pack(side=tk.LEFT, padx=4, pady=6)
        self.btn_run = tk.Button(ctl, text="RUN/STOP", width=9, bg="#7f1d1d", fg="white",
                                 relief=tk.FLAT, command=lambda: self.send_key(KEY_RUN_STOP))
        self.btn_run.pack(side=tk.LEFT, padx=4, pady=6)
        tk.Label(ctl, text="(sent to the scope as button presses)", font=("Segoe UI", 9),
                 fg="#64748b", bg="#1a1e28").pack(side=tk.LEFT, padx=8)

        tk.Button(bar, text="0 V ▼", bg="#334155", fg="white", relief=tk.FLAT,
                  command=lambda: self.move_zero(-0.5)).pack(side=tk.RIGHT, padx=2, pady=6)
        tk.Button(bar, text="0 V ▲", bg="#334155", fg="white", relief=tk.FLAT,
                  command=lambda: self.move_zero(0.5)).pack(side=tk.RIGHT, padx=2, pady=6)
        root.bind("<Up>", lambda e: self.move_zero(0.5))
        root.bind("<Down>", lambda e: self.move_zero(-0.5))
        tk.Checkbutton(bar, text="Whole buffer", variable=self.show_all, bg="#1a1e28",
                       fg="#cbd5e1", selectcolor="#334155", activebackground="#1a1e28",
                       activeforeground="#e2e8f0").pack(side=tk.RIGHT, padx=6)

        self.canvas = tk.Canvas(root, bg="#0b0e14", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        self.meas = tk.Label(root, text="", font=("Consolas", 12, "bold"),
                             fg="#38bdf8", bg="#12151c", anchor="w")
        self.meas.pack(fill=tk.X, padx=14, pady=(0, 10))

        root.protocol("WM_DELETE_WINDOW", self.quit)
        root.after(100, self.connect)

    def connect(self):
        try:
            self.ocd = OpenOCD()
        except Exception as e:
            messagebox.showerror("OpenOCD", str(e))
            self.root.destroy()
            return
        self.poll()

    def poll(self):
        if not self.paused:
            try:
                self.frame = Frame(self.ocd)
                self.frames += 1
            except Exception as e:
                self.lbl_set.config(text=f"read error: {e}")
        now = time.time()
        if now - self.t_fps >= 1:
            self.fps, self.frames, self.t_fps = self.frames, 0, now
            self.lbl_fps.config(text=f"{self.fps} fps")
        self.draw()
        self.root.after(30, self.poll)

    @staticmethod
    def trigger(volts, count, level):
        """Latest rising crossing of `level` that still has `count` samples
        after it; None if there is no such edge."""
        for i in range(len(volts) - count, 0, -1):
            if volts[i - 1] < level <= volts[i]:
                return i
        return None

    @staticmethod
    def frequency(volts, dt):
        lo, hi = min(volts), max(volts)
        if hi - lo < 1e-9:
            return None
        mid = (lo + hi) / 2
        hyst = (hi - lo) * 0.1
        edges, low = [], False
        for i, v in enumerate(volts):
            if v < mid - hyst:
                low = True
            elif low and v > mid + hyst:
                edges.append(i)
                low = False
        if len(edges) < 2:
            return None
        return (len(edges) - 1) / ((edges[-1] - edges[0]) * dt)

    def draw(self):
        c = self.canvas
        w, h = c.winfo_width(), c.winfo_height()
        f = self.frame
        if w < 100 or h < 100 or f is None:
            return
        c.delete("all")

        # grid: 10 x 8 divisions; 0 V line at self.zero_div up from the bottom
        ppd = h / V_DIVS
        zero_y = h - self.zero_div * ppd
        for i in range(H_DIVS + 1):
            x = w * i / H_DIVS
            c.create_line(x, 0, x, h, fill="#334155" if i == H_DIVS // 2 else "#1e293b",
                          dash=() if i == H_DIVS // 2 else (2, 4))
        for i in range(V_DIVS + 1):
            y = h * i / V_DIVS
            c.create_line(0, y, w, y, fill="#1e293b", dash=(2, 4))
            if 0 < i < V_DIVS:
                volts = (zero_y - y) / ppd * f.vdiv
                c.create_text(4, y - 8, text=f"{volts:.3g}V", anchor="w",
                              fill="#64748b", font=("Consolas", 9))
        c.create_line(0, zero_y, w, zero_y, fill="#475569")
        c.create_text(w - 4, zero_y - 8, text="0V", anchor="e",
                      fill="#94a3b8", font=("Consolas", 9, "bold"))

        # window: 10 divisions of samples, triggered on a rising edge
        per_div = f.tdiv / f.dt if f.dt > 0 else N / H_DIVS
        count = int(round(per_div * H_DIVS))
        if self.show_all.get() or count >= N or count < 2:
            start, count, trig = 0, N, False
        else:
            t = self.trigger(f.volts, count, f.trig_level)
            start, trig = (t, True) if t is not None else (N - count, False)
        seg = f.volts[start:start + count]

        ppv = ppd / f.vdiv
        pts = []
        for i, v in enumerate(seg):
            y = zero_y - v * ppv
            pts += [w * i / max(count - 1, 1), max(-2, min(h + 2, y))]
        if len(pts) >= 4:
            c.create_line(*pts, fill="#22c55e", width=2)
        trig_y = zero_y - f.trig_level * ppv
        if 0 <= trig_y <= h:
            c.create_line(0, trig_y, w, trig_y, fill="#facc15", dash=(6, 4))
            c.create_text(w - 4, trig_y + 8, text=f"T {f.trig_level:.3g}V", anchor="e",
                          fill="#facc15", font=("Consolas", 9, "bold"))
        if trig:
            c.create_text(6, 6, text="T", anchor="nw", fill="#facc15", font=("Consolas", 11, "bold"))

        freq = self.frequency(seg, f.dt)
        cpl = "AC" if f.coupling else "DC"
        probe = "  x10" if f.probe == 10 else ""
        mode = TRIG_MODES[f.trig_mode] if f.trig_mode < len(TRIG_MODES) else str(f.trig_mode)
        self.lbl_set.config(
            text=f"{'RUN' if f.running else 'STOP'}   "
                 f"{VOLTS[f.vidx]}/div{probe}   {TIMES[f.tidx]}/div   {cpl}?   "
                 f"Trig {mode} {f.trig_level:.3g}V   sample {fmt_time(f.dt)}")
        self.btn_run.config(text="STOP" if f.running else "RUN",
                            bg="#7f1d1d" if f.running else "#166534")
        self.meas.config(
            text=f"Vmax {f.vmax:6.2f}V   Vmin {f.vmin:6.2f}V   Vpp {f.vpp:6.2f}V   "
                 f"Vavg {f.vave:6.2f}V   Vrms {f.vrms:6.2f}V   "
                 f"Freq {fmt_freq(freq) if freq else '--'}")

    def adjust(self, item, key):
        if not self.ocd:
            return
        try:
            if not self.ocd.adjust(item, key):
                self.lbl_fps.config(text="scope did not take the key press")
        except Exception as e:
            self.lbl_fps.config(text=f"key error: {e}")

    def send_key(self, key):
        if not self.ocd:
            return
        try:
            if not self.ocd.press(key):
                self.lbl_fps.config(text="scope did not take the key press")
        except Exception as e:
            self.lbl_fps.config(text=f"key error: {e}")

    def move_zero(self, step):
        self.zero_div = max(0, min(V_DIVS, self.zero_div + step))

    def toggle(self):
        self.paused = not self.paused
        self.btn_pause.config(text="Run" if self.paused else "Pause")

    def export(self):
        f = self.frame
        if not f:
            return
        path = filedialog.asksaveasfilename(defaultextension=".csv",
                                            filetypes=[("CSV", "*.csv")])
        if path:
            with open(path, "w") as out:
                out.write(f"# {VOLTS[f.vidx]}/div, {TIMES[f.tidx]}/div, dt={f.dt}\n")
                out.write("index,time_s,raw,volts\n")
                for i, (r, v) in enumerate(zip(f.raw, f.volts)):
                    out.write(f"{i},{i * f.dt:.9g},{r},{v:.4f}\n")

    def quit(self):
        if self.ocd:
            self.ocd.close()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    Viewer(root)
    root.mainloop()
