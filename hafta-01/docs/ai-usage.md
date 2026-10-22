# AI kullanım kaydı

Araç: Claude Code (model: Claude Opus 5.5). Her faz: ne yaptırıldı, nasıl doğrulandı, neyi düzelttim/düzeltildi.

## Faz 0b — Proje iskeleti (CubeMX'siz) · 2026-10-03/04 · commit `30d106c`

- **Yaptırılan:** Sürücüler sabit etiketli git submodule olarak eklendi (HAL v1.8.5, cmsis-device-f4 v2.6.11, cmsis-core v5.6.0_cm4, FreeRTOS V11.3.1). CubeMX'in üreteceği her şey `Core/` altında elle yazıldı: saat 168 MHz, GPIO, USART2 + DMA, TIM2, TIM6 timebase, NVIC, FreeRTOSConfig. `docs/setup.md` CubeMX karşılık tablolarıyla hazırlandı.
- **Doğrulama:**
  - AI tarafı: komut satırından `-Wall -Werror` ile 0 uyarı derleme; ELF'te vektör tablosu ve handler eşlemesi kontrolü.
  - Benim tarafım (kartta): LED 1 Hz yanıp söndü; debugger'da `SystemCoreClock` 16 MHz'den 168 MHz'e çıktı; `HAL_UART_Transmit` sonrası satıra ulaşıldı.
  - "Merhaba" PC'de görülemedi: ST-LINK sanal COM portu (COM5) PA2'ye bağlı değil. (2026-10-12'de USB-TTL dönüştürücüyle görüldü, bkz. Faz 2.)
- **Düzeltilenler:**
  - AI, kod yorumlarına doğrulanmamış datasheet tablo numaraları yazmıştı; bölüm adlarıyla değiştirildi.
  - "FreeRTOS NVIC grubunu scheduler başlarken kontrol eder" iddiası yanlıştı. FreeRTOS kaynağına bakılarak düzeltildi: kontrol `vPortValidateInterruptPriority` içinde yapılıyor.

## Faz 1 — Zaman kaynağı ve buton ISR · 2026-10-04 · commit `344bf3c`

- **Yaptırılan:** `App/app_time` (`timer_us`, taşma güvenli `elapsed_us`) ve `App/app_button` (ISR'da t0, ayrı "ilk olay" durumlu 30 ms sabit pencereli filtre, `buttonQ`'ya `xQueueSendFromISR`, sayaçlar, drop log) yazıldı. Geçici test görevi her olayda PD12'yi toggle ediyor.
- **Doğrulama (kartta, Live Expressions):**
  - Her basışta LED tek değişim yaptı; 10 hızlı basışta `accepted` tam +10 arttı.
  - 48 basışta `filtered = 6`: buton gerçekten zıplıyor, filtre gerekli.
  - `btn_drop = 0` (normal modda beklenen). Kuyruk-dolu testi henüz yapılmadı.
- **Düzeltilenler / öğrendiklerim:**
  - Debugger `TIM2->CNT` ifadesini tanımadı, çünkü `TIM2` bir makro ve proje `-g` ile derleniyor. Yerine `htim2.Instance->CNT` kullanıldı.
  - Kod değişince eski bir breakpoint boş satıra düştü ve debugger onu bir sonraki kod satırına (`MX_GPIO_Init`) kaydırdı. Ders: testten sonra breakpoint'leri temizle.
  - AI'ın main.c'ye yazdığı "BASEPRI'yi kaldırır" yorumu ters anlamlıydı ("kurar/maskeler" olmalı); derlemeden önce düzeltildi.

## Faz 2 — Görev mimarisi ve kuyruklar · 2026-10-12 · commit `d3d1bee`

- **Yaptırılan:** TelemetryTask (p3), ButtonTask (p2), UartTxTask (p1); `buttonQ` 8, `txQ` 16. 64 bayt `TxMsg` biçimleyici, 64 slotlu kayıt havuzu (alan başına tek yazar, kilitsiz), `app_config.h`'de senaryo/öncelik/stack sabitleri. Önce plan istendi, onaydan sonra yazıldı.
- **Doğrulama:**
  - AI tarafı: 6 senaryonun hepsi `-Werror` ile derlendi; ELF'te `vsnprintf`'in bellek ayırmadığı makine kodundan kontrol edildi.
  - Benim tarafım (kartta): görev durumları FreeRTOS Task List'te beklendiği gibi; S1'de `sent` ~10/s, `tel_drop` 0; `g_records[1]`: t1−t0 12 µs, t2−t1 18 µs, t3−t2 23 µs; stack kullanımı en fazla 576/1024 B.
  - "Merhaba F407" USB-TTL (CP2102) ile PC'de görüldü; CP2102 sürücüsü kurulması gerekti.
- **Düzeltilenler / bulunanlar:**
  - AI planda LED değişiminin ölçülen aralıklara girmediğini söylemişti; ölçüm sonrası LED komutunun t2–t3 içine düştüğü görüldü (~0.1 µs, çözünürlük altında). Kod yorumu düzeltildi.
  - CubeIDE "Stack Usage" sütunu N/A idi; görünümdeki "Toggle Stack Checking" düğmesiyle açıldı.
  - **Faz 1 filtresinde sorun bulundu:** 10–12 basışta 22 olay sayıldı. Basılı tutma testi: basışta hep +1, bırakmada 5'te 2 kez +1, 5 hızlı dokunuşta +7–8. Bırakma zıplaması 30 ms pencereden sonra yükselen kenar üretiyor. Faz 1'deki "10 basışta +10" sonucu bu sorunu yakalamamıştı.

## Faz 1 düzeltmesi — Bırakma zıplaması filtresi · 2026-10-12

- **Sorun (Faz 2 testinde bulundu):** Bırakma zıplaması 30 ms pencereden sonra yükselen kenar üretip sahte basış sayılıyordu (5 bırakmada 2, 5 hızlı dokunuşta 7–8 olay).
- **Seçenekler:** AI üç yol sundu: firmware'e bırakma şartı eklemek, sahteleri analizde ayıklamak, 30 ms'yi büyütmek. Ben birinciyi seçtim. Şartnamedeki 30 ms kuralı aynen kaldı; üstüne şu kural eklendi: kabul edilen basıştan sonra buton 30 ms kesintisiz bırakılmış (TIM6 ile 1 ms örnekleme + yeni kenar yok) görülmeden yeni basış kabul edilmiyor. Elenen kenarlar `release_glitch` sayacında görünüyor. **Bu, sabit deney koşuluna yapılmış bir ektir; raporda gerekçesiyle belirtilecek.**
- **Doğrulama (kartta):** Basılı tutma 5/5 (basışta +1, bırakınca +0), 5 hızlı dokunuş +5, 10 normal basış +10. Son değerler: `rise_edges` 44 = `accepted` 28 + `filtered` 0 + `release_glitch` 16; `releases` 28 = `accepted`; `rx_btn` 28, drop 0. Bu kartta basışta zıplama görülmedi; fazla kenarların hepsi bırakmada.
- **Düzeltilenler:** AI kendi kod incelemesinde TIM6 kararı ile sayaç artışı arasında nadir bir yarış durumu buldu (buton kesmesi araya girerse basılıyken "bırakıldı" durumuna geçilebiliyordu); kısa bir kritik bölümle kapatıldı.

## Faz 3 — UART gönderimi ve t4 · 2026-10-20

- **Yaptırılan:** UartTxTask mesajı kendi statik tamponuna kopyalıyor, t3'ü yazıyor, `HAL_UART_Transmit_DMA` ile gönderiyor ve `ulTaskNotifyTake` ile en fazla 1 s bekliyor. `HAL_UART_TxCpltCallback` t4'ü yazıp kaydı `ok` ile kapatıyor; `HAL_UART_ErrorCallback` hatayı göreve bildiriyor. Başlatma hatası, hata ve timeout durumlarında kayıt `tx_error`/`timeout` ile kapanıyor, UART `HAL_UART_AbortTransmit` ile durduruluyor. Callback ile timeout'un aynı anda gelmesi durumunda kaydı ortak bir durum değişkeni (`s_tx_state`) kimin kapatacağını belirliyor.
- **Doğrulama:**
  - AI tarafı: HAL kaynağında Grep ile gösterildi: DMA normal modda TxCpltCallback DMA bitişinde değil, USART TC kesmesinde (son bit hattan çıktıktan sonra) çağrılıyor. 6 senaryo `-Werror` ile 0 uyarı derlendi. ELF'te DMA tamponunun SRAM1'de (`0x20000a2c`) olduğu kontrol edildi.
  - Benim tarafım (kartta, S1): 60 satırlık okumada her satır 64 bayt, BTN satırları göründü, TEL numaralarında atlama yok. Sayaçlar: `rx_tel` 6530 + `rx_btn` 18 = `tx_done` 6548; `start_fail`, `tx_error`, `tx_timeout` 0; `accepted` 18 = `rx_btn`. İncelenen kayıt: beş zamanın hepsi dolu, `status` ok, t4−t3 = 5552 µs, R = 5608 µs.
  - t4−t3 için beklenti AI tarafından HAL'in BRR formülünden hesaplandı: gerçek baud 115385, 64 bayt hat süresi 5547 µs. Ölçülen 5552 µs, 115200 ile hesaplanan 5556 µs'den kısa; bu da gerçek baud'un biraz yüksek olduğunu doğruluyor.
  - Test edilmeyenler: timeout ve DMA hata yolları kartta tetiklenmedi, sadece kod incelemesiyle kontrol edildi. Başlatma hatası testi (debugger'da `huart2.gState = 0x21`) yapılmadı.
- **Düzeltilenler / bulunanlar:**
  - AI, HAL kaynağını okurken `HAL_UART_AbortTransmit` içinde bir yarış buldu: abort sırasında gelen DMA TC kesmesi TCIE'yi yeniden açabiliyor. Abort'tan sonra TCIE'nin bir kez daha kapatılmasıyla çözüldü.
  - HAL'in DMA kesme kodu sadece TE hatasında DMA stream'ini kapatıyor, DME hatasında DMA çalışmaya devam ediyor. Bu yüzden hata sonrası DMA'yı her durumda görev durduruyor; aksi halde tampon aktarım sürerken üzerine yazılabilirdi.

## Faz 4 — Kalibrasyon, deney akışı, dışa aktarma · 2026-10-20

- **Yaptırılan:**
  - Açılışta CPU işi kalibrasyonu (`CAL` satırı) ve her periyotta iş süresi ölçümü. Telemetri periyodu (min/ort/max) ve txQ yüksek su seviyesi ölçülüyor.
  - Deney akışı (yeni `app_experiment.c`): 5 s ısınma, 30 kabulde otomatik durma, ardından `REC`/`CNT`/`END` satırlarıyla dışa aktarma.
  - Durdurma zinciri: ButtonTask son olaydan 1 s sonra txQ'ya EXPORT işareti koyuyor. FIFO sayesinde işaret geldiğinde tüm kayıtlar kapanmış oluyor; telemetri ancak o zaman durduğu için son olay da aynı yük altında ölçülüyor.
  - S4/S5 kalibrasyon değerleri girilmeden derlenmiyor.
- **Kararlar (ben):**
  - Şartnamede durdurma tetikleyicisi yok; `PROMPTLAR.md`'deki "3 s basılı tutma / UART komutu" yalnızca örnekti. AI kaynakları kontrol edip gösterdi. Ben sadece 30'da otomatik durmayı seçtim.
  - Arayüzden "Bitir" komutu düşünüldü ama seçilmedi: UART alma, PA3 kablosu ve alma/gönderme hatası ayrımı gerektiriyordu, ayrıca Faz 5'teki "ölçüm sırasında MCU'ya veri gönderme" kuralıyla sınırdaydı.
  - CMakeLists'e yeni dosya eklenmesine izin verdim.
- **Doğrulama:**
  - AI tarafı:
    - S0–S3 `-Werror` ile 0 uyarı derlendi. S4/S5 iterasyon girilmeden `#error` verdi, girildikten sonra 0 uyarı derlendi.
    - `-fstack-usage` ile dışa aktarma yolunun yığın kullanımı ölçüldü (`format_cnt` 352 B, toplam ~590 B + `vsnprintf`).
  - Benim tarafım, kartta, S1 tam deney (debugger bağlı):
    - 30. basışta otomatik durdu; 30 `REC` (hepsi `ok`, beş zaman dolu), `CNT` ve `END,S1,rec=30` geldi.
    - 776 TEL/BTN/CAL satırının hepsi 64 bayt.
    - `rise_edges` 44 = 30 kabul + 14 `release_glitch`. Drop, hata, timeout ve kayıt taşması 0; `txq_hwm` 1.
    - Telemetri periyodu min/ort/max = 100000 µs.
    - R min/ort/max = 5612 / 5728 / 7542 µs. İki olayda t3−t2 1431 ve 1953 µs: basış, gönderilmekte olan bir TEL satırına denk geldi.
  - Kalibrasyon: n=100000, min 12537 µs, max 12539 µs → 7976 iter/ms. Buradan S4 = 15952, S5 = 39880 iterasyon.
  - S4/S5 (Live Expressions): ortalama iş süresi S4'te 2002 µs, S5'te 5011 µs. İki testte de ilk saniyelerdeki basış `warmup_ignored = 1` olarak sayıldı.
  - Test edilmeyenler: dışa aktarmada `btn_drop`/`tx_drop`/`timeout` durumları ve boş zaman alanı (bu testte hepsi `ok` çıktı), kayıt taşması. Bunlar sadece kod incelemesiyle kontrol edildi.
- **Düzeltilenler / bulunanlar:**
  - AI kendi incelemesinde iki sorun buldu ve test öncesi düzeltti:
    - CNT satırı en kötü durumda ~560 bayt olabiliyor, 512 baytlık tampon yetmezdi → tampon 640 bayt yapıldı.
    - Dışa aktarma yolu 1 KB'lık UartTxTask yığınını zorluyordu → yığın 2 KB yapıldı.
  - S1 testinde ısınma denenemedi: ilk basış 26. saniyedeydi, `rise_edges` ısınmada hiç kenar olmadığını gösterdi. S4/S5 turunda tekrar test edildi.
  - AI log'daki satır uzunluklarını ilk kontrolünde yanlış saydı (Windows `\r\n` sonları); düzeltip tekrar saydı.

## Faz 5 — PC arayüzü · 2026-10-20/22

- **Yaptırılan:** `interface/` (Python + pyserial + tkinter + matplotlib).
  - Seri port ayrı bir thread'de okunuyor; satırlar LF ile, kendi tamponumuzla ayrılıyor.
  - TEL/BTN/CAL satırları için 64 bayt kontrolü yapılıyor.
  - Pencerede TEL ve BTN ayrı alanlarda; "Butona basıldı · Olay N" göstergesi ve ısınma geri sayımı var.
  - Deney bitince R grafiği (20 ms çizgisiyle) çiziliyor, kayıp/hata sayaçları gösteriliyor ve iki dosya yazılıyor: `measurements/S<x>.csv` ve `S<x>_counters.csv`.
  - Var olan dosyanın üzerine yazılmıyor (S1_2.csv). Arayüz karta hiç veri göndermiyor.
  - Ayrıca `requirements.txt`, `README.md` ve sentetik verili birim testleri.
- **Kararlar (ben):**
  - Senaryonun arayüzden seçilmesini sordum. Yapılmadı: şartname senaryonun derleme anında seçilmesini istiyor ve seçim PC'den karta veri göndermeyi gerektirirdi.
  - Deneme S1 koşusunun resmi ölçüm olup olmayacağına henüz karar vermedim; commit'e girmedi.
- **Doğrulama:**
  - AI tarafı:
    - 12 birim testi geçti (sentetik veri).
    - Faz 4'teki gerçek S1 log'u gizli bir pencereye beslendi: 745 TEL, 30 BTN ve 30 REC satırının hepsi alındı; END sayısı eşleşti; 64 bayt hatası 0. R min/ort/max = 5.61 / 5.73 / 7.54 ms, Faz 4'teki elle hesapla aynı.
    - Üretilen grafik görüntülenip kontrol edildi.
  - Benim tarafım, kartta, S1, debugger bağlı değil:
    - Isınmadaki 2–3 basış kaydedilmedi (`warmup_ignored` 3).
    - 30 basıştan sonra grafik çizildi ve iki dosya kaydedildi: 30 REC'in hepsi `ok`, 64 bayt hatası 0, R 5613–5623 µs.
    - Bu koşuda hiçbir basış bir TEL satırına denk gelmedi. Basışlar rastgele zamanlıysa bunun olasılığı yaklaşık %18.
    - Kes, tekrar Bağlan ve pencereyi kapatma sorunsuz çalıştı.
- **Düzeltilenler / bulunanlar:**
  - AI kendi testinde, uzun dosya yolları yüzünden pencerenin 2000 piksel genişliğe çıktığını gördü; uzun etiketlere satır kaydırma eklendi.
  - README'deki biçim örneğinde uydurma bir değer vardı; gerçek veri gibi görünmesin diye yer tutucularla değiştirildi.

## Ölçümler S0–S5 · 2026-10-22

- **Yaptırılan:** Ölçüm defteri şablonu (`measurements/olcum_defteri.md`). Altı koşudan sonra defterin sonuç tablosu, CSV ve sayaç dosyalarından AI tarafından dolduruldu; S5'teki davranış verilerle açıklandı.
- **Ölçümleri ben yaptım:** debugger bağlı değilken, arayüzle, her senaryoda 30 basış.
- **Doğrulama (AI, dosyalardan):**
  - Her dosyada 30 REC ve doğru senaryo alanı var.
  - Senaryo ayarları sayaçlarla tutarlı: TEL periyodu 100 / 20 / 10 ms, S0'da TEL yok, iş süresi S4'te 2002 µs, S5'te 5011 µs.
  - 64 bayt hatası 0, çözülemeyen satır 0.
  - t4−t3 her senaryoda 5.55–5.56 ms; beklendiği gibi yükten bağımsız.
- **Bulunanlar / düzeltilenler:**
  - **AI'ın S5 hipotezi yanlıştı.** Ölçümden önce en kötü durum için R ≈ 20.6 ms tahmin edilmişti. Ölçülen: R 165 ms'ye kadar çıktı, 7 BTN yanıtı düştü (`tx_drop`), txQ doldu (`txq_hwm` 16).
  - Neden: S5'te her 10 ms'nin 5 ms'si CPU işi. Bir mesaj hatta 5.56 ms sürdüğü için kalan 5 ms'lik boşlukta bitemiyor ve UartTxTask periyot başına ancak 1 mesaj başlatabiliyor. Bu da telemetrinin ürettiği mesaj sayısına eşit. Hatta boş kapasite kalmadığı için her BTN yanıtı kuyrukta kalıcı ~10 ms'lik birikim ekledi: t3−t2 basış başına ~10 ms arttı (9.7, 15.5, 30, 40 … ms). Kuyruk dolunca yeni yanıtlar düştü.
  - Tahmin, kuyruğun boş olduğunu varsayıyordu ve bu birikimi hesaba katmamıştı.
