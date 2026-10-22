"""Seri portu ayri bir thread'de okur ve satirlari kuyruga koyar.

tkinter thread-safe degildir: bu thread pencereye hic dokunmaz, sadece
queue.Queue'ya olay koyar; GUI kuyrugu kendi thread'inde periyodik okur.

Olcumu etkilememek icin karta HICBIR ZAMAN veri yazilmaz (write cagrisi yok).
"""
from __future__ import annotations

import queue
import threading

import serial

from protocol import LineFramer

BAUDRATE = 115200   # firmware: USART2 115200 8N1


class SerialReader(threading.Thread):
    """Kuyruga koydugu olaylar:
      ("connected", port)          port acildi
      ("line", bytes)              LF dahil tam bir satir
      ("error", mesaj)             port acilamadi / koptu
      ("closed", atilan_bayt)      okuma bitti (her durumda en son gelir)
    """

    def __init__(self, port: str, events: queue.Queue) -> None:
        super().__init__(name=f"SerialReader-{port}", daemon=True)
        self.port = port
        self.events = events
        self._stop_event = threading.Event()

    def stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        framer = LineFramer()
        try:
            # timeout: stop() en gec 0.1 s'de fark edilsin.
            with serial.Serial(self.port, BAUDRATE, timeout=0.1) as port:
                self.events.put(("connected", self.port))
                while not self._stop_event.is_set():
                    chunk = port.read(port.in_waiting or 1)
                    for line in framer.feed(chunk):
                        self.events.put(("line", line))
        except (serial.SerialException, OSError) as exc:
            self.events.put(("error", str(exc)))
        finally:
            self.events.put(("closed", framer.discarded_bytes))
