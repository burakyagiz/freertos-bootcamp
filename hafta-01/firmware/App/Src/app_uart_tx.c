/**
 * @file    app_uart_tx.c
 * @brief   UartTxTask (oncelik 1) ve USART2 TX callback'leri. En dusuk
 *          oncelikte oldugu icin sadece Telemetry ve Button gorevleri bloklu
 *          iken calisir.
 *
 * Bir mesajin yolu:
 *   xQueueReceive(txQ) -> s_tx_buffer'a kopya -> t3 -> HAL_UART_Transmit_DMA
 *   -> DMA 64 bayti USART2->DR'ye tasir -> DMA TC kesmesi: HAL, USART TC
 *   kesmesini acar -> son baytin stop biti hattan cikar -> USART TC kesmesi
 *   -> HAL_UART_TxCpltCallback: t4, kayit "ok", gorevi uyandir.
 * Gorev bu sirada ulTaskNotifyTake'te bloklu bekler (en fazla 1 s).
 *
 * Gorev ile callback'ler s_tx_state uzerinden haberlesir. Aktarimin sonucunu
 * (bitti / hata / timeout) PENDING'den ilk cikaran taraf belirler; gec kalan
 * taraf hicbir sey yazmaz. Boylece ayni kayda hem "ok" hem "timeout" yazilamaz.
 *
 * Deney sonu (EXPORT isareti): telemetri durdurulur, kuyrukta kalanlar
 * gonderilir, sonra REC/CNT/END satirlari ayni tampon ve ayni gonderim
 * yoluyla (transmit_and_wait) gider.
 */
#include <stdbool.h>
#include <string.h>

#include "app_uart_tx.h"
#include "app_config.h"
#include "app_experiment.h"
#include "app_msg.h"
#include "app_record.h"
#include "app_time.h"
#include "usart.h"

#include "task.h"

/* IDLE = 0: acilista (.bss sifir) callback'ler hicbir sey yapmaz. */
typedef enum
{
    TX_IDLE = 0,    /* henuz aktarim yok */
    TX_PENDING,     /* baslatildi, sonuc bekleniyor */
    TX_DONE,        /* TxCpltCallback: son bit hattan cikti */
    TX_ERROR,       /* ErrorCallback: HAL aktarimi hatayla bitirdi */
    TX_TIMEOUT,     /* gorev UART_TX_TIMEOUT_MS icinde haber alamadi */
    TX_START_FAIL,  /* HAL_UART_Transmit_DMA HAL_OK donmedi, aktarim yok */
} TxState;

volatile UartTxStats g_uart_tx_stats;

static QueueHandle_t s_tx_queue;
static TaskHandle_t  s_task;   /* callback'lerin uyandirdigi gorev */

/* DMA'nin okudugu tampon. Statik: adresi sabit ve .bss -> SRAM'de (DMA1
 * CCM RAM'e erisemez). Sadece UartTxTask yazar, sadece DMA durmusken.
 * Olcum mesajlari ilk 64 bayti kullanir; boyut en uzun disa aktarma
 * satirina gore secildi: CNT, her sayac 10 basamak olsa bile ~560 bayt. */
#define TX_BUFFER_LEN  640U
_Static_assert(TX_BUFFER_LEN >= MSG_LEN, "tampon 64 baytlik mesaji almali");
static uint8_t s_tx_buffer[TX_BUFFER_LEN];

/* Gorev aktarimi baslatmadan once yazar, callback'ler okur. */
static volatile uint8_t  s_tx_is_btn;     /* 1: BTN mesaji, t4 kaydedilir */
static volatile uint32_t s_tx_event_id;   /* BTN: kayit havuzu anahtari */

/* TxState. Gorev PENDING yapar; callback'ler veya timeout PENDING'den cikarir. */
static volatile uint8_t  s_tx_state;

void app_uart_tx_init(QueueHandle_t tx_queue)
{
    s_tx_queue = tx_queue;
}

/* Hata ya da timeout sonrasi cagrilir. Donunce DMA s_tx_buffer'i artik
 * okumuyordur ve huart2 yeni aktarima hazirdir (gState READY). */
static void uart_make_safe(void)
{
    /* TCIE/TXEIE'yi kapatir, DMAT'i temizler, DMA stream'ini bloklayarak
     * durdurur (HAL_DMA_Abort, en fazla 5 ms). Gorevden cagrilir; kesmeler
     * acik oldugu icin HAL_GetTick (TIM6) ilerler, timeout calisir. */
    if (HAL_UART_AbortTransmit(&huart2) != HAL_OK)
    {
        /* DMA durmadi: tampon hala okunuyor olabilir. Tamponu ezip devam
         * etmek bozuk veri gondermek olurdu; dur. */
        Error_Handler();
    }

    /* Yaris kapatma: abort TCIE'yi kapattiktan hemen sonra DMA TC kesmesi
     * araya girerse HAL (UART_DMATransmitCplt) TCIE'yi yeniden acar ve
     * sonraki aktarimin ortasina gec bir TxCplt dusebilir. Tekrar kapat. */
    ATOMIC_CLEAR_BIT(huart2.Instance->CR1, USART_CR1_TCIE);
}

/* Aktarimin sonucunu bekler: TX_DONE, TX_ERROR veya TX_TIMEOUT doner. */
static TxState wait_for_result(void)
{
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(UART_TX_TIMEOUT_MS)) != 0U)
    {
        /* Callback once sonucu yazdi, sonra uyandirdi. */
        return (TxState)s_tx_state;
    }

    /* Sure doldu; ama callback tam bu anda gelmis olabilir. Kontrol ve
     * yazma, callback araya giremesin diye kritik bolumde (birkac komut):
     * PENDING ise timeout bizimdir, degilse callback kazanmistir. */
    taskENTER_CRITICAL();
    if (s_tx_state == TX_PENDING)
    {
        s_tx_state = TX_TIMEOUT;
    }
    const TxState result = (TxState)s_tx_state;
    taskEXIT_CRITICAL();
    return result;
}

/* s_tx_buffer'in ilk len baytini gonderir ve sonucu bekler. Donunce aktarim
 * ya bitmis ya da durdurulmustur: DMA tamponu artik okumuyor, cagiran onu
 * yeniden yazabilir. is_btn ise t3 burada yazilir; t4 ve "ok" callback'te. */
static TxState transmit_and_wait(uint16_t len, bool is_btn, uint32_t event_id)
{
    s_tx_is_btn   = is_btn ? 1U : 0U;
    s_tx_event_id = event_id;

    /* Timeout ile callback ayni anda olduysa callback'in bildirimi
     * sayacta kalmistir; silinmezse asagidaki bekleme hemen donerdi. */
    (void)ulTaskNotifyTake(pdTRUE, 0);
    s_tx_state = TX_PENDING;

    /* t3: baslatma cagrisindan hemen once. */
    if (is_btn)
    {
        record_set_time(event_id, REC_T3, timer_us()); /* t3 burada okunur ve kayda yazilir */
    }

    if (HAL_UART_Transmit_DMA(&huart2, s_tx_buffer, len) != HAL_OK)
    {
        /* Aktarim hic baslamadi, callback gelmeyecek: 1 s beklemek
         * bosuna olur. huart2 takili kalmasin diye yine de durdurulur. */
        s_tx_state = TX_START_FAIL;
        uart_make_safe();
        return TX_START_FAIL;
    }

    const TxState result = wait_for_result();
    if (result != TX_DONE)
    {
        /* Hata ya da timeout: DMA hala calisiyor olabilir, durdur. */
        uart_make_safe();
    }
    return result;
}

/* Bir olcum mesajini (TEL, BTN, CAL; hepsi 64 bayt) gonderir ve sonucu sayar. */
static void send_message(const TxMsg *msg)
{
    const bool is_btn = (msg->type == MSG_TYPE_BTN);
    if (is_btn)
    {
        g_uart_tx_stats.rx_btn++;
    }
    else if (msg->type == MSG_TYPE_CAL)
    {
        g_uart_tx_stats.rx_cal++;
    }
    else
    {
        g_uart_tx_stats.rx_tel++;
    }

    /* Buraya ancak onceki aktarim bittikten ya da durdurulduktan sonra
     * gelinir (transmit_and_wait): DMA tamponu okumuyor, uzerine yazmak
     * guvenli. Kopya t3'ten once, olculen araliga karismasin. */
    memcpy(s_tx_buffer, msg->data, MSG_LEN);

    RecordStatus status;
    switch (transmit_and_wait(MSG_LEN, is_btn, msg->event_id))
    {
    case TX_DONE:
        g_uart_tx_stats.tx_done++;   /* t4 ve "ok" callback'te yazildi */
        return;
    case TX_TIMEOUT:
        g_uart_tx_stats.tx_timeout++;
        status = REC_STATUS_TIMEOUT;
        break;
    case TX_START_FAIL:
        g_uart_tx_stats.start_fail++;
        status = REC_STATUS_TX_ERROR;
        break;
    default:   /* TX_ERROR */
        g_uart_tx_stats.tx_error++;
        status = REC_STATUS_TX_ERROR;
        break;
    }
    if (is_btn)
    {
        record_close(msg->event_id, status);
    }
}

/* EXPORT isareti alindi: olcum bitti (akis: app_experiment.h, adim 3-4). */
static void finish_experiment(void)
{
    /* Isaret FIFO'da son BTN yanitinin arkasindaydi ve bir mesaj bitmeden
     * sonraki alinmaz: su an butun olcum kayitlari kapanmis. Telemetri bu
     * gorev calisirken bloklu (daha yuksek oncelik); uyaninca bayragi gorur
     * ve bir daha mesaj uretmez. */
    experiment_stop_telemetry();

    /* Isaretten sonra kuyruga girmis TEL'ler: gonderilir ki sayaclar
     * (tel_sent = rx_tel) tutarli kalsin. Olcum kayitlarina dokunmazlar. */
    TxMsg msg;
    while (xQueueReceive(s_tx_queue, &msg, 0) == pdPASS)
    {
        if (msg.type != MSG_TYPE_EXPORT)
        {
            send_message(&msg);
        }
    }

    /* Satirlar dogrudan DMA tamponuna yazilir. Ayni guvence: her satir
     * ancak transmit_and_wait bir oncekinin bittigini gosterdikten sonra
     * yazilir. */
    for (uint32_t index = 0U; ; index++)
    {
        const uint32_t len = experiment_export_line(index, (char *)s_tx_buffer, TX_BUFFER_LEN);
        if (len == 0U)
        {
            break;
        }
        if (transmit_and_wait((uint16_t)len, false, 0U) == TX_DONE)
        {
            g_uart_tx_stats.export_lines++;
        }
        else
        {
            g_uart_tx_stats.export_fail++;
        }
    }
    experiment_mark_exported();
}

void uart_tx_task(void *argument)
{
    (void)argument;
    TxMsg msg;

    /* Callback'ler bu handle ile uyandirir. Ilk aktarimdan once atanir,
     * yani hicbir callback onu bos goremez. */
    s_task = xTaskGetCurrentTaskHandle();

    for (;;)
    {
        /* Mesaj gelene kadar CPU harcamadan bloklu bekler. */
        if (xQueueReceive(s_tx_queue, &msg, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        if (msg.type == MSG_TYPE_EXPORT)
        {
            finish_experiment();
        }
        else
        {
            send_message(&msg);
        }
    }
}

/* ---------------------------------------------------------------- Callback'ler
 * Ikisi de USART2 veya DMA1_Stream6 kesmesinden cagrilir (oncelik 6 >= 5):
 * FromISR API serbest. Ikisi ayni oncelikte, birbirini kesemez; gorevi ise
 * her zaman keserler. Bu yuzden icerideki "PENDING mi? -> yaz" adimi gorev
 * acisindan bolunmez. */

static void wake_task_from_isr(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_task, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/* DMA normal modda USART TC kesmesinden cagrilir (HAL_UART_IRQHandler ->
 * UART_EndTransmit_IT): son baytin stop biti hattan cikmistir. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    /* t4 adayi: ilk is (t0 ile ayni yaklasim). */
    const uint32_t now = timer_us(); /* t4 burada okunur; kayda asagida yazilir (sadece BTN) */

    if ((huart->Instance != USART2) || (s_tx_state != TX_PENDING))
    {
        return;   /* bekleyen aktarim yok ya da gorev timeout'a karar verdi */
    }
    s_tx_state = TX_DONE;

    if (s_tx_is_btn != 0U)
    {
        record_set_time(s_tx_event_id, REC_T4, now);
        record_close(s_tx_event_id, REC_STATUS_OK);
    }
    wake_task_from_isr();
}

/* TX tarafinda DMA hatasinda gelir (UART_DMAError). HAL gState'i READY
 * yapar ama DMA'yi her zaman durdurmaz; durdurmayi gorev yapar. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2)
    {
        return;
    }
    g_uart_tx_stats.last_error_code = huart->ErrorCode;

    if (s_tx_state != TX_PENDING)
    {
        return;
    }
    s_tx_state = TX_ERROR;
    wake_task_from_isr();
}
