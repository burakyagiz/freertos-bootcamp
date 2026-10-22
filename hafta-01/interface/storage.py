"""Oturum sonuclarini measurements/ klasorune kaydeder.

  <S>.csv           REC satirlari, CLAUDE.md'deki baslikla. Eksik zaman bos alan.
  <S>_counters.csv  CNT sayaclari + CAL degerleri + PC tarafinin kendi sayaclari.

Var olan dosyanin uzerine ASLA yazilmaz: S1.csv varsa S1_2.csv,
S1_2_counters.csv kullanilir. Hangi kosunun resmi olcum oldugunu sen secersin.
"""
from __future__ import annotations

import csv
from pathlib import Path

from session import Session

CSV_HEADER = ["scenario", "event_id", "t0_us", "t1_us", "t2_us", "t3_us", "t4_us", "status"]
COUNTER_HEADER = ["scenario", "name", "value"]


def _free_stem(out_dir: Path, base: str) -> str:
    """Iki dosyanin da (csv ve counters) bos oldugu ilk adi bulur."""
    stem, index = base, 1
    while (out_dir / f"{stem}.csv").exists() or (out_dir / f"{stem}_counters.csv").exists():
        index += 1
        stem = f"{base}_{index}"
    return stem


def _cell(value: int | None) -> str:
    return "" if value is None else str(value)   # eksik deger: bos, 0 degil


def save_session(session: Session, out_dir: Path) -> tuple[Path, Path]:
    """REC ve sayac dosyalarini yazar, yollarini dondurur."""
    out_dir.mkdir(parents=True, exist_ok=True)
    scenario = session.scenario or "S_bilinmiyor"
    stem = _free_stem(out_dir, scenario)
    rec_path = out_dir / f"{stem}.csv"
    cnt_path = out_dir / f"{stem}_counters.csv"

    with rec_path.open("w", newline="", encoding="ascii") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(CSV_HEADER)
        for r in session.records:
            writer.writerow([r.scenario, r.event_id, *(_cell(t) for t in r.t), r.status])

    rows: list[tuple[str, int | None]] = list(session.counters.items())
    # CAL satiri: CNT'de olmayanlar (cal_iter_per_ms zaten CNT'de var).
    for name, value in session.cal.items():
        key = f"cal_{name}"
        if key not in session.counters:
            rows.append((key, value))
    # PC tarafinin kendi kontrolleri.
    rows += [
        ("pc_tel_lines", session.tel_lines),
        ("pc_btn_lines", session.btn_lines),
        *((f"pc_bad_length_{kind.lower()}", n) for kind, n in session.bad_length.items()),
        ("pc_parse_errors", session.parse_errors),
        ("pc_end_rec", session.end_rec),
        ("pc_rec_received", len(session.records)),
    ]
    with cnt_path.open("w", newline="", encoding="ascii") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(COUNTER_HEADER)
        for name, value in rows:
            writer.writerow([scenario, name, _cell(value)])

    return rec_path, cnt_path
