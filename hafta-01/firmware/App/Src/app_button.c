/**
 * @file    app_button.c
 * @brief   Buton ISR mantigi ve ButtonTask.
 *
 * Akis: EXTI0_IRQHandler (stm32f4xx_it.c) -> HAL_GPIO_EXTI_IRQHandler
 *       (bayragi temizler) -> HAL_GPIO_EXTI_Callback (burasi).
 * ISR icinde bekleme, printf, UART YOK; is birkac mikrosaniyede biter.
 */
#include <inttypes.h>
#include <stdbool.h>

#include "app_button.h"
#include "app_config.h"
#include "app_msg.h"
#include "app_record.h"
#include "app_time.h"
#include "main.h"

volatile ButtonStats g_button_stats;

static QueueHandle_t s_button_queue;   /* NULL: henuz baglanmadi */
static QueueHandle_t s_tx_queue;

/* Filtre durumu. Sadece buton ISR'i yazar ve ISR kendi kendini kesemez
 * (ayni oncelik), bu yuzden kritik bolum gerekmez. */
static bool     s_has_last;            /* false: henuz hic kabul yok ("ilk olay" durumu) */
static uint32_t s_last_accept_us;      /* son KABUL edilen kenarin zamani */
static uint32_t s_releases_at_accept;  /* son kabulde g_button_stats.releases degeri */
static uint32_t s_next_id = 1U;

/* Birakma takibi durumu. Sadece TIM6 ISR'i yazar. Acilista buton birakilmis
 * ve bu birakma zaten sayilmis kabul edilir; boylece bos dururken
 * releases = accepted olur. */
static uint32_t s_low_ms = BUTTON_RELEASE_QUIET_MS;  /* pin kac ms'dir kesintisiz LOW */
static uint32_t s_seen_rise_edges;                   /* son ornekte gorulen kenar sayisi */

void app_button_init(QueueHandle_t button_queue, QueueHandle_t tx_queue)
{
    s_tx_queue     = tx_queue;
    s_button_queue = button_queue;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* t0 adayi: callback'te yapilan ILK is, ki filtre/kuyruk islemleri
     * olculen sureye karismasin. Filtre karari da ayni degerle verilir;
     * zaman ikinci kez okunmaz. */
    const uint32_t now = timer_us();

    if (GPIO_Pin != BUTTON_Pin)
    {
        return;
    }
    if (s_button_queue == NULL)
    {
        return;  /* MX_GPIO_Init ile app_button_init arasindaki kisa an */
    }
    g_button_stats.rise_edges++;   /* TIM6 bunu gorup birakma sayimini sifirlar */

    /* Kural 1, tekrar-kenar filtresi. Ilk olayda (s_has_last == false) karsilastirma
     * yapilmaz: s_last_accept_us = 0 varsayilsaydi acilistan sonraki ilk
     * 30 ms'deki gercek bir basis yanlislikla atilirdi. Sure farki isaretsiz
     * cikarma ile alinir (elapsed_us), TIM2 sifira donse de dogrudur.
     * Pencere sabittir: atilan kenarlar referansi (s_last_accept_us) kaydirmaz. */
    if (s_has_last && (elapsed_us(s_last_accept_us, now) < BUTTON_DEBOUNCE_US))
    {
        g_button_stats.filtered++;
        return;
    }

    /* Kural 2, birakma sarti. Son kabulden beri TIM6 tamamlanmis bir birakma
     * saymadiysa buton hala basili ya da birakma ziplamasi suruyor demektir:
     * bu kenar yeni bir basis degildir. Ilk olayda (s_has_last == false)
     * kontrol yapilmaz; acilista buton birakilmis kabul edilir. */
    if (s_has_last && (g_button_stats.releases == s_releases_at_accept))
    {
        g_button_stats.release_glitch++;
        return;
    }

    s_has_last           = true;
    s_last_accept_us     = now;
    s_releases_at_accept = g_button_stats.releases;

    const ButtonEvent event = { .id = s_next_id++, .t0_us = now };
    g_button_stats.accepted++;

    /* Kayit kuyruga gondermeden ONCE acilir: ButtonTask olayi alip t1'i
     * yazdiginda slot hazir olmali. */
    record_open(event.id, now);

    /* Kuyruk doluysa bekleyemeyiz (ISR). Olay kaybolmaz: kaydi btn_drop
     * durumuyla kapanir, id ve t0'i havuzda kalir. */
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (xQueueSendFromISR(s_button_queue, &event, &higher_priority_task_woken) != pdPASS)
    {
        record_close(event.id, REC_STATUS_BTN_DROP);
        g_button_stats.btn_drop++;
    }

    /* Kuyruk, bu olayi bekleyen ve su an calisan gorevden daha yuksek
     * oncelikli bir gorevi uyandirdiysa, ISR biter bitmez ona gecilir
     * (PendSV istenir). Yoksa kesilen gorev kaldigi yerden devam eder. */
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/* TIM6 ISR'indan her 1 ms'de cagrilir (oncelik 15). Buton ISR'i (oncelik 6)
 * bunu kesebilir ama tersi olamaz; iki taraf da sadece kendi sayacina
 * yazar. Maliyet birkac komut (~0.1 us).
 *
 * Birakma tamamlandi sayilmasi icin 30 ardisik ornekte:
 *  - pin LOW okunmali ve
 *  - buton ISR'ina hic yeni kenar gelmemis olmali.
 * Ikinci sart, iki ornek arasina sigan kisa ziplamalari da yakalar. */
void app_button_tick_1ms(void)
{
    const uint32_t edges = g_button_stats.rise_edges;
    const bool     low   = (HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin) == GPIO_PIN_RESET);

    if (!low || (edges != s_seen_rise_edges))
    {
        s_seen_rise_edges = edges;
        s_low_ms = 0U;      /* basili ya da zipliyor: sayima bastan basla */
        return;
    }

    if (s_low_ms < BUTTON_RELEASE_QUIET_MS)
    {
        s_low_ms++;
        if (s_low_ms == BUTTON_RELEASE_QUIET_MS)
        {
            /* Esik birakma basina bir kez gecilir. Son kontrol ve artis buton
             * kesmesi araya giremeden yapilir: yukaridaki okumadan sonra gelen
             * bir kenar, eski "birakildi" kararina gore sayilmasin. Kritik
             * bolum birkac komut surer (< 0.1 us) ve birakma basina bir kez
             * calisir. */
            const UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR();
            if (g_button_stats.rise_edges == edges)
            {
                g_button_stats.releases++;
            }
            else
            {
                s_low_ms = 0U;
            }
            taskEXIT_CRITICAL_FROM_ISR(saved);
        }
    }
}

/* ButtonTask (oncelik 2). Olcum noktalari arasinda gereksiz is yok: t1 ile t2
 * arasinda sadece yanit mesajinin hazirlanmasi, t2 ile gonderim arasinda
 * sadece kaydin yazilmasi var. */
void button_task(void *argument)
{
    (void)argument;
    ButtonEvent event;
    TxMsg msg;

    for (;;)
    {
        /* Olay gelene kadar CPU harcamadan bloklu bekler. */
        if (xQueueReceive(s_button_queue, &event, portMAX_DELAY) != pdPASS)
        {
            continue;
        }
        record_set_time(event.id, REC_T1, timer_us());

        if (!msg_format(&msg, MSG_TYPE_BTN, event.id, "BTN,%" PRIu32 ",S%u,PRESSED",
                        event.id, (unsigned)SCENARIO))
        {
            /* Metin sabit ve kisa (en fazla 25 karakter); buraya gelinmesi
             * programlama hatasidir. */
            configASSERT(0);
            continue;
        }

        /* t2 gonderimden ONCE okunur ve kayda yazilir. Olay txQ'ya verildikten
         * sonra kayda artik sonraki sahip (UartTxTask) yazar; oncelikler
         * degistirilse bile bu sira bozulmaz. Gonderim basarisizsa olay
         * elden cikmamistir, kaydi ButtonTask kapatir. */
        record_set_time(event.id, REC_T2, timer_us());

        /* Timeout 0: txQ doluysa beklemek ButtonTask'i baska bir duruma sokardi;
         * bunun yerine kayip gorunur kilinir. */
        if (xQueueSend(s_tx_queue, &msg, 0) != pdPASS)
        {
            record_close(event.id, REC_STATUS_TX_DROP);
            g_button_stats.tx_drop++;
        }

        /* Gorsel geri bildirim. t1..t2 disinda ama t2..t3 icinde: UartTxTask
         * (daha dusuk oncelik) ancak ButtonTask bloklaninca, yani bu satirdan
         * sonra calisir. Maliyeti birkac komut (~0.1 us), TIM2'nin 1 us
         * cozunurlugunun altinda. */
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_GREEN_Pin);
    }
}
