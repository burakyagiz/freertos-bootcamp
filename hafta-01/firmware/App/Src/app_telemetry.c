/**
 * @file    app_telemetry.c
 * @brief   TelemetryTask (oncelik 3): sistemi rahatsiz eden periyodik yuk.
 *
 * Acilista (isinma icinde, her senaryoda) CPU isini kalibre eder ve CAL
 * satirini gonderir. Sonra senaryonun periyoduyla: CPU isi + TEL mesaji.
 * Deney bitince UartTxTask'in koydugu bayrakla durur.
 */
#include <inttypes.h>
#include <stdbool.h>

#include "app_telemetry.h"
#include "app_config.h"
#include "app_experiment.h"
#include "app_msg.h"
#include "app_time.h"

volatile TelemetryStats   g_telemetry_stats;
volatile CalibrationResult g_calibration;

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

/* calibrated_work(CAL_ITERATIONS) CAL_REPEATS kez TIM2 ile olculur.
 * Kesmeler acik kalir: SysTick, TIM6 ve o an gelen buton/UART kesmeleri bir
 * turun icine dusup olculen sureyi uzatabilir. Bu yuzden iterasyon/ms, bu
 * etkiye en az maruz kalan EN KISA turdan hesaplanir; max - min farki
 * kesmelerin ne kadar etkiledigini gosterir. Gercek yukte de ayni kesmeler
 * olacagi icin is suresi ayrica her periyotta olculur (work_* sayaclari). */
static void run_calibration(void)
{
    uint32_t min_us = UINT32_MAX;
    uint32_t max_us = 0U;

    for (uint32_t r = 0U; r < CAL_REPEATS; r++)
    {
        const uint32_t start = timer_us();
        calibrated_work(CAL_ITERATIONS);
        const uint32_t took_us = elapsed_us(start, timer_us());

        if (took_us < min_us)
        {
            min_us = took_us;
        }
        if (took_us > max_us)
        {
            max_us = took_us;
        }
    }

    g_calibration.iterations  = CAL_ITERATIONS;
    g_calibration.min_us      = min_us;
    g_calibration.max_us      = max_us;
    g_calibration.iter_per_ms = (min_us > 0U)
        ? (uint32_t)(((uint64_t)CAL_ITERATIONS * 1000U) / min_us)
        : 0U;
}

static void send_calibration_line(void)
{
    TxMsg msg;
    if (!msg_format(&msg, MSG_TYPE_CAL, 0U,
                    "CAL,S%u,n=%" PRIu32 ",min_us=%" PRIu32 ",max_us=%" PRIu32 ",iter_per_ms=%" PRIu32,
                    (unsigned)SCENARIO, g_calibration.iterations, g_calibration.min_us,
                    g_calibration.max_us, g_calibration.iter_per_ms))
    {
        /* Degerler ~5 basamakli; 63 bayta sigmamasi programlama hatasidir. */
        configASSERT(0);
        return;
    }
    /* Isinmada kuyruk bos, timeout 0 yeter. Gidemese de deger CNT satirinda. */
    (void)xQueueSend(s_tx_queue, &msg, 0);
}

#if TELEMETRY_PERIOD_MS > 0U
/* min/max/toplam/adet istatistigine bir ornek ekler. */
static void stat_add(volatile uint32_t *count, volatile uint32_t *min_us,
                     volatile uint32_t *max_us, volatile uint64_t *sum_us, uint32_t value_us)
{
    if ((*count == 0U) || (value_us < *min_us))
    {
        *min_us = value_us;
    }
    if ((*count == 0U) || (value_us > *max_us))
    {
        *max_us = value_us;
    }
    *sum_us += value_us;
    (*count)++;
}

/* Deney bitene kadar periyodik is + TEL. Telemetri durdurulunca doner. */
static void run_telemetry(void)
{
    const TickType_t period = pdMS_TO_TICKS(TELEMETRY_PERIOD_MS);
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t seq = 0U;
    bool     has_prev = false;   /* periyot icin onceki uretim ani var mi */
    uint32_t prev_us  = 0U;
    TxMsg msg;

    for (;;)
    {
        /* Sabit referansa gore uyanir: vTaskDelay'in aksine periyot is
         * suresinden etkilenmez ve zamanla kaymaz. */
        (void)xTaskDelayUntil(&last_wake, period);

        /* Bayragi UartTxTask (oncelik 1) koyar; o calisiyorsa bu gorev
         * (oncelik 3) bloklu demektir, yani bayrak hep burada, bir
         * mesajin ortasinda degil, gorulur. Sonrasinda TEL uretilmez. */
        if (experiment_telemetry_stopped())
        {
            return;
        }
        const bool measuring = experiment_warmup_done();

        /* PD13 (turuncu LED) CPU isi boyunca HIGH: osiloskopla is suresi
         * dogrulanabilir; S4/S5'te LED parlakligi da farki gosterir. */
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_SET);
        const uint32_t work_start = timer_us();
        calibrated_work(TELEMETRY_WORK_ITERATIONS);
        const uint32_t work_us = elapsed_us(work_start, timer_us());
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_RESET);

        /* Uretim ani: hem mesaja yazilir hem periyot olcumunde kullanilir. */
        const uint32_t now = timer_us();
        if (measuring)
        {
            stat_add(&g_telemetry_stats.work_count, &g_telemetry_stats.work_min_us,
                     &g_telemetry_stats.work_max_us, &g_telemetry_stats.work_sum_us, work_us);
            if (has_prev)
            {
                stat_add(&g_telemetry_stats.period_count, &g_telemetry_stats.period_min_us,
                         &g_telemetry_stats.period_max_us, &g_telemetry_stats.period_sum_us,
                         elapsed_us(prev_us, now));
            }
            has_prev = true;
            prev_us  = now;
        }

        seq++;
        if (!msg_format(&msg, MSG_TYPE_TEL, seq, "TEL,%" PRIu32 ",S%u,%" PRIu32,
                        seq, (unsigned)SCENARIO, now))
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

            /* txQ sadece UartTxTask (daha dusuk oncelik) alinca azalir; o
             * araya giremeyecegi icin bu okuma, bu gonderimin ulastigi
             * doluluktur. ButtonTask kendi gonderimleri icin ayni seyi yapar. */
            const UBaseType_t waiting = uxQueueMessagesWaiting(s_tx_queue);
            if (waiting > g_telemetry_stats.txq_max)
            {
                g_telemetry_stats.txq_max = waiting;
            }
        }
        else
        {
            g_telemetry_stats.tel_drop++;
        }
    }
}
#endif

void telemetry_task(void *argument)
{
    (void)argument;

    /* Her senaryoda (S0 dahil) isinmanin ilk ~70 ms'sinde biter; olcum
     * baslamadan once. */
    run_calibration();
    send_calibration_line();

#if TELEMETRY_PERIOD_MS > 0U
    run_telemetry();
#endif

    /* S0'da hemen, digerlerinde deney bitince: bu goreve kimse bildirim
     * gondermedigi icin sonsuza kadar bloklu kalir ve CPU harcamaz. */
    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}
