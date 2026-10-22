# PC arayüzü (Hafta 1)

Kartın UART çıktısını okur, deneyi canlı izler ve deney bitince sonuçları
`measurements/` klasörüne kaydeder. Arayüz karta **hiçbir zaman veri
göndermez**; ölçümü etkilememesi için yalnızca dinler.

## Kurulum

Python 3.10 veya üstü gerekir (tkinter Windows Python kurulumuyla gelir).
`hafta-01` klasöründen:

```
python -m pip install -r interface/requirements.txt
```

## Bağlantı

| USB-TTL (3.3 V) | Kart |
|---|---|
| RXD | PA2 (USART2 TX) |
| GND | GND |

PA3'e kablo gerekmez. Adaptörün 5V/3V3 ucunu bağlama; kart USB'den beslenir.

## Bir senaryoyu ölçmek

1. `firmware/App/Inc/app_config.h` içinde `#define SCENARIO` değerini seç
   (0–5). CubeIDE'de derle ve yükle.
2. Arayüzü başlat: `python interface/app.py`
3. Port listesinden adaptörü seç ("Silicon Labs CP210x"), **Bağlan**.
4. Kartı başlat: siyah **RESET** düğmesine bas ya da debugger'da **Resume**.
   Arayüz `Merhaba F407` ve `CAL` satırlarını görünce yeni oturum başlatır.
5. Durum satırı **"Isınma"** gösterirken (ilk 5 s) butona basma.
6. **"Ölçüm"** yazınca 30 kez bas: aralar en az 0,5 s, ritim düzensiz.
   Her basışta sağdaki alan "Butona basıldı · Olay N" olarak yanıp söner.
7. 30. basıştan ~1 s sonra kart kayıtları gönderir. Arayüz R grafiğini
   çizer ve iki dosya kaydeder:
   - `measurements/S<x>.csv`: her olay için t0..t4 ve durum
   - `measurements/S<x>_counters.csv`: kartın sayaçları + PC kontrolleri

Var olan dosyanın üzerine yazılmaz: `S1.csv` varsa yeni koşu `S1_2.csv`
olur. Rapora hangi koşunun gireceğini sen seçersin.

Kart her resetlendiğinde ekran temizlenir ve yeni oturum başlar.

## Ekranda ne var

| Alan | Kaynak |
|---|---|
| Senaryo, son olay kimliği | TEL/BTN satırları |
| Isınma / Ölçüm / Bitti | Açılıştan beri geçen süre (PC saati, yaklaşık) ve REC/END satırları |
| Telemetri (TEL) | Son TEL satırı ve TEL sayısı |
| Buton (BTN) | "Butona basıldı · Olay N" göstergesi |
| 64 bayt olmayan satır | TEL, BTN ve CAL satırlarının uzunluk kontrolü |
| Grafik | R = t4 − t0 (ms), olay başına; 20 ms deadline çizgisi; yanıtı olmayan olaylar × ile |
| Kayıp / hata | CNT satırındaki btn_drop, tx_drop, tx_error, timeout, tel_drop, rec_overflow |

## Dosya biçimleri

`S<x>.csv`:

```
scenario,event_id,t0_us,t1_us,t2_us,t3_us,t4_us,status
S1,1,<t0>,<t1>,<t2>,<t3>,<t4>,ok
S1,7,<t0>,,,,,btn_drop
```

(Biçim örneği; değerler yer tutucudur.)

- Zamanlar kartın TIM2 sayacının ham değeridir (µs).
- Eksik zaman **boş alandır, 0 değildir**.
- Farkları 32-bit işaretsiz alın: `(t4 - t0) % 2**32`. TIM2 71,6 dakikada
  bir sıfıra döner.

`S<x>_counters.csv` (`scenario,name,value`):

- Kartın CNT satırındaki tüm sayaçlar. Ölçülmemiş değer boştur; örneğin
  S0'da telemetri periyodu.
- `cal_*`: açılıştaki kalibrasyon satırı.
- `pc_*`: arayüzün kendi kontrolleri: alınan TEL/BTN satırı, 64 bayt olmayan
  satır, çözülemeyen satır, END'deki ve gerçekte alınan REC sayısı.

## Notlar

- Satırlar LF ile çerçevelenir. 64 bayt kontrolü yalnızca TEL, BTN ve CAL
  için yapılır. REC, CNT ve END serbest uzunluktadır; ölçüm bittikten sonra
  gönderildikleri için ölçümü etkilemezler (bkz.
  `firmware/App/Inc/app_experiment.h`).
- Seri port ayrı bir thread'de okunur; pencere donmaz.

## Testler

Kart olmadan, sentetik satırlarla:

```
python -m unittest discover -s interface/tests -v
```

## Dosyalar

| Dosya | Görevi |
|---|---|
| `app.py` | Pencere (tkinter + matplotlib) |
| `serial_reader.py` | Seri portu ayrı thread'de okur |
| `protocol.py` | LF çerçeveleme, 64 bayt kontrolü, satır çözümleme |
| `session.py` | Bir deney oturumunun durumu ve özeti |
| `storage.py` | CSV kaydı |
| `tests/` | Birim testleri (sentetik veri) |
