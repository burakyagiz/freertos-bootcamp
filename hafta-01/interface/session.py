"""Bir deney oturumunun durumu: kartin acilisindan END satirina kadar.

Kart her reset'te once "Merhaba F407", ~0.1 s sonra CAL satirini gonderir;
ikisi de yeni oturum baslatir. GUI'den bagimsizdir; GUI sadece bu nesnenin
durumunu okur.
"""
from __future__ import annotations

import statistics
import time
from dataclasses import dataclass

from protocol import (
    FIXED_LEN_KINDS,
    Line,
    Record,
    classify,
    parse_btn,
    parse_cal,
    parse_cnt,
    parse_end,
    parse_rec,
    parse_tel,
)

# Firmware ve sartnameyle ayni olmali (firmware/App/Inc/app_config.h, CLAUDE.md).
WARMUP_S = 5.0          # EXPERIMENT_WARMUP_MS
TARGET_EVENTS = 30      # EXPERIMENT_TARGET_EVENTS
DEADLINE_MS = 20.0      # R = t4 - t0 <= 20 ms

BOOT_GREETING = "Merhaba"


@dataclass
class Summary:
    """REC satirlarindan cikan ozet (deney bitince)."""
    total: int                 # REC satiri sayisi
    with_response: int         # R hesaplanabilen (t0 ve t4 var)
    r_min_ms: float | None
    r_avg_ms: float | None
    r_max_ms: float | None
    deadline_miss: int         # R > 20 ms
    no_response: int           # t4 yok: drop / hata / timeout
    status_counts: dict[str, int]


class Session:
    def __init__(self, clock=time.monotonic) -> None:
        self._clock = clock
        self.generation = 0     # her yeni oturumda artar; GUI ekrani temizlemek icin bakar
        self.reset()

    def reset(self) -> None:
        self.generation += 1
        self.started_at: float | None = None    # PC saatiyle acilis (Merhaba/CAL) ani
        self.scenario: str | None = None
        self.tel_lines = 0
        self.last_tel: tuple[int, int] | None = None   # (sira no, kart zamani us)
        self.btn_lines = 0
        self.last_event_id: int | None = None
        self.bad_length = {kind: 0 for kind in FIXED_LEN_KINDS}
        self.parse_errors = 0
        self.last_error: str | None = None
        self.cal: dict[str, int | None] = {}
        self.records: list[Record] = []
        self.counters: dict[str, int | None] = {}
        self.end_rec: int | None = None

    @property
    def finished(self) -> bool:
        return self.end_rec is not None

    def _has_content(self) -> bool:
        return bool(self.cal or self.tel_lines or self.btn_lines or self.records)

    def _start(self) -> None:
        if self._has_content():
            self.reset()
        if self.started_at is None:
            self.started_at = self._clock()

    def feed(self, raw: bytes) -> Line:
        """Bir ham satiri (LF dahil) isler ve cozulmus halini dondurur."""
        line = classify(raw)

        if line.kind == "OTHER":
            if line.text.startswith(BOOT_GREETING):
                self._start()
            return line

        # Port acilisi Merhaba'yi kacirdiysa CAL de oturum baslangicidir.
        if line.kind == "CAL":
            self._start()

        if not line.length_ok:
            self.bad_length[line.kind] += 1

        try:
            if line.kind == "TEL":
                seq, scenario, t_us = parse_tel(line)
                self.tel_lines += 1
                self.last_tel = (seq, t_us)
            elif line.kind == "BTN":
                event_id, scenario = parse_btn(line)
                self.btn_lines += 1
                self.last_event_id = event_id
            elif line.kind == "CAL":
                scenario, self.cal = parse_cal(line)
            elif line.kind == "REC":
                record = parse_rec(line)
                scenario = record.scenario
                self.records.append(record)
            elif line.kind == "CNT":
                scenario, self.counters = parse_cnt(line)
            else:  # END
                scenario, self.end_rec = parse_end(line)
            self.scenario = scenario
        except ValueError as exc:
            self.parse_errors += 1
            self.last_error = str(exc)
        return line

    def phase(self) -> str:
        """waiting, warmup, measuring, exporting veya finished."""
        if self.finished:
            return "finished"
        if self.records or self.counters:
            return "exporting"
        if self.started_at is None:
            return "waiting" if not self.tel_lines else "measuring"
        if self._clock() - self.started_at < WARMUP_S:
            return "warmup"
        return "measuring"

    def warmup_left_s(self) -> float:
        if self.started_at is None:
            return 0.0
        return max(0.0, WARMUP_S - (self._clock() - self.started_at))

    def end_matches(self) -> bool:
        """END satirindaki REC sayisi ile alinan REC sayisi ayni mi."""
        return self.end_rec == len(self.records)

    def summary(self) -> Summary:
        responses = [r.response_us() for r in self.records]
        values_ms = [us / 1000.0 for us in responses if us is not None]
        status_counts: dict[str, int] = {}
        for record in self.records:
            key = record.status or "(bos)"
            status_counts[key] = status_counts.get(key, 0) + 1
        return Summary(
            total=len(self.records),
            with_response=len(values_ms),
            r_min_ms=min(values_ms) if values_ms else None,
            r_avg_ms=statistics.fmean(values_ms) if values_ms else None,
            r_max_ms=max(values_ms) if values_ms else None,
            deadline_miss=sum(1 for v in values_ms if v > DEADLINE_MS),
            no_response=sum(1 for us in responses if us is None),
            status_counts=status_counts,
        )
