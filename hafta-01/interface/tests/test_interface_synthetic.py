"""protocol, session ve storage icin birim testleri.

TUM VERILER SENTETIKTIR (elle yazilmis ornek satirlar); gercek olcum degildir.
Calistirma (hafta-01 klasorunden):  python -m unittest discover -s interface/tests -v
"""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from protocol import LineFramer, classify, parse_rec  # noqa: E402
from session import Session  # noqa: E402
import storage  # noqa: E402


def pad64(text: str) -> bytes:
    """Firmware'in yaptigi gibi: metin + bosluk (63 bayta) + LF."""
    return (text.ljust(63) + "\n").encode("ascii")


def free(text: str) -> bytes:
    """Serbest uzunluklu satir (REC/CNT/END)."""
    return (text + "\n").encode("ascii")


class FakeClock:
    def __init__(self) -> None:
        self.now = 100.0

    def __call__(self) -> float:
        return self.now


SYNTHETIC_SESSION = [
    b"Merhaba F407\r\n",
    pad64("CAL,S1,n=100000,min_us=12000,max_us=12010,iter_per_ms=8333"),
    pad64("TEL,1,S1,200000"),
    pad64("TEL,2,S1,300000"),
    pad64("BTN,1,S1,PRESSED"),
    pad64("TEL,3,S1,400000"),
    pad64("BTN,2,S1,PRESSED"),
    free("REC,S1,1,1000,1013,1031,1060,6615,ok"),
    free("REC,S1,2,4294967000,4294967013,4294967031,4294967060,5320,ok"),   # TIM2 sifira dondu
    free("REC,S1,3,9000,,,,,btn_drop"),
    free("CNT,S1,accepted=3,btn_drop=1,tx_drop=0,timeout=0,tel_period_min_us=,cal_iter_per_ms=8333"),
    free("END,S1,rec=3"),
]


class FramerTest(unittest.TestCase):
    def test_lines_split_across_chunks(self):
        framer = LineFramer()
        data = pad64("TEL,1,S1,5") + pad64("BTN,1,S1,PRESSED")
        out = []
        for i in range(0, len(data), 7):          # 7 baytlik parcalarla gelsin
            out += framer.feed(data[i:i + 7])
        self.assertEqual(out, [pad64("TEL,1,S1,5"), pad64("BTN,1,S1,PRESSED")])

    def test_long_garbage_is_dropped(self):
        framer = LineFramer()
        self.assertEqual(framer.feed(b"x" * 5000), [])
        self.assertEqual(framer.discarded_bytes, 5000)
        self.assertEqual(framer.feed(pad64("TEL,1,S1,5")), [pad64("TEL,1,S1,5")])


class ClassifyTest(unittest.TestCase):
    def test_fixed_length_checked_only_for_tel_btn_cal(self):
        self.assertTrue(classify(pad64("TEL,1,S1,5")).length_ok)
        short = ("TEL,1,S1,5".ljust(62) + "\n").encode()
        self.assertFalse(classify(short).length_ok)
        self.assertTrue(classify(free("REC,S1,1,1,2,3,4,5,ok")).length_ok)

    def test_padding_stripped(self):
        self.assertEqual(classify(pad64("BTN,7,S3,PRESSED")).text, "BTN,7,S3,PRESSED")

    def test_unknown_line_is_other(self):
        self.assertEqual(classify(b"Merhaba F407\r\n").kind, "OTHER")


class RecordTest(unittest.TestCase):
    def test_missing_times_are_none_not_zero(self):
        rec = parse_rec(classify(free("REC,S1,3,9000,,,,,btn_drop")))
        self.assertEqual(rec.t, [9000, None, None, None, None])
        self.assertIsNone(rec.response_us())

    def test_response_uses_unsigned_wrap(self):
        rec = parse_rec(classify(free("REC,S1,2,4294967000,1,2,3,5320,ok")))
        self.assertEqual(rec.response_us(), 5320 + (2 ** 32 - 4294967000))

    def test_bad_status_rejected(self):
        with self.assertRaises(ValueError):
            parse_rec(classify(free("REC,S1,1,1,2,3,4,5,unknown")))


class SessionTest(unittest.TestCase):
    def feed_all(self, session, lines):
        for raw in lines:
            session.feed(raw)

    def test_full_session(self):
        clock = FakeClock()
        s = Session(clock=clock)
        self.feed_all(s, SYNTHETIC_SESSION[:2])
        self.assertEqual(s.phase(), "warmup")
        clock.now += 5.1
        self.assertEqual(s.phase(), "measuring")
        self.feed_all(s, SYNTHETIC_SESSION[2:])

        self.assertTrue(s.finished)
        self.assertTrue(s.end_matches())
        self.assertEqual(s.scenario, "S1")
        self.assertEqual((s.tel_lines, s.btn_lines, s.last_event_id), (3, 2, 2))
        self.assertEqual(s.bad_length, {"TEL": 0, "BTN": 0, "CAL": 0})
        self.assertEqual(s.parse_errors, 0)
        self.assertEqual(s.counters["btn_drop"], 1)
        self.assertIsNone(s.counters["tel_period_min_us"])

        sm = s.summary()
        self.assertEqual((sm.total, sm.with_response, sm.no_response), (3, 2, 1))
        self.assertAlmostEqual(sm.r_min_ms, 5.615)
        self.assertEqual(sm.deadline_miss, 0)

    def test_bad_length_counted(self):
        s = Session()
        s.feed(("TEL,1,S1,5".ljust(40) + "\n").encode())
        self.assertEqual(s.bad_length["TEL"], 1)

    def test_board_reset_starts_new_session(self):
        s = Session()
        self.feed_all(s, SYNTHETIC_SESSION)
        generation = s.generation
        s.feed(b"Merhaba F407\r\n")
        self.assertGreater(s.generation, generation)
        self.assertEqual((s.records, s.tel_lines, s.end_rec), ([], 0, None))


class StorageTest(unittest.TestCase):
    def test_csv_format_and_no_overwrite(self):
        s = Session()
        for raw in SYNTHETIC_SESSION:
            s.feed(raw)
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            rec1, cnt1 = storage.save_session(s, out)
            rec2, cnt2 = storage.save_session(s, out)
            self.assertEqual((rec1.name, cnt1.name), ("S1.csv", "S1_counters.csv"))
            self.assertEqual((rec2.name, cnt2.name), ("S1_2.csv", "S1_2_counters.csv"))

            rows = rec1.read_text(encoding="ascii").splitlines()
            self.assertEqual(rows[0], "scenario,event_id,t0_us,t1_us,t2_us,t3_us,t4_us,status")
            self.assertEqual(rows[3], "S1,3,9000,,,,,btn_drop")   # bos alan, 0 degil

            counters = cnt1.read_text(encoding="ascii").splitlines()
            self.assertIn("S1,btn_drop,1", counters)
            self.assertIn("S1,tel_period_min_us,", counters)
            self.assertIn("S1,cal_min_us,12000", counters)
            self.assertIn("S1,pc_bad_length_tel,0", counters)


if __name__ == "__main__":
    unittest.main()
