"""analyze.py hesaplari icin birim testleri.

TUM VERILER SENTETIKTIR (elle yazilmis ornek kayitlar); gercek olcum degildir.
Calistirma (hafta-01 klasorunden):  python -m unittest discover -s analysis/tests -v
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from analyze import diff_us, event_row, p95_nearest_rank, summarize  # noqa: E402


def rec(event_id, times, status="ok"):
    return {"scenario": "S9", "event_id": event_id, "status": status, "t": times}


class DiffTest(unittest.TestCase):
    def test_unsigned_wrap(self):
        self.assertEqual(diff_us(2 ** 32 - 100, 50), 150)

    def test_missing_is_none_not_zero(self):
        self.assertIsNone(diff_us(None, 5))


class SummaryTest(unittest.TestCase):
    def setUp(self):
        self.events = [
            event_row(rec(1, [0, 10, 30, 60, 5_560])),           # R 5.56 ms
            event_row(rec(2, [0, 10, 30, 20_000, 25_560])),      # R 25.56 ms > 20
            event_row(rec(3, [0, 10, None, None, None], "tx_drop")),
        ]
        self.row = summarize("S9", self.events, {"txq_hwm": 3})

    def test_counts(self):
        r = self.row
        self.assertEqual((r["events"], r["ok"], r["tx_drop"], r["r_n"]), (3, 2, 1, 2))
        self.assertEqual((r["deadline_miss"], r["no_response"], r["complete_n"]), (1, 1, 2))
        self.assertEqual(r["txq_hwm"], 3)

    def test_stage_means_add_up_to_mean_r(self):
        r = self.row
        stages = sum(r[f"{n}_mean_ms"] for n in ("t1_t0", "t2_t1", "t3_t2", "t4_t3"))
        self.assertAlmostEqual(stages, r["r_mean_ms"])

    def test_p95_nearest_rank(self):
        self.assertEqual(p95_nearest_rank([float(v) for v in range(1, 31)]), 29.0)


if __name__ == "__main__":
    unittest.main()
