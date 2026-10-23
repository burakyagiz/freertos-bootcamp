"""Faz 6: measurements/S0..S5.csv -> asama sureleri, ozet tablo ve grafikler.

Calistirma (hafta-01 klasorunden):  python analysis/analyze.py

Ciktilar (her calistirmada bastan uretilir):
  analysis/events.csv              her olay icin asama sureleri (ms)
  measurements/summary.csv         senaryo basina ozet (CLAUDE.md'deki yer)
  analysis/plots/r_per_event.png   olay no -> R, 20 ms cizgisi
  analysis/plots/stage_means.png   senaryo -> asama ortalamalari (yigilmis)

Tanimlar:
  - Farklar TIM2'nin 32-bit sayacindan, isaretsiz alinir: (b - a) mod 2^32.
  - Eksik zaman (bos alan) 0 sayilmaz; o aralik hesaplanmaz.
  - R = t4 - t0. t4'u olmayan olay "yanit yok"tur, deadline'i karsilamis sayilmaz.
  - Asama ortalamalari sadece bes zamani da olan (tamamlanan) olaylardan alinir;
    boylece yigilmis sutunun toplami ortalama R'ye esit olur.
  - p95 en yakin sira yontemiyle; n=30'da tek bir olaya dayanir, dikkatli yorumla.
"""
from __future__ import annotations

import csv
import math
import statistics
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")   # pencere acmadan PNG uret
import matplotlib.pyplot as plt  # noqa: E402

REPO_DIR = Path(__file__).resolve().parent.parent
MEAS_DIR = REPO_DIR / "measurements"
OUT_DIR = REPO_DIR / "analysis"
PLOT_DIR = OUT_DIR / "plots"

SCENARIOS = [f"S{i}" for i in range(6)]
U32 = 2 ** 32
DEADLINE_MS = 20.0
STATUSES = ("ok", "btn_drop", "tx_drop", "tx_error", "timeout")

# (sutun adi, baslangic indeksi, bitis indeksi, grafik etiketi)
STAGES = [
    ("t1_t0", 0, 1, "t1−t0  kesme → ButtonTask"),
    ("t2_t1", 1, 2, "t2−t1  yanıtın hazırlanması"),
    ("t3_t2", 2, 3, "t3−t2  txQ'da bekleme"),
    ("t4_t3", 3, 4, "t4−t3  64 baytın hatta gitmesi"),
]

# Grafik renkleri (dataviz referans paleti, acik tema; validate_palette.js ile kontrol edildi).
SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SERIES = "#2a78d6"
CRITICAL = "#d03b3b"
STAGE_COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]   # sabit sira, dongu yok


# ---------------------------------------------------------------- hesap
def diff_us(start: int | None, end: int | None) -> int | None:
    """Isaretsiz 32-bit fark; zamanlardan biri yoksa None."""
    if start is None or end is None:
        return None
    return (end - start) % U32


def load_records(path: Path) -> list[dict]:
    """S<x>.csv satirlarini okur; zamanlari int, bos alanlari None yapar."""
    records = []
    with path.open(newline="", encoding="ascii") as f:
        for row in csv.DictReader(f):
            times = [int(row[f"t{i}_us"]) if row[f"t{i}_us"] else None for i in range(5)]
            records.append({"scenario": row["scenario"], "event_id": int(row["event_id"]),
                            "status": row["status"], "t": times})
    return records


def load_counters(path: Path) -> dict[str, int | None]:
    if not path.exists():
        return {}
    with path.open(newline="", encoding="ascii") as f:
        return {r["name"]: (int(r["value"]) if r["value"] else None) for r in csv.DictReader(f)}


def event_row(rec: dict) -> dict:
    """Bir olayin asama sureleri (ms); hesaplanamayan aralik None."""
    t = rec["t"]
    row = {"scenario": rec["scenario"], "event_id": rec["event_id"], "status": rec["status"]}
    for name, a, b, _ in STAGES:
        us = diff_us(t[a], t[b])
        row[f"{name}_ms"] = None if us is None else us / 1000.0
    r_us = diff_us(t[0], t[4])
    row["r_ms"] = None if r_us is None else r_us / 1000.0
    return row


def p95_nearest_rank(values: list[float]) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    return ordered[math.ceil(0.95 * len(ordered)) - 1]


def summarize(scenario: str, events: list[dict], counters: dict) -> dict:
    r_values = [e["r_ms"] for e in events if e["r_ms"] is not None]
    complete = [e for e in events if all(e[f"{n}_ms"] is not None for n, *_ in STAGES)]
    row = {
        "scenario": scenario,
        "events": len(events),
        **{s: sum(1 for e in events if e["status"] == s) for s in STATUSES},
        "r_n": len(r_values),
        "r_min_ms": min(r_values) if r_values else None,
        "r_mean_ms": statistics.fmean(r_values) if r_values else None,
        "r_p95_ms": p95_nearest_rank(r_values),
        "r_max_ms": max(r_values) if r_values else None,
        "deadline_miss": sum(1 for v in r_values if v > DEADLINE_MS),
        "no_response": len(events) - len(r_values),
        "complete_n": len(complete),
    }
    for name, *_ in STAGES:
        values = [e[f"{name}_ms"] for e in complete]
        row[f"{name}_mean_ms"] = statistics.fmean(values) if values else None
    # Yorum icin kartin sayaclarindan birkaci.
    for key in ("txq_hwm", "tel_drop", "tel_period_avg_us", "work_avg_us"):
        row[key] = counters.get(key)
    return row


# ---------------------------------------------------------------- dosyalar
def fmt(value) -> str:
    if value is None:
        return ""
    if isinstance(value, float):
        return f"{value:.3f}"
    return str(value)


def write_csv(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="ascii") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(rows[0].keys())
        for row in rows:
            writer.writerow(fmt(v) for v in row.values())


# ---------------------------------------------------------------- grafikler
def style_axes(ax) -> None:
    ax.set_facecolor(SURFACE)
    ax.grid(color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.tick_params(colors=TEXT_SECONDARY, labelsize=8)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)


def plot_r_per_event(by_scenario: dict[str, list[dict]], summary: dict[str, dict], path: Path) -> None:
    fig, axes = plt.subplots(2, 3, figsize=(12, 6.5), layout="constrained")
    fig.set_facecolor(SURFACE)
    for ax, scenario in zip(axes.flat, SCENARIOS):
        style_axes(ax)
        events = by_scenario.get(scenario, [])
        ok = [(e["event_id"], e["r_ms"]) for e in events if e["r_ms"] is not None]
        missing = [e["event_id"] for e in events if e["r_ms"] is None]
        if ok:
            ids, values = zip(*ok)
            ax.plot(ids, values, linestyle="none", marker="o", markersize=5, color=SERIES,
                    markeredgecolor=SURFACE, markeredgewidth=1, label="R (yanıt geldi)", zorder=3)
        if missing:
            ax.plot(missing, [0.0] * len(missing), linestyle="none", marker="x", markersize=7,
                    markeredgewidth=1.5, color=TEXT_PRIMARY, label="yanıt yok (drop)", zorder=3)
        ax.axhline(DEADLINE_MS, color=CRITICAL, linestyle="--", linewidth=1, zorder=2,
                   label=f"sınır {DEADLINE_MS:g} ms")
        top = max([DEADLINE_MS] + [v for _, v in ok]) * 1.15
        ax.set_ylim(0, top)
        ax.set_xlim(0, max(31, len(events) + 1))
        s = summary.get(scenario)
        title = f"{scenario}  ·  n = {s['events']}  ·  aşım {s['deadline_miss']}  ·  yanıt yok {s['no_response']}" if s else f"{scenario}: veri yok"
        if top > DEADLINE_MS * 1.5:
            title += "  (farklı ölçek)"
        ax.set_title(title, loc="left", fontsize=9, color=TEXT_PRIMARY)
        ax.set_xlabel("olay (event_id)", fontsize=8, color=TEXT_SECONDARY)
        ax.set_ylabel("R (ms)", fontsize=8, color=TEXT_SECONDARY)
    handles, labels = [], []
    for ax in axes.flat:
        for h, l in zip(*ax.get_legend_handles_labels()):
            if l not in labels:
                handles.append(h)
                labels.append(l)
    fig.legend(handles, labels, loc="outside upper right", ncol=3, frameon=False, fontsize=9,
               labelcolor=TEXT_SECONDARY)
    fig.suptitle("Buton yanıt süresi R = t4 − t0, olay başına", x=0.01, ha="left",
                 fontsize=12, color=TEXT_PRIMARY)
    fig.savefig(path, dpi=130, facecolor=SURFACE)
    plt.close(fig)


def plot_stage_means(summary: dict[str, dict], path: Path) -> None:
    # S5 digerlerinden ~15 kat buyuk: ayni eksende S0-S4 farklari gorunmez olurdu.
    # Bu yuzden iki panel, her biri kendi olceginde (tek eksen kurali korunur).
    groups = [s for s in SCENARIOS if s in summary]
    left = [s for s in groups if s != "S5"]
    right = [s for s in groups if s == "S5"]
    panels = [(left, "S0–S4")] + ([(right, "S5 (farklı ölçek)")] if right else [])

    fig, axes = plt.subplots(1, len(panels), figsize=(11, 5), layout="constrained",
                             gridspec_kw={"width_ratios": [len(p[0]) for p in panels]}, squeeze=False)
    fig.set_facecolor(SURFACE)
    for ax, (names, panel_title) in zip(axes[0], panels):
        style_axes(ax)
        x = range(len(names))
        bottoms = [0.0] * len(names)
        for (stage, _, _, label), color in zip(STAGES, STAGE_COLORS):
            heights = [summary[s][f"{stage}_mean_ms"] or 0.0 for s in names]
            # 2 px zemin rengi kenar: ust uste binen parcalar ayrisir.
            ax.bar(x, heights, bottom=bottoms, width=0.6, color=color, edgecolor=SURFACE,
                   linewidth=1.5, label=label, zorder=3)
            bottoms = [b + h for b, h in zip(bottoms, heights)]
        top = max(bottoms) * 1.18
        ax.set_ylim(0, top)
        for i, total in enumerate(bottoms):
            ax.annotate(f"{total:.2f} ms", (i, total), xytext=(0, 3), textcoords="offset points",
                        ha="center", va="bottom", fontsize=8, color=TEXT_PRIMARY)
        ax.set_xticks(list(x), [f"{s}\nn = {summary[s]['complete_n']}" for s in names])
        ax.set_title(panel_title, loc="left", fontsize=9, color=TEXT_SECONDARY)
        ax.set_ylabel("ortalama süre (ms)", fontsize=8, color=TEXT_SECONDARY)
    handles, labels = axes[0][0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="outside upper right", ncol=2, frameon=False, fontsize=8,
               labelcolor=TEXT_SECONDARY)
    fig.suptitle("R'nin aşamalara göre ortalaması (yalnızca tamamlanan olaylar)", x=0.01, ha="left",
                 fontsize=12, color=TEXT_PRIMARY)
    fig.savefig(path, dpi=130, facecolor=SURFACE)
    plt.close(fig)


# ---------------------------------------------------------------- ana akis
def main() -> int:
    by_scenario: dict[str, list[dict]] = {}
    summary: dict[str, dict] = {}
    for scenario in SCENARIOS:
        path = MEAS_DIR / f"{scenario}.csv"
        if not path.exists():
            print(f"{scenario}: {path.name} yok, atlandi")
            continue
        events = [event_row(r) for r in load_records(path)]
        by_scenario[scenario] = events
        summary[scenario] = summarize(scenario, events, load_counters(MEAS_DIR / f"{scenario}_counters.csv"))
    if not summary:
        print("Hic olcum dosyasi bulunamadi.")
        return 1

    write_csv(OUT_DIR / "events.csv", [e for s in by_scenario.values() for e in s])
    write_csv(MEAS_DIR / "summary.csv", list(summary.values()))
    PLOT_DIR.mkdir(parents=True, exist_ok=True)
    plot_r_per_event(by_scenario, summary, PLOT_DIR / "r_per_event.png")
    plot_stage_means(summary, PLOT_DIR / "stage_means.png")

    for s in summary.values():
        print(f"{s['scenario']}: n={s['events']} R min/ort/max = {fmt(s['r_min_ms'])} / "
              f"{fmt(s['r_mean_ms'])} / {fmt(s['r_max_ms'])} ms, asim {s['deadline_miss']}, "
              f"yanit yok {s['no_response']}")
    print("Yazildi: analysis/events.csv, measurements/summary.csv, analysis/plots/*.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
