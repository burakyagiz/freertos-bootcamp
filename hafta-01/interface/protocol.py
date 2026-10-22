"""Kartin gonderdigi satirlarin cercevelenmesi ve cozumlenmesi.

GUI'den ve seri porttan bagimsizdir; bu yuzden kart olmadan test edilebilir.

Satir turleri (firmware: App/Inc/app_msg.h ve App/Inc/app_experiment.h):
  TEL,<seq>,S<n>,<t_us>                                64 bayt
  BTN,<event_id>,S<n>,PRESSED                          64 bayt
  CAL,S<n>,n=..,min_us=..,max_us=..,iter_per_ms=..     64 bayt
  REC,S<n>,<event_id>,<t0>,<t1>,<t2>,<t3>,<t4>,<status> serbest uzunluk
  CNT,S<n>,ad=deger,...                                serbest uzunluk
  END,S<n>,rec=<k>                                     serbest uzunluk
64 baytlik satirlar: ASCII metin + bosluk dolgusu (63 bayta kadar) + LF.
Bunlarin disindaki satirlar (orn. acilistaki "Merhaba F407") OTHER olur.
"""
from __future__ import annotations

from dataclasses import dataclass

MSG_LEN = 64                          # LF dahil
FIXED_LEN_KINDS = ("TEL", "BTN", "CAL")
KNOWN_KINDS = FIXED_LEN_KINDS + ("REC", "CNT", "END")
REC_STATUSES = ("ok", "btn_drop", "tx_drop", "tx_error", "timeout", "")  # "": kapanmamis
U32 = 2 ** 32                         # TIM2 32-bit, 71.6 dakikada sifira doner
MAX_LINE_BYTES = 4096                 # bundan uzun LF'siz veri: gurultu, atilir


class LineFramer:
    """Gelen bayt parcalarini LF ile satirlara boler.

    pyserial readline() timeout dolunca yarim satir dondurebilir; bu da tek
    satiri ikiye bolup yanlis "64 bayt degil" hatasi uretirdi. Bu yuzden
    cerceveleme burada, kendi tamponumuzla yapilir.
    """

    def __init__(self) -> None:
        self._buf = bytearray()
        self.discarded_bytes = 0      # LF'siz cok uzun veri (gurultu) yuzunden atilan

    def feed(self, chunk: bytes) -> list[bytes]:
        """Parcayi ekler, tamamlanan satirlari (LF dahil) dondurur."""
        self._buf += chunk
        lines = []
        while True:
            end = self._buf.find(b"\n")
            if end < 0:
                break
            lines.append(bytes(self._buf[: end + 1]))
            del self._buf[: end + 1]
        if len(self._buf) > MAX_LINE_BYTES:
            self.discarded_bytes += len(self._buf)
            self._buf.clear()
        return lines


@dataclass
class Line:
    kind: str          # TEL, BTN, CAL, REC, CNT, END veya OTHER
    text: str          # LF, CR ve sondaki dolgu bosluklari atilmis metin
    raw_len: int       # LF dahil bayt sayisi
    length_ok: bool    # TEL/BTN/CAL: tam 64 bayt ve LF ile bitiyor mu; digerleri: True
    fields: list[str]


def classify(raw: bytes) -> Line:
    """Ham satiri turune ayirir ve 64 bayt kuralini kontrol eder."""
    text = raw.decode("ascii", "replace").rstrip("\r\n").rstrip(" ")
    fields = text.split(",")
    kind = fields[0] if fields[0] in KNOWN_KINDS else "OTHER"
    if kind in FIXED_LEN_KINDS:
        length_ok = len(raw) == MSG_LEN and raw.endswith(b"\n")
    else:
        length_ok = True
    return Line(kind=kind, text=text, raw_len=len(raw), length_ok=length_ok, fields=fields)


def _scenario(value: str) -> str:
    if len(value) < 2 or value[0] != "S" or not value[1:].isdigit():
        raise ValueError(f"senaryo alani hatali: {value!r}")
    return value


def _key_values(items: list[str]) -> dict[str, int | None]:
    """'ad=deger' listesini sozluge cevirir. Bos deger -> None (olculmedi)."""
    result: dict[str, int | None] = {}
    for item in items:
        name, sep, value = item.partition("=")
        if not sep or not name:
            raise ValueError(f"ad=deger bekleniyordu: {item!r}")
        result[name] = int(value) if value != "" else None
    return result


@dataclass
class Record:
    scenario: str
    event_id: int
    t: list[int | None]   # t0..t4 (us); None = eksik zaman (0 DEGIL)
    status: str

    def response_us(self) -> int | None:
        """R = t4 - t0, isaretsiz 32-bit fark. t0 veya t4 yoksa None."""
        if self.t[0] is None or self.t[4] is None:
            return None
        return (self.t[4] - self.t[0]) % U32


def parse_rec(line: Line) -> Record:
    f = line.fields
    if len(f) != 9:
        raise ValueError(f"REC 9 alan olmali, {len(f)} var: {line.text!r}")
    times = [int(v) if v != "" else None for v in f[3:8]]
    if f[8] not in REC_STATUSES:
        raise ValueError(f"bilinmeyen durum: {f[8]!r}")
    return Record(scenario=_scenario(f[1]), event_id=int(f[2]), t=times, status=f[8])


def parse_cnt(line: Line) -> tuple[str, dict[str, int | None]]:
    return _scenario(line.fields[1]), _key_values(line.fields[2:])


def parse_cal(line: Line) -> tuple[str, dict[str, int | None]]:
    return _scenario(line.fields[1]), _key_values(line.fields[2:])


def parse_end(line: Line) -> tuple[str, int]:
    scenario = _scenario(line.fields[1])
    values = _key_values(line.fields[2:])
    if values.get("rec") is None:
        raise ValueError(f"END satirinda rec yok: {line.text!r}")
    return scenario, int(values["rec"])


def parse_tel(line: Line) -> tuple[int, str, int]:
    """(sira no, senaryo, uretim zamani us)"""
    f = line.fields
    if len(f) != 4:
        raise ValueError(f"TEL 4 alan olmali: {line.text!r}")
    return int(f[1]), _scenario(f[2]), int(f[3])


def parse_btn(line: Line) -> tuple[int, str]:
    """(olay kimligi, senaryo)"""
    f = line.fields
    if len(f) != 4 or f[3] != "PRESSED":
        raise ValueError(f"BTN bicimi hatali: {line.text!r}")
    return int(f[1]), _scenario(f[2])
