/**
 * @file    app_button.h
 * @brief   Buton (PA0/EXTI0) olay yakalama ve ButtonTask.
 *
 * ISR: t0 zaman damgasi, kenar filtresi, kayit havuzunda olayi acma,
 *      buttonQ'ya gonderim.
 * ButtonTask: olayi alir (t1), yanit mesajini bicimler, txQ'ya gonderir (t2).
 *
 * Kenar filtresi iki kuraldan olusur:
 *  1. Sartname kurali (degismedi): kabul edilen kenardan sonraki 30 ms
 *     icindeki kenarlar sayilir ve atilir (filtered).
 *  2. Birakma sarti (eklendi, 2026-10-12): bir basis kabul edildikten sonra
 *     buton en az 30 ms kesintisiz birakilmis (LOW, hic yukselen kenar yok)
 *     gorulmeden yeni basis kabul edilmez (release_glitch).
 *     Neden: bu kartta birakma ziplamasi 30 ms pencereden SONRA yukselen
 *     kenar uretiyor ve sahte basis sayiliyordu (olcum: 5 birakmada 2,
 *     5 hizli dokunusta 7-8 olay). Birakma TIM6'nin 1 ms'lik kesmesinde
 *     pin orneklenerek izlenir; t0 yolu degismez, kabul aninda bekleme yok.
 */
#ifndef APP_BUTTON_H
#define APP_BUTTON_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

#define BUTTON_DEBOUNCE_US        30000U   /* kural 1: kabulden sonra 30 ms boyunca kenarlar atilir */
#define BUTTON_RELEASE_QUIET_MS   30U      /* kural 2: yeni basistan once gereken kesintisiz birakma */

/* buttonQ'ya DEGER olarak kopyalanan olay. */
typedef struct
{
    uint32_t id;      /* kabul edilen her olaya artan numara (1'den baslar) */
    uint32_t t0_us;   /* ISR'da filtre kararindan once okunan TIM2 degeri */
} ButtonEvent;

/* Her sayacin tek yazari var; 32-bit hizali okuma/yazma Cortex-M4'te tek
 * komut oldugu icin kilit gerekmez.
 *   rise_edges, accepted, filtered, release_glitch, btn_drop -> buton ISR
 *   releases                                                 -> TIM6 ISR
 *   tx_drop                                                  -> ButtonTask
 * Kontroller:
 *   rise_edges = accepted + filtered + release_glitch  (ISR'a gelen her kenar)
 *   accepted   = buttonQ'ya giden + btn_drop
 *   buton birakilmis dururken: releases = accepted */
typedef struct
{
    uint32_t rise_edges;      /* ISR'a gelen tum yukselen kenarlar */
    uint32_t accepted;
    uint32_t filtered;        /* kural 1: kabulden sonraki 30 ms icinde */
    uint32_t release_glitch;  /* kural 2: buton henuz birakilmamisken */
    uint32_t releases;        /* tamamlanan birakma (30 ms kesintisiz LOW) */
    uint32_t btn_drop;
    uint32_t tx_drop;
} ButtonStats;

extern volatile ButtonStats g_button_stats;

/* ISR'in ve ButtonTask'in kullanacagi kuyruklari baglar. Kuyruklar
 * olusturulduktan sonra, scheduler baslamadan once bir kez cagrilir.
 * Baglanmadan gelen kenarlar yok sayilir. */
void app_button_init(QueueHandle_t button_queue, QueueHandle_t tx_queue);

/* Birakma takibi. SADECE TIM6 zaman tabani kesmesinden (1 kHz, NVIC
 * onceligi 15) cagrilir: pini ornekler, kesintisiz birakmayi sayar. */
void app_button_tick_1ms(void);

/* FreeRTOS gorev fonksiyonu (oncelik BUTTON_TASK_PRIORITY). */
void button_task(void *argument);

#endif /* APP_BUTTON_H */
