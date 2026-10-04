/**
 * @file    app_button.h
 * @brief   Buton (PA0/EXTI0) olay yakalama: t0 zaman damgasi, 30 ms tekrar-kenar
 *          filtresi, buttonQ'ya gonderim ve sayaclar.
 */
#ifndef APP_BUTTON_H
#define APP_BUTTON_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

#define BUTTON_QUEUE_LENGTH   8U       /* buttonQ kapasitesi (olay) */
#define BUTTON_DEBOUNCE_US    30000U   /* kabulden sonra 30 ms boyunca kenarlar atilir */
#define BUTTON_DROP_LOG_LEN   16U      /* kuyruga giremeyen olaylarin saklanan ilk N tanesi */

/* buttonQ'ya DEGER olarak kopyalanan olay. */
typedef struct
{
    uint32_t id;      /* kabul edilen her olaya artan numara (1'den baslar) */
    uint32_t t0_us;   /* ISR'da filtre kararindan once okunan TIM2 degeri */
} ButtonEvent;

/* Sayaclara sadece ISR yazar; gorevler/debugger okur. 32-bit hizali
 * okuma/yazma Cortex-M4'te tek komut oldugu icin kilit gerekmez.
 *   accepted = kuyruga giden + btn_drop  (filtreyi gecen her kenar) */
typedef struct
{
    uint32_t accepted;
    uint32_t filtered;
    uint32_t btn_drop;
} ButtonStats;

extern volatile ButtonStats g_button_stats;

/* Kuyruk doluyken kaybolmasin diye ilk BUTTON_DROP_LOG_LEN dusen olayin
 * id ve t0'i burada tutulur (gecici; Faz 2'de kayit havuzuna yazilacak).
 * Gecerli kayit sayisi: min(g_button_stats.btn_drop, BUTTON_DROP_LOG_LEN). */
extern ButtonEvent g_button_drop_log[BUTTON_DROP_LOG_LEN];

/* ISR'in olay gonderecegi kuyrugu baglar. Kuyruk olusturulduktan sonra,
 * scheduler baslamadan once bir kez cagrilir. Baglanmadan gelen kenarlar
 * yok sayilir. */
void app_button_init(QueueHandle_t button_queue);

#endif /* APP_BUTTON_H */
