/**
 * @file    app_telemetry.h
 * @brief   TelemetryTask: senaryoya gore periyodik TEL mesaji ve CPU yuku.
 */
#ifndef APP_TELEMETRY_H
#define APP_TELEMETRY_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

/* Her alanin tek yazari TelemetryTask. period_* ve work_* sadece isinma
 * bittikten sonra, telemetri durana kadar toplanir; *_count 0 ise olculmedi. */
typedef struct
{
    uint32_t sent;            /* txQ'ya giren TEL mesaji */
    uint32_t tel_drop;        /* txQ dolu oldugu icin atilan TEL mesaji */
    uint32_t txq_max;         /* TEL gonderiminden hemen sonra gorulen en buyuk txQ dolulugu */

    /* Gercek telemetri periyodu: ardisik iki TEL uretim aninin farki. */
    uint32_t period_count;
    uint32_t period_min_us;
    uint32_t period_max_us;
    uint64_t period_sum_us;   /* ortalama = sum / count */

    /* CPU isinin (calibrated_work) duvar saati suresi; araya giren ISR'lar dahil. */
    uint32_t work_count;
    uint32_t work_min_us;
    uint32_t work_max_us;
    uint64_t work_sum_us;
} TelemetryStats;

extern volatile TelemetryStats g_telemetry_stats;

/* Acilis kalibrasyonu. Tek yazari TelemetryTask (acilista bir kez). */
typedef struct
{
    uint32_t iterations;    /* tur basina iterasyon (CAL_ITERATIONS) */
    uint32_t min_us;        /* en kisa tur: kesme etkisi en az olan */
    uint32_t max_us;        /* en uzun tur: min ile farki kesmelerin etkisini gosterir */
    uint32_t iter_per_ms;   /* iterations * 1000 / min_us; 0 = henuz yapilmadi */
} CalibrationResult;

extern volatile CalibrationResult g_calibration;

/* TEL mesajlarinin gidecegi kuyrugu baglar; scheduler'dan once bir kez. */
void app_telemetry_init(QueueHandle_t tx_queue);

/* Sabit iterasyonlu CPU isi (iterasyon/ms acilis kalibrasyonuyla bulunur).
 * Kesmeleri kapatmaz, CPU'yu birakmaz: gercek hesaplama yukunu temsil eder
 * (vTaskDelay yuk degildir). */
void calibrated_work(uint32_t iterations);

/* FreeRTOS gorev fonksiyonu (oncelik TELEMETRY_TASK_PRIORITY). */
void telemetry_task(void *argument);

#endif /* APP_TELEMETRY_H */
