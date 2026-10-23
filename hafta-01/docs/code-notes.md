# Kod notları

Sistemin ölçümle ilgili parçaları: her biri için kısa bir kod alıntısı ve üç
soru: **ne yapar**, **hangi zaman damgası**, **hangi tuzaktan kaçınır**.
Alıntılar kısaltılmıştır (`...`); tam kod satır numaralarıyla verilen
dosyalarda.

Genel akış:

```
Buton kesmesi (t0) --buttonQ--> ButtonTask (t1, t2) --txQ--> UartTxTask (t3) --DMA--> USART2
                                TelemetryTask (TEL) --txQ--^      TxCpltCallback (t4) <--+
```

Öncelikler: TelemetryTask 3 > ButtonTask 2 > UartTxTask 1. Kesmeler (EXTI0,
USART2, DMA1 Stream6) NVIC önceliği 6.

---

## 1. Buton kesmesi: `HAL_GPIO_EXTI_Callback`

[`firmware/App/Src/app_button.c`](../firmware/App/Src/app_button.c)

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    const uint32_t now = timer_us();                       /* t0: ilk is */
    ...
    if (s_has_last && (elapsed_us(s_last_accept_us, now) < BUTTON_DEBOUNCE_US))
    { g_button_stats.filtered++; return; }                 /* kural 1: 30 ms */
    if (s_has_last && (g_button_stats.releases == s_releases_at_accept))
    { g_button_stats.release_glitch++; return; }           /* kural 2: birakma sarti */
    ...
    if (!experiment_warmup_done())   { g_button_stats.warmup_ignored++; return; }
    if (experiment_stop_requested()) { g_button_stats.late_ignored++;  return; }

    const ButtonEvent event = { .id = s_next_id++, .t0_us = now };
    record_open(event.id, now);
    if (xQueueSendFromISR(s_button_queue, &event, &woken) != pdPASS)
    { record_close(event.id, REC_STATUS_BTN_DROP); g_button_stats.btn_drop++; }
    ...
    portYIELD_FROM_ISR(woken);
}
```

**Ne yapar:**
- Kesme zinciri: vektör tablosu → `EXTI0_IRQHandler` → HAL'in
  `HAL_GPIO_EXTI_IRQHandler` fonksiyonu (bayrağı temizler) → bu callback.
  HAL'de bu fonksiyon `__weak` tanımlıdır; aynı adla yazılan sürüm onun yerine
  geçer.
- Kenarı filtreler, olaya numara verir, kayıt havuzunda yer açar ve olayı
  `buttonQ`'ya koyar. Ağır işler ButtonTask'e bırakılır (ertelenmiş kesme
  işleme).

**Zaman damgası:** t0, fonksiyonun ilk satırı. Filtre kararı da aynı değerle
verilir; zaman ikinci kez okunmaz.

**Kaçındığı tuzaklar:**
- **NVIC önceliği 6 (≥ 5).** `FromISR` çağrısı yaptığı için 5'ten küçük
  olsaydı FreeRTOS'un öncelik kontrolüne takılırdı.
- **Kesmede bekleme, `printf` ya da UART kullanımı yok.** Kuyruk doluysa
  bekleyemez; olay kaybolmaz, `btn_drop` durumuyla kapanır.
- **Olay kuyruğa pointer olarak değil, değer olarak kopyalanır.**
- **Sabit pencere.** Atılan kenarlar referans zamanı kaydırmaz.
- **İlk olay ayrı ele alınır.** "Henüz kabul yok" durumunda karşılaştırma
  yapılmaz; açılıştan sonraki ilk basış yanlışlıkla atılmaz.
- **Kayıt kuyruğa vermeden önce açılır.** ButtonTask t1'i yazarken slot hazır
  olur.
- **Bırakma şartı (kural 2) şartnameye bir ektir.** Bu kartta bırakma
  zıplaması 30 ms penceresinden sonra kenar üretip sahte basış sayılıyordu.
  Bırakma, TIM6'nın 1 ms'lik kesmesinde (`app_button_tick_1ms`) pin
  örneklenerek izlenir.

## 2. ButtonTask

[`firmware/App/Src/app_button.c`](../firmware/App/Src/app_button.c)

```c
if (xQueueReceive(s_button_queue, &event, wait) != pdPASS)  /* sadece deney sonunda */
{ request_export(); export_requested = true; continue; }
record_set_time(event.id, REC_T1, timer_us());                       /* t1 */
msg_format(&msg, MSG_TYPE_BTN, event.id, "BTN,%" PRIu32 ",S%u,PRESSED", event.id, SCENARIO);
record_set_time(event.id, REC_T2, timer_us());                       /* t2 */
if (xQueueSend(s_tx_queue, &msg, 0) != pdPASS)
{ record_close(event.id, REC_STATUS_TX_DROP); g_button_stats.tx_drop++; }
```

**Ne yapar:**
- Olayı alır, 64 baytlık `BTN,<id>,S<n>,PRESSED` satırını hazırlar
  (metin + boşluk dolgusu + LF) ve `txQ`'ya koyar.
- Ölçüm sırasında bekleme süresi sonsuzdur. 30. basıştan sonra 1 s'ye düşer;
  bu sürede olay gelmezse dışa aktarma işaretini gönderir.

**Zaman damgaları:** t1, olay alındıktan hemen sonra. t2, mesaj kuyruğa
verilmeden hemen önce.

**Kaçındığı tuzaklar:**
- **t2 gönderimden önce yazılır.** Olay `txQ`'ya verildikten sonra kayda
  artık UartTxTask yazar; sıra öncelikler değişse de bozulmaz.
- **`txQ`'ya beklemeden (timeout 0) konur.** Dolu kuyrukta beklemek yerine
  kayıp görünür kılınır (`tx_drop`).
- **Mesaj sabit 64 bayt.** Hat süresi senaryolar arasında değişmez.
- **LED değişimi ve kuyruk doluluğu okuması t2..t3 içine düşer.** Her olayda
  aynıdır ve TIM2'nin 1 µs çözünürlüğünün altında kaldığı tahmin edilir.

## 3. UartTxTask

[`firmware/App/Src/app_uart_tx.c`](../firmware/App/Src/app_uart_tx.c)

```c
memcpy(s_tx_buffer, msg->data, MSG_LEN);           /* DMA durmusken */
...
(void)ulTaskNotifyTake(pdTRUE, 0);                 /* gec kalmis bildirimi sil */
s_tx_state = TX_PENDING;
if (is_btn) record_set_time(event_id, REC_T3, timer_us());     /* t3 */
if (HAL_UART_Transmit_DMA(&huart2, s_tx_buffer, len) != HAL_OK)
{ ... uart_make_safe(); return TX_START_FAIL; }
const TxState result = wait_for_result();          /* ulTaskNotifyTake(.., 1 s) */
if (result != TX_DONE) uart_make_safe();           /* AbortTransmit + TCIE */
```

**Ne yapar:**
- `txQ`'daki mesajları sırayla DMA ile gönderir. Her aktarımda görev
  bildirim gelene kadar (en fazla 1 s) uyur; DMA baytları donanımla taşır.
- Hata ya da timeout olursa UART'ı durdurup kaydı `tx_error` veya `timeout`
  ile kapatır.

**Zaman damgası:** t3, `HAL_UART_Transmit_DMA` çağrısından hemen önce
(yalnızca BTN mesajlarında).

**Kaçındığı tuzaklar:**
- **UART'a tek görev dokunur.** Satırlar karışmaz, mutex gerekmez.
- **DMA tamponu statik ve SRAM'de** (`.bss`, `0x2000xxxx` adres bölgesi).
  DMA1, CCM RAM'e (`0x1000xxxx`) erişemez.
- **Tampon aktarım sürerken değişmez.** Yeni mesaj ancak bir öncekinin bittiği
  ya da DMA'nın durdurulduğu görüldükten sonra kopyalanır. F4'te veri
  önbelleği olmadığı için önbellek temizlemeye gerek yoktur.
- **Sonsuz bekleme yok.** 1 s timeout dolarsa `HAL_UART_AbortTransmit` çağrılır.
- **Timeout ile callback aynı anda gelebilir.** Kaydı hangisinin kapatacağına
  `s_tx_state` karar verir; görev tarafındaki kontrol kısa bir kritik bölümde
  yapılır.
- **HAL'deki bir yarış.** Abort sırasında gelen DMA kesmesi TCIE'yi yeniden
  açabiliyor; abort'tan sonra TCIE bir kez daha kapatılır.
- **En düşük öncelikte çalışır.** Bu yüzden CPU yükü (S4/S5) yeni gönderimin
  başlamasını geciktirir. S5'teki taşmanın nedeni budur (aşağıda 6. bölüm).

## 4. `HAL_UART_TxCpltCallback`

[`firmware/App/Src/app_uart_tx.c`](../firmware/App/Src/app_uart_tx.c)

```c
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    const uint32_t now = timer_us();                 /* t4: ilk is */
    if ((huart->Instance != USART2) || (s_tx_state != TX_PENDING)) return;
    s_tx_state = TX_DONE;
    if (s_tx_is_btn != 0U)
    { record_set_time(s_tx_event_id, REC_T4, now); record_close(s_tx_event_id, REC_STATUS_OK); }
    wake_task_from_isr();                            /* vTaskNotifyGiveFromISR */
}
```

**Ne yapar:** Son bit hattan çıkınca t4'ü yazar, kaydı `ok` ile kapatır ve
UartTxTask'i uyandırır.

**Zaman damgası:** t4, fonksiyonun ilk satırında okunur. Kayda yalnızca BTN
mesajları için yazılır.

**Kaçındığı tuzak: "DMA bitti" anını t4 sanmak.** HAL kaynağında doğrulandı
(`stm32f4xx_hal_uart.c`):
1. DMA son baytı DR'ye yazınca `UART_DMATransmitCplt` çalışır (3019–3028).
   Normal modda callback'i çağırmaz, yalnızca USART'ın TC kesmesini açar.
2. Son stop biti hattan çıkınca `HAL_UART_IRQHandler` TC dalı (2606–2608)
   `UART_EndTransmit_IT`'i (3603–3616) çağırır; callback buradan gelir.

DMA bitişinde ölçülseydi t4 yaklaşık 2 karakter süresi (~0,17 ms) erken
olurdu. Ölçülen t4−t3 = 5,55–5,56 ms; 64 bayt × 10 bit / 115385 baud =
5547 µs hat süresiyle tutarlı.

## 5. Kayıt havuzu

[`firmware/App/Inc/app_record.h`](../firmware/App/Inc/app_record.h)

```c
typedef struct {
    uint32_t event_id;
    uint32_t t_us[REC_T_COUNT];     /* t0..t4 (TIM2, us) */
    uint8_t  has_t[REC_T_COUNT];    /* 0: bos (0 us DEGIL) */
    uint8_t  status;                /* ok, btn_drop, tx_drop, tx_error, timeout */
} Record;
Record g_records[RECORD_POOL_LEN];  /* 64 slot, slot = event_id % 64 */
```

**Ne yapar:** Her kabul edilen olayın zamanlarını ve durumunu RAM'de tutar.
Ölçüm sırasında hiçbir şey yazdırılmaz; kayıtlar deney bitince gönderilir.

**Kim neyi yazar:**

| Alan | Yazan |
|---|---|
| event_id, t0, `btn_drop` | Buton kesmesi |
| t1, t2, `tx_drop` | ButtonTask |
| t3, `tx_error`, `timeout` | UartTxTask |
| t4, `ok` | TxCpltCallback |

**Neden kilit gerekmiyor:**
1. Her alanın tek bir yazarı var.
2. Aynı olayın yazarları zamanda sıralı. Olay kuyruklarla elden ele geçer;
   bir sonraki yazar, önceki işini bitirmeden olayı eline alamaz.
3. Hizalı 32-bit yazma Cortex-M4'te tek komuttur. "Geçerli" bayrakları ayrı
   baytlardır; iki yazar birbirinin bitini silemez.

**Kaçındığı tuzaklar:**
- **Eksik zaman 0 yazılmaz, `has_t = 0` kalır.** Dışa aktarmada boş alan olur.
- **Taşma görünür.** 64 olay sonra hâlâ açık bir slot yeniden kullanılırsa
  `rec_overflow` artar; eski olayın geç gelen yazmaları `event_id` kontrolüyle
  atılır. Deneyde 30 olay olduğu için taşma olmadı.
- **Zaman farkları işaretsiz 32-bit alınır.** TIM2 71,6 dakikada sıfıra dönse
  de doğrudur.

## 6. TelemetryTask: yük

[`firmware/App/Src/app_telemetry.c`](../firmware/App/Src/app_telemetry.c)

```c
(void)xTaskDelayUntil(&last_wake, period);         /* sabit periyot, kayma yok */
if (experiment_telemetry_stopped()) return;
calibrated_work(TELEMETRY_WORK_ITERATIONS);        /* S4: 2 ms, S5: 5 ms */
msg_format(&msg, MSG_TYPE_TEL, seq, "TEL,%" PRIu32 ",S%u,%" PRIu32, seq, SCENARIO, now);
xQueueSend(s_tx_queue, &msg, 0);                   /* dolu: tel_drop */
```

**Ne yapar:** Senaryonun periyodunda önce CPU işi yapar, sonra TEL satırını
`txQ`'ya koyar. S0'da hiç çalışmaz.

**Kalibrasyon:** Açılışta 100 000 iterasyon 8 kez ölçülür ve en kısa turdan
iterasyon/ms hesaplanır (7976). Kesmeler açıktır; ölçülen süre kesme sürelerini
içerebilir. Sonuç yalnızca Debug (-O0) derleme için geçerlidir.

**Kaçındığı tuzaklar:**
- **`vTaskDelay` yük değildir.** İş, sonucu `volatile` değişkene yazılan ve
  optimizasyonla silinemeyen bir döngüdür.
- **Kesmeler kapatılmaz.**
- **S0'da görev boş döngüde dönmez,** bloklu bekler.

**S5'te neden taşma oldu:** Her 10 ms'nin 5 ms'si CPU işidir. UartTxTask
(öncelik 1) bu sırada yeni gönderim başlatamaz. Bir mesaj 5,56 ms sürdüğü için
kalan 5 ms'lik boşlukta bitemez, sonraki ancak iş bitince başlar. Sonuç:
periyotta en fazla 1 mesaj gidebilir, ve bu telemetrinin üretimine eşittir.
Her BTN yanıtı kuyrukta kalıcı ~10 ms'lik birikim ekler.

## 7. Deney akışı

[`firmware/App/Inc/app_experiment.h`](../firmware/App/Inc/app_experiment.h)

- **Isınma:** Scheduler başladıktan sonraki 5 s boyunca basışlar kaydedilmez
  (`warmup_ignored`). Süre FreeRTOS tick sayacından okunur.
- **Durdurma:**
  1. 30. kabulde kesme durdurma ister; sonraki basışlar sayılır ama kaydedilmez.
  2. ButtonTask son olaydan 1 s sonra `txQ`'ya EXPORT işareti koyar. Kuyruk
     FIFO olduğu için işaret geldiğinde tüm kayıtlar kapanmıştır. Telemetri
     ancak o zaman durur, yani son olay da aynı yük altında ölçülür.
- **Dışa aktarma:** `REC`, `CNT` ve `END` satırları. Bunlar 64 bayt kuralının
  dışındadır: ölçüm bittikten sonra gönderildikleri için hiçbir ölçülen
  aralıkla zamanda çakışmazlar.
