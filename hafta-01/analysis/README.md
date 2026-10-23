# Analiz (Faz 6)

`measurements/S0..S5.csv` dosyalarından aşama sürelerini, senaryo özetini ve
iki grafiği üretir. Her çalıştırmada çıktılar baştan yazılır.

```
python analysis/analyze.py
```

Gereken paket: matplotlib (`interface/requirements.txt` ile aynı kurulum).

## Çıktılar

| Dosya | İçerik |
|---|---|
| `measurements/summary.csv` | Senaryo başına: olay sayısı, durumlara göre sayılar, R min / ort / p95 / max, 20 ms aşımı, yanıt yok, aşama ortalamaları, birkaç kart sayacı |
| `analysis/events.csv` | Her olay için t1−t0, t2−t1, t3−t2, t4−t3 ve R (ms) |
| `analysis/plots/r_per_event.png` | Senaryo başına olay no → R, 20 ms sınır çizgisi |
| `analysis/plots/stage_means.png` | Senaryo başına aşama ortalamaları (yığılmış sütun) |

## Tanımlar

- **Farklar** TIM2'nin 32-bit sayacından, işaretsiz alınır: `(b − a) mod 2^32`.
- **Eksik zaman** (boş alan) 0 sayılmaz; o aralık hesaplanmaz.
- **R = t4 − t0.** t4'ü olmayan olay "yanıt yok" sayılır; sınırı karşılamış
  sayılmaz.
- **20 ms aşımı:** R > 20 ms.
- **Aşama ortalamaları** yalnızca beş zamanı da olan (tamamlanan) olaylardan
  alınır; böylece sütunun toplamı ortalama R'ye eşit olur. S5'te bu 23 olaydır.
- **p95** en yakın sıra yöntemiyle hesaplanır. n = 30'da tek bir olaya
  (29. sıradaki) dayanır; dikkatli yorumla.
- **S5'in değerleri diğerlerinden ~15 kat büyük.** Bu yüzden grafiklerde ayrı
  panelde, kendi ölçeğinde çizilir; başlıkta "farklı ölçek" yazar.

## Testler

Sentetik verilerle:

```
python -m unittest discover -s analysis/tests -v
```
