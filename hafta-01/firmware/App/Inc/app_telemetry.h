/**
 * @file    app_telemetry.h
 * @brief   TelemetryTask: senaryoya gore periyodik TEL mesaji ve CPU yuku.
 */
#ifndef APP_TELEMETRY_H
#define APP_TELEMETRY_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

/* Her sayacin tek yazari TelemetryTask. */
typedef struct
{
    uint32_t sent;       /* txQ'ya giren TEL mesaji */
    uint32_t tel_drop;   /* txQ dolu oldugu icin atilan TEL mesaji */
} TelemetryStats;

extern volatile TelemetryStats g_telemetry_stats;

/* TEL mesajlarinin gidecegi kuyrugu baglar; scheduler'dan once bir kez. */
void app_telemetry_init(QueueHandle_t tx_queue);

/* Sabit iterasyonlu CPU isi (Faz 4'te kalibre edilecek). Kesmeleri kapatmaz,
 * CPU'yu birakmaz: gercek hesaplama yukunu temsil eder (vTaskDelay yuk degildir). */
void calibrated_work(uint32_t iterations);

/* FreeRTOS gorev fonksiyonu (oncelik TELEMETRY_TASK_PRIORITY). */
void telemetry_task(void *argument);

#endif /* APP_TELEMETRY_H */
