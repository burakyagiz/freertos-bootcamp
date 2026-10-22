"""Hafta 1 PC arayuzu: buton yanit suresi deneyini izler ve sonuclari kaydeder.

Calistirma (hafta-01 klasorunden):  python interface/app.py
Ayrintilar ve adimlar: interface/README.md

Yapi: SerialReader (ayri thread) satirlari kuyruga koyar; bu pencere kuyrugu
50 ms'de bir okur ve Session'a verir. Pencereye sadece ana thread dokunur.
Arayuz karta hicbir zaman veri gondermez (olcumu etkilememek icin).
"""
from __future__ import annotations

import queue
import tkinter as tk
from pathlib import Path
from tkinter import ttk

import matplotlib

matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg  # noqa: E402
from matplotlib.figure import Figure  # noqa: E402
from serial.tools import list_ports  # noqa: E402

import storage  # noqa: E402
from serial_reader import SerialReader  # noqa: E402
from session import DEADLINE_MS, TARGET_EVENTS, Session  # noqa: E402

REPO_DIR = Path(__file__).resolve().parent.parent
DEFAULT_OUT_DIR = REPO_DIR / "measurements"

POLL_MS = 50               # kuyruk okuma araligi
MAX_EVENTS_PER_POLL = 500  # cok veri gelse de pencere donmasin
FLASH_MS = 800             # BTN gostergesinin vurgulu kalma suresi

# Grafik renkleri (dataviz referans paleti, acik tema).
SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SERIES = "#2a78d6"         # R olcumleri
CRITICAL = "#d03b3b"       # deadline cizgisi
FLASH_BG = "#fab219"       # BTN vurgusu

# Grafikte gosterilen kayip/hata sayaclari (CNT satirindan).
DROP_COUNTERS = ("btn_drop", "tx_drop", "tx_error", "timeout", "tel_drop", "rec_overflow")


class App:
    def __init__(self, root: tk.Tk, out_dir: Path = DEFAULT_OUT_DIR) -> None:
        self.root = root
        self.out_dir = out_dir
        self.session = Session()
        self.events: queue.Queue = queue.Queue()
        self.reader: SerialReader | None = None
        self._conn_error: str | None = None
        self._shown_generation: int | None = None
        self._flash_job: str | None = None
        self.saved: tuple[Path, Path] | None = None
        self.save_error: str | None = None

        root.title("Hafta 1 · Buton yanıt süresi")
        root.minsize(860, 680)
        self._build_ui()
        self.refresh_ports()
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(POLL_MS, self._poll)

    # ------------------------------------------------------------ arayuz
    def _build_ui(self) -> None:
        pad = {"padx": 8, "pady": 4}

        # Baglanti satiri
        top = ttk.Frame(self.root)
        top.pack(fill="x", **pad)
        ttk.Label(top, text="Port:").pack(side="left")
        self.port_box = ttk.Combobox(top, state="readonly", width=46)
        self.port_box.pack(side="left", padx=4)
        ttk.Button(top, text="Yenile", command=self.refresh_ports).pack(side="left")
        self.connect_btn = ttk.Button(top, text="Bağlan", command=self.toggle_connection)
        self.connect_btn.pack(side="left", padx=4)
        self.conn_label = ttk.Label(top, text="Bağlı değil")
        self.conn_label.pack(side="left", padx=8)

        # Deney durumu
        info = ttk.Frame(self.root)
        info.pack(fill="x", **pad)
        self.scenario_label = ttk.Label(info, font=("Segoe UI", 12, "bold"))
        self.scenario_label.pack(side="left")
        self.event_label = ttk.Label(info, font=("Segoe UI", 12))
        self.event_label.pack(side="left", padx=16)
        self.phase_label = ttk.Label(info, font=("Segoe UI", 12))
        self.phase_label.pack(side="left", padx=16)

        # TEL ve BTN ayri alanlarda
        mid = ttk.Frame(self.root)
        mid.pack(fill="x", **pad)
        tel = ttk.LabelFrame(mid, text="Telemetri (TEL)")
        tel.pack(side="left", fill="both", expand=True, padx=(0, 4))
        self.tel_last = ttk.Label(tel, text="–", font=("Consolas", 11))
        self.tel_last.pack(anchor="w", padx=6, pady=(6, 2))
        self.tel_count = ttk.Label(tel)
        self.tel_count.pack(anchor="w", padx=6, pady=(0, 6))

        btn = ttk.LabelFrame(mid, text="Buton (BTN)")
        btn.pack(side="left", fill="both", expand=True, padx=(4, 0))
        self.btn_indicator = tk.Label(btn, text="Henüz basılmadı",
                                      font=("Segoe UI", 20, "bold"), height=2)
        self.btn_indicator.pack(fill="both", expand=True, padx=6, pady=6)
        self._btn_idle_bg = self.btn_indicator.cget("background")

        # Uzun metinler (dosya yollari, hata mesajlari) pencereyi genisletmesin,
        # alt satira kaysin.
        wrap = "8.5i"

        # 64 bayt dogrulamasi
        self.check_label = ttk.Label(self.root, wraplength=wrap)
        self.check_label.pack(fill="x", **pad)

        # Sonuclar
        results = ttk.LabelFrame(self.root, text="Sonuçlar (deney bitince)")
        results.pack(fill="both", expand=True, **pad)
        self.figure = Figure(figsize=(8, 3.2), dpi=100, layout="constrained")
        self.figure.set_facecolor(SURFACE)
        self.ax = self.figure.add_subplot(111)
        self.canvas = FigureCanvasTkAgg(self.figure, master=results)
        self.canvas.get_tk_widget().pack(fill="both", expand=True, padx=4, pady=4)
        self.summary_label = ttk.Label(results, justify="left", wraplength=wrap)
        self.summary_label.pack(anchor="w", padx=6)
        self.drops_label = ttk.Label(results, justify="left", wraplength=wrap)
        self.drops_label.pack(anchor="w", padx=6)
        self.saved_label = ttk.Label(results, justify="left", wraplength=wrap)
        self.saved_label.pack(anchor="w", padx=6, pady=(0, 6))

        self._draw_plot()

    def refresh_ports(self) -> None:
        ports = sorted(list_ports.comports(), key=lambda p: p.device)
        values = [f"{p.device} — {p.description}" for p in ports]
        self.port_box["values"] = values
        if values and self.port_box.get() not in values:
            # USB-TTL (CP210x) varsa onu sec; yoksa ilk port.
            self.port_box.set(next((v for v in values if "CP210" in v), values[0]))

    # ------------------------------------------------------------ baglanti
    def toggle_connection(self) -> None:
        if self.reader is not None:
            self.reader.stop()
            self.connect_btn.config(state="disabled")  # "closed" olayi gelince acilir
            return
        selected = self.port_box.get()
        if not selected:
            self.conn_label.config(text="Önce bir port seç")
            return
        port = selected.split(" ", 1)[0]
        self._conn_error = None
        self.reader = SerialReader(port, self.events)
        self.reader.start()
        self.connect_btn.config(text="Kes")
        self.conn_label.config(text=f"{port} açılıyor…")

    def close(self) -> None:
        if self.reader is not None:
            self.reader.stop()
            self.reader.join(timeout=0.5)   # port duzgun kapansin
        self.root.destroy()

    # ------------------------------------------------------------ olaylar
    def _poll(self) -> None:
        for _ in range(MAX_EVENTS_PER_POLL):
            try:
                kind, payload = self.events.get_nowait()
            except queue.Empty:
                break
            self._handle_event(kind, payload)
        self._refresh_status()
        self.root.after(POLL_MS, self._poll)

    def _handle_event(self, kind: str, payload) -> None:
        if kind == "line":
            self.process_raw(payload)
        elif kind == "connected":
            self.conn_label.config(
                text=f"{payload} bağlı · kartı resetle ya da debugger'da Resume'a bas")
        elif kind == "error":
            self._conn_error = payload
        elif kind == "closed":
            self.reader = None
            self.connect_btn.config(text="Bağlan", state="normal")
            self.conn_label.config(text=f"Bağlantı hatası: {self._conn_error}"
                                   if self._conn_error else "Bağlı değil")

    def process_raw(self, raw: bytes) -> None:
        """Bir satiri isler. Test icin seri port olmadan da cagrilabilir."""
        line = self.session.feed(raw)
        if self.session.generation != self._shown_generation:
            self._show_new_session()

        if line.kind == "TEL":
            self.tel_last.config(text=line.text)
        elif line.kind == "BTN":
            self._flash_button()
        elif line.kind == "END":
            self._finish()

    def _show_new_session(self) -> None:
        """Kart resetlendi: onceki oturumun ekranini temizle."""
        self._shown_generation = self.session.generation
        self.saved = None
        self.save_error = None
        self.tel_last.config(text="–")
        self.btn_indicator.config(text="Henüz basılmadı", background=self._btn_idle_bg)
        for label in (self.summary_label, self.drops_label, self.saved_label):
            label.config(text="")
        self._draw_plot()

    def _flash_button(self) -> None:
        self.btn_indicator.config(text=f"Butona basıldı · Olay {self.session.last_event_id}",
                                  background=FLASH_BG)
        if self._flash_job is not None:
            self.root.after_cancel(self._flash_job)
        self._flash_job = self.root.after(FLASH_MS, self._unflash)

    def _unflash(self) -> None:
        self._flash_job = None
        self.btn_indicator.config(background=self._btn_idle_bg)

    def _refresh_status(self) -> None:
        s = self.session
        self.scenario_label.config(text=f"Senaryo: {s.scenario or '–'}")
        last = "–" if s.last_event_id is None else str(s.last_event_id)
        self.event_label.config(text=f"Son olay: {last}  (BTN {s.btn_lines} / {TARGET_EVENTS})")

        phase = s.phase()
        if phase == "waiting":
            text = "Kart bekleniyor: kartı resetle" if self.reader else ""
        elif phase == "warmup":
            text = f"Isınma: {s.warmup_left_s():.1f} s · butona basma"
        elif phase == "measuring":
            text = "Ölçüm: butona bas (aralar ≥ 0,5 s, düzensiz ritim)"
        elif phase == "exporting":
            text = f"Dışa aktarılıyor… {len(s.records)} REC"
        else:
            text = "Bitti"
        self.phase_label.config(text=text)

        self.tel_count.config(text=f"TEL satırı: {s.tel_lines}")
        bad = " · ".join(f"{kind} {n}" for kind, n in s.bad_length.items())
        check = f"64 bayt olmayan satır: {bad}   |   çözülemeyen satır: {s.parse_errors}"
        if s.last_error:
            check += f" (son: {s.last_error})"
        self.check_label.config(text=check)

    # ------------------------------------------------------------ sonuc
    def _finish(self) -> None:
        """END geldi: kaydet, grafigi ve ozeti goster."""
        try:
            self.saved = storage.save_session(self.session, self.out_dir)
            self.save_error = None
        except OSError as exc:
            self.saved = None
            self.save_error = str(exc)
        self._draw_plot()
        self._show_summary()

    def _show_summary(self) -> None:
        s = self.session
        sm = s.summary()

        def ms(value: float | None) -> str:
            return "–" if value is None else f"{value:.2f}"

        statuses = ", ".join(f"{k} {v}" for k, v in sm.status_counts.items())
        lines = [
            f"REC {sm.total} · R min / ort / max = {ms(sm.r_min_ms)} / {ms(sm.r_avg_ms)} / "
            f"{ms(sm.r_max_ms)} ms · {DEADLINE_MS:g} ms aşımı: {sm.deadline_miss} · "
            f"yanıt yok: {sm.no_response} · durumlar: {statuses}",
        ]
        if not s.end_matches():
            lines.append(f"UYARI: END rec={s.end_rec}, ama {len(s.records)} REC satırı alındı")
        self.summary_label.config(text="\n".join(lines))

        drops = " · ".join(
            f"{name} {'–' if s.counters.get(name) is None else s.counters[name]}"
            for name in DROP_COUNTERS)
        self.drops_label.config(text=f"Kayıp / hata (CNT): {drops}")

        if self.saved:
            paths = " ve ".join(str(self._relative(p)) for p in self.saved)
            self.saved_label.config(text=f"Kaydedildi: {paths}")
        else:
            self.saved_label.config(text=f"KAYDEDİLEMEDİ: {self.save_error}")

    @staticmethod
    def _relative(path: Path) -> Path:
        try:
            return path.resolve().relative_to(REPO_DIR)
        except ValueError:
            return path

    def _draw_plot(self) -> None:
        ax = self.ax
        ax.clear()
        ax.set_facecolor(SURFACE)
        s = self.session

        points = [(r.event_id, r.response_us() / 1000.0)
                  for r in s.records if r.response_us() is not None]
        missing = [r.event_id for r in s.records if r.response_us() is None]

        if points:
            ids, values = zip(*points)
            ax.plot(ids, values, linestyle="none", marker="o", markersize=6,
                    color=SERIES, markeredgecolor=SURFACE, markeredgewidth=1,
                    label="R = t4 − t0", zorder=3)
        if missing:
            ax.plot(missing, [0.0] * len(missing), linestyle="none", marker="x",
                    markersize=7, color=TEXT_PRIMARY, markeredgewidth=1.5,
                    label="yanıt yok (drop / hata / timeout)", zorder=3)
            ax.legend(loc="upper left", fontsize=8, frameon=False, labelcolor=TEXT_SECONDARY)

        ax.axhline(DEADLINE_MS, color=CRITICAL, linestyle="--", linewidth=1, zorder=2)
        ax.annotate(f"deadline {DEADLINE_MS:g} ms", xy=(1.0, DEADLINE_MS),
                    xycoords=("axes fraction", "data"), xytext=(-4, 4),
                    textcoords="offset points", ha="right", va="bottom",
                    fontsize=8, color=TEXT_SECONDARY)

        top = max([DEADLINE_MS] + [v for _, v in points]) * 1.15
        ax.set_ylim(0, top)
        ax.set_xlabel("olay (event_id)", color=TEXT_SECONDARY, fontsize=9)
        ax.set_ylabel("R (ms)", color=TEXT_SECONDARY, fontsize=9)
        title = (f"{s.scenario}: R = t4 − t0 (n = {len(s.records)})" if s.records
                 else "Deney bitince R = t4 − t0 grafiği burada")
        ax.set_title(title, color=TEXT_PRIMARY, fontsize=10, loc="left")
        ax.tick_params(colors=TEXT_SECONDARY, labelsize=8)
        ax.grid(color=GRID, linewidth=0.8)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color(GRID)
        self.canvas.draw_idle()


def main() -> None:
    try:
        # Windows'ta yuksek DPI ekranda bulanik yazilari onler.
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except (AttributeError, OSError):
        pass
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
