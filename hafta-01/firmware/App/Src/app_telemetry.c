/**
 * @file    app_telemetry.c
 * @brief   TelemetryTask (oncelik 3): sistemi rahatsiz eden periyodik yuk.
 */
#include <inttypes.h>
#include <stdbool.h>

#include "app_telemetry.h"
#include "app_config.h"
#include "app_msg.h"
#include "app_time.h"

volatile TelemetryStats g_telemetry_stats;

static QueueHandle_t s_tx_queue;

/* Hesaplama sonucunun yazildigi yer. volatile oldugu icin derleyici bu
 * yazmayi, dolayisiyla sonucu ureten donguyu silemez. */
static volatile uint32_t s_work_sink;

void app_telemetry_init(QueueHandle_t tx_queue)
{
    s_tx_queue = tx_queue;
}

void calibrated_work(uint32_t iterations)
{
    /* Her tur bir onceki sonuca bagli (dogrusal eslik ureteci): derleyici
     * donguyu tek bir formulle kisaltamaz, her tur gercekten calisir. */
    uint32_t acc = s_work_sink;
    for (uint32_t i = 0U; i < iterations; i++)
    {
        acc = (acc * 1664525U) + 1013904223U;
    }
    s_work_sink = acc;
}

void telemetry_task(void *argument)
{
    (void)argument;

#if TELEMETRY_PERIOD_MS == 0U
    /* S0: telemetri kapali. Bu goreve kimse bildirim gondermedigi icin
     * sonsuza kadar bloklu kalir ve hic CPU harcamaz. */
    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
#else
    const TickType_t period = pdMS_TO_TICKS(TELEMETRY_PERIOD_MS);
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t seq = 0U;
    TxMsg msg;

    for (;;)
    {
        /* Sabit referansa gore uyanir: vTaskDelay'in aksine periyot is
         * suresinden etkilenmez ve zamanla kaymaz. */
        (void)xTaskDelayUntil(&last_wake, period);

        calibrated_work(TELEMETRY_WORK_ITERATIONS);

        seq++;
        if (!msg_format(&msg, MSG_TYPE_TEL, seq, "TEL,%" PRIu32 ",S%u,%" PRIu32,
                        seq, (unsigned)SCENARIO, timer_us()))
        {
            /* Metin sabit ve kisa; buraya gelinmesi programlama hatasidir. */
            configASSERT(0);
            continue;
        }

        /* Timeout 0: telemetri kuyrukta beklerse yuk deseni degisirdi.
         * Kuyruk doluysa mesaj atilir ve sayilir. */
        if (xQueueSend(s_tx_queue, &msg, 0) == pdPASS)
        {
            g_telemetry_stats.sent++;
        }
        else
        {
            g_telemetry_stats.tel_drop++;
        }
    }
#endif
}
