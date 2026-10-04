/**
 * @file    app_button.c
 * @brief   Buton ISR mantigi.
 *
 * Akis: EXTI0_IRQHandler (stm32f4xx_it.c) -> HAL_GPIO_EXTI_IRQHandler
 *       (bayragi temizler) -> HAL_GPIO_EXTI_Callback (burasi).
 * ISR icinde bekleme, printf, UART YOK; is birkac mikrosaniyede biter.
 */
#include <stdbool.h>

#include "app_button.h"
#include "app_time.h"
#include "main.h"

volatile ButtonStats g_button_stats;
ButtonEvent g_button_drop_log[BUTTON_DROP_LOG_LEN];

static QueueHandle_t s_button_queue;   /* NULL: henuz baglanmadi */

/* Filtre durumu. Sadece bu ISR yazar ve ISR kendi kendini kesemez
 * (ayni oncelik), bu yuzden kritik bolum gerekmez. */
static bool     s_has_last;            /* false: henuz hic kabul yok ("ilk olay" durumu) */
static uint32_t s_last_accept_us;      /* son KABUL edilen kenarin zamani */
static uint32_t s_next_id = 1U;

void app_button_init(QueueHandle_t button_queue)
{
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

    /* Tekrar-kenar filtresi. Ilk olayda (s_has_last == false) karsilastirma
     * yapilmaz: s_last_accept_us = 0 varsayilsaydi acilistan sonraki ilk
     * 30 ms'deki gercek bir basis yanlislikla atilirdi. Sure farki isaretsiz
     * cikarma ile alinir (elapsed_us), TIM2 sifira donse de dogrudur.
     * Pencere sabittir: atilan kenarlar referansi (s_last_accept_us) kaydirmaz. */
    if (s_has_last && (elapsed_us(s_last_accept_us, now) < BUTTON_DEBOUNCE_US))
    {
        g_button_stats.filtered++;
        return;
    }
    s_has_last       = true;
    s_last_accept_us = now;

    const ButtonEvent event = { .id = s_next_id++, .t0_us = now };
    g_button_stats.accepted++;

    /* Kuyruk doluysa bekleyemeyiz (ISR). Olay btn_drop sayilir ama id/t0
     * kaybolmaz: drop log'a yazilir. id her kabule verildigi icin dusen
     * olaylar da numarali kalir. */
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (xQueueSendFromISR(s_button_queue, &event, &higher_priority_task_woken) != pdPASS)
    {
        const uint32_t drop_index = g_button_stats.btn_drop;
        if (drop_index < BUTTON_DROP_LOG_LEN)
        {
            g_button_drop_log[drop_index] = event;
        }
        g_button_stats.btn_drop = drop_index + 1U;
    }

    /* Kuyruk, bu olayi bekleyen ve su an calisan gorevden daha yuksek
     * oncelikli bir gorevi uyandirdiysa, ISR biter bitmez ona gecilir
     * (PendSV istenir). Yoksa kesilen gorev kaldigi yerden devam eder. */
    portYIELD_FROM_ISR(higher_priority_task_woken);
}
