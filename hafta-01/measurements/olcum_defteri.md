# Ölçüm defteri: S0–S5

Bu dosyayı ölçüm sırasında sen doldur. Sayılar CSV dosyalarında zaten var;
buradaki amaç her koşunun koşullarını ve senin gözlemlerini kayda geçirmek.
Raporun "ölçüm koşulları" bölümü buradan yazılacak.

## Genel koşullar (bir kez doldur)

| Alan | Değer |
|---|---|
| Ölçüm tarihi | 2026-10-22, 21:41–22:03 (dosya kayıt saatleri) |
| Kart | STM32F407 Discovery |
| Firmware | commit `00d4316` (yerel değişiklik: yalnızca açıklama yorumları), Debug derleme (-O0), kalibrasyon 7976 iter/ms |
| Yükleme | CubeIDE **Run** (debugger bağlı değil) |
| UART | USB-TTL (CP210x), 115200 8N1, PA2 → RXD, GND |
| PC arayüzü | `python interface/app.py` |
| Basış kuralı | Isınmada basma; 30 basış, aralar ≥ 0,5 s, düzensiz ritim |

## Her senaryoda görmen gereken (kontrol için)

| Senaryo | `SCENARIO` | Arayüzde TEL sayacı | Turuncu LED (PD13) |
|---|---|---|---|
| S0 | 0 | Hiç artmaz (telemetri kapalı) | Sönük |
| S1 | 1 | ~10 / s artar | Sönük |
| S2 | 2 | ~50 / s artar | Sönük |
| S3 | 3 | ~100 / s artar | Sönük |
| S4 | 4 | ~100 / s artar | Soluk yanar |
| S5 | 5 | ~100 / s artar | S4'ten parlak |

Bunlardan biri tutmuyorsa koşuyu bitirmeden dur ve bana yaz.

## Sonuçlar (her koşudan sonra doldur)

Tablo 2026-10-22'de Claude tarafından `S<x>.csv` ve `S<x>_counters.csv`
dosyalarından dolduruldu. "Doğru senaryo" sütunu, kartın gönderdiği
satırlardaki senaryo alanına ve sayaçlara göre kontrol edildi:
- TEL periyodu S1'de 100, S2'de 20, S3–S5'te 10 ms.
- S0'da hiç TEL yok.
- İş süresi S4'te 2002 µs, S5'te 5011 µs.

S5'in gözlem notunu kullanıcı bildirdi; diğer senaryolarda kullanıcı gözlemi
(örneğin LED durumu) kaydedilmedi. "TEL'e denk gelen" = t3−t2 > 1 ms;
"CPU bekleyen" = t1−t0 > 0,1 ms.

| Senaryo | Tarih-saat | Ekranda doğru senaryo? | R min / ort / max (ms) | 20 ms aşımı | Yanıt yok | 64 B dışı / çözülemeyen | Kaydedilen dosya | Gözlem / not |
|---|---|---|---|---|---|---|---|---|
| S0 | 2026-10-22 21:41 | Evet (S0) | 5,61 / 5,62 / 5,62 | 0 | 0 | 0 / 0 | S0.csv | TEL yok; 30/30 ok |
| S1 | 2026-10-22 21:49 | Evet (S1) | 5,61 / 5,91 / 10,53 | 0 | 0 | 0 / 0 | S1.csv | 2 basış TEL'e denk geldi |
| S2 | 2026-10-22 21:51 | Evet (S2) | 5,61 / 6,35 / 11,04 | 0 | 0 | 0 / 0 | S2.csv | 5 basış TEL'e denk geldi |
| S3 | 2026-10-22 21:55 | Evet (S3) | 5,61 / 6,52 / 10,98 | 0 | 0 | 0 / 0 | S3.csv | 10 basış TEL'e denk geldi |
| S4 | 2026-10-22 21:58 | Evet (S4) | 5,61 / 7,75 / 12,75 | 0 | 0 | 0 / 0 | S4.csv | 18 basış TEL'e denk geldi, 3'ü CPU bekledi; txq_hwm 2 |
| S5 | 2026-10-22 22:03 | Evet (S5) | 15,33 / 112,48 / 164,74 (n=23) | 22 | 7 (tx_drop) | 0 / 0 | S5.csv | Aşırı yük: txQ doldu (txq_hwm 16), 7 BTN yanıtı ve 9 TEL düştü. Kullanıcı gözlemi: arayüzde basış sayacı birkaç basış boyunca artmadı, sonra son olay numarası atladı. Ekran görüntüleri alındı. |

Bir koşuyu tekrarlarsan (yanlış senaryo, yarıda kalma vb.) yeni dosya
`S<x>_2.csv` adıyla kaydedilir. "Gözlem / not" sütununa hangisinin resmi
olduğunu ve nedenini yaz; dosyaları kendin silme.
