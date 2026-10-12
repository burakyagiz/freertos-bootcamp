/**
 * @file    app_button.h
 * @brief   Buton (PA0/EXTI0) olay yakalama ve ButtonTask.
 *
 * ISR: t0 zaman damgasi, 30 ms tekrar-kenar filtresi, kayit havuzunda olayi
 *      acma, buttonQ'ya gonderim.
 * ButtonTask: olayi alir (t1), yanit mesajini bicimler, txQ'ya gonderir (t2).
 */
#ifndef APP_BUTTON_H
#define APP_BUTTON_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

#define BUTTON_DEBOUNCE_US    30000U   /* kabulden sonra 30 ms boyunca kenarlar atilir */

/* buttonQ'ya DEGER olarak kopyalanan olay. */
typedef struct
{
    uint32_t id;      /* kabul edilen her olaya artan numara (1'den baslar) */
    uint32_t t0_us;   /* ISR'da filtre kararindan once okunan TIM2 degeri */
} ButtonEvent;

/* Her sayacin tek yazari var; 32-bit hizali okuma/yazma Cortex-M4'te tek
 * komut oldugu icin kilit gerekmez.
 *   accepted, filtered, btn_drop -> ISR
 *   tx_drop                      -> ButtonTask
 *   accepted = buttonQ'ya giden + btn_drop  (filtreyi gecen her kenar) */
typedef struct
{
    uint32_t accepted;
    uint32_t filtered;
    uint32_t btn_drop;
    uint32_t tx_drop;
} ButtonStats;

extern volatile ButtonStats g_button_stats;

/* ISR'in ve ButtonTask'in kullanacagi kuyruklari baglar. Kuyruklar
 * olusturulduktan sonra, scheduler baslamadan once bir kez cagrilir.
 * Baglanmadan gelen kenarlar yok sayilir. */
void app_button_init(QueueHandle_t button_queue, QueueHandle_t tx_queue);

/* FreeRTOS gorev fonksiyonu (oncelik BUTTON_TASK_PRIORITY). */
void button_task(void *argument);

#endif /* APP_BUTTON_H */
