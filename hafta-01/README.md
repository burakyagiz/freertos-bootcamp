# Hafta 1: FreeRTOS altında buton yanıt süresi analizi

STM32F407 Discovery üzerinde FreeRTOS ile bir buton yanıt sistemi tasarladım.
Butona basıldığı andan (t0) yanıt mesajının son bitinin UART'tan çıktığı ana
(t4) kadar geçen süre (R) altı yük senaryosunda ölçüldü. Ölçümler, gecikmenin
hangi aşamada oluştuğunu gösterecek biçimde analiz edildi. Hedef: R ≤ 20 ms.

## Sonuçlar (n = 30 basış / senaryo)

| Senaryo | Yük | R ort (ms) | Gözlenen R maks (ms) | > 20 ms | Yanıt yok |
|---|---|---|---|---|---|
| S0 | yok | 5,62 | 5,62 | 0 | 0 |
| S1 | 10 Hz telemetri | 5,91 | 10,53 | 0 | 0 |
| S2 | 50 Hz telemetri | 6,35 | 11,04 | 0 | 0 |
| S3 | 100 Hz telemetri | 6,52 | 10,98 | 0 | 0 |
| S4 | 100 Hz + 2 ms CPU | 7,75 | 12,75 | 0 | 0 |
| S5 | 100 Hz + 5 ms CPU | 112,48 | 164,74 | 22 | 7 |

- **S0'da** R'nin ~%99'u 64 baytın hatta gitme süresi.
- **S1–S4'te** artış yanıtın gönderim kuyruğunda beklemesinden (t3−t2);
  S4'te ayrıca CPU beklemesi (t1−t0) ekleniyor.
- **S5'te sistem aşırı yükleniyor.** CPU işi + mesaj süresi periyodu aşıyor;
  gönderim kuyruğu dolup yanıtlar düşüyor.

Ayrıntılar: [analysis/report.md](analysis/report.md).

## Depo yapısı

| Klasör / dosya | İçerik |
|---|---|
| [firmware/](firmware/) | STM32CubeIDE CMake projesi. Uygulama `firmware/App/`, donanım yapılandırması `firmware/Core/` (CubeMX'siz, elle). Sürücüler `firmware/third_party/` altında git submodule |
| [interface/](interface/) | Python PC arayüzü: canlı izleme, 64 bayt kontrolü, CSV kaydı ([README](interface/README.md)) |
| [measurements/](measurements/) | Ham ölçümler `S0..S5.csv` + sayaçlar, `summary.csv`, [ölçüm defteri](measurements/olcum_defteri.md) |
| [analysis/](analysis/) | [Rapor](analysis/report.md), analiz scripti ([README](analysis/README.md)), `plots/` |
| [docs/setup.md](docs/setup.md) | Kurulum, araç sürümleri, saat / zamanlayıcı / NVIC / FreeRTOS ayarları |
| [docs/code-notes.md](docs/code-notes.md) | Kodun ölçümle ilgili parçalarının açıklaması |
| [docs/ai-usage.md](docs/ai-usage.md) | Yapay zekâ kullanım kaydı: faz faz ne yaptırıldı, nasıl doğrulandı |

## Hızlı başlangıç

1. **Depoyu sürücülerle birlikte al:**
   ```
   git clone --recurse-submodules <depo>
   ```
2. **Bağlantı:** USB-TTL (3,3 V) RXD → PA2, GND → GND. Kart mini-USB'den
   beslenir.
3. **Senaryoyu seç:** `firmware/App/Inc/app_config.h` içinde
   `#define SCENARIO 0..5`.
4. **Derle ve yükle:** STM32CubeIDE 2.2'de projeyi aç, **Build**, sonra
   **Run** (debugger'sız).
5. **Ölç:**
   ```
   python -m pip install -r interface/requirements.txt
   python interface/app.py
   ```
   Portu seç → Bağlan → kartta RESET → ısınma (5 s) bitince 30 basış.
   Sonuç `measurements/S<x>.csv` olarak kaydedilir.
6. **Analiz:**
   ```
   python analysis/analyze.py
   ```
   `measurements/summary.csv` ve `analysis/plots/*.png` üretilir.

Testler (kart gerekmez, sentetik veri):
```
python -m unittest discover -s interface/tests
python -m unittest discover -s analysis/tests
```
