/**
 * @file    app_config.h
 * @brief   Deney ayarlari: senaryo secimi, gorev oncelikleri, stack ve kuyruk
 *          boyutlari. Deneyi degistiren her sabit burada, tek yerde durur.
 *
 * SCENARIO derleme aninda secilir: asagidaki varsayilani degistir veya
 * derleyiciye -DSCENARIO=<0..5> ver.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "FreeRTOS.h"
#include "task.h"

#ifndef SCENARIO
#define SCENARIO  1   /* Faz 2 testi icin S1: telemetrinin calistigi gorulsun */
#endif

/* Senaryo tablosu (CLAUDE.md). Periyot 0 = telemetri kapali (S0).
 * CPU isi iterasyonlari Faz 4'te kalibre edilecek; simdilik hepsi 0. */
#if   SCENARIO == 0
#define TELEMETRY_PERIOD_MS        0U
#define TELEMETRY_WORK_ITERATIONS  0U
#elif SCENARIO == 1
#define TELEMETRY_PERIOD_MS        100U   /* 10 Hz */
#define TELEMETRY_WORK_ITERATIONS  0U
#elif SCENARIO == 2
#define TELEMETRY_PERIOD_MS        20U    /* 50 Hz */
#define TELEMETRY_WORK_ITERATIONS  0U
#elif SCENARIO == 3
#define TELEMETRY_PERIOD_MS        10U    /* 100 Hz */
#define TELEMETRY_WORK_ITERATIONS  0U
#elif SCENARIO == 4
#define TELEMETRY_PERIOD_MS        10U    /* 100 Hz + ~2 ms CPU isi */
#define TELEMETRY_WORK_ITERATIONS  0U     /* Faz 4'te kalibre edilecek */
#elif SCENARIO == 5
#define TELEMETRY_PERIOD_MS        10U    /* 100 Hz + ~5 ms CPU isi */
#define TELEMETRY_WORK_ITERATIONS  0U     /* Faz 4'te kalibre edilecek */
#else
#error "SCENARIO 0..5 araliginda olmali"
#endif

/* Gorev oncelikleri (FreeRTOS'ta buyuk sayi = daha oncelikli; NVIC'in tersi).
 *   Telemetry 3: sistemi rahatsiz eden yuk; digerlerini kesebilsin ki etkisi
 *                olculebilsin.
 *   Button    2: incelenen gorev; telemetri calisirken bekler (t1 - t0).
 *   UartTx    1: hatti besler; sadece digerleri bloklu iken calisir (t3 - t2). */
#define TELEMETRY_TASK_PRIORITY    (tskIDLE_PRIORITY + 3U)
#define BUTTON_TASK_PRIORITY       (tskIDLE_PRIORITY + 2U)
#define UART_TX_TASK_PRIORITY      (tskIDLE_PRIORITY + 1U)

/* Stack boyutlari word (4 bayt) cinsinden. vsnprintf kullanan gorevler icin
 * 256 word = 1 KB baslangic degeri; kartta stack payi olculup ayarlanir. */
#define TELEMETRY_TASK_STACK_WORDS 256U
#define BUTTON_TASK_STACK_WORDS    256U
#define UART_TX_TASK_STACK_WORDS   256U

/* Kuyruk kapasiteleri (oge sayisi). Ogeler DEGER olarak kopyalanir. */
#define BUTTON_QUEUE_LENGTH        8U    /* ButtonEvent */
#define TX_QUEUE_LENGTH            16U   /* TxMsg */

#endif /* APP_CONFIG_H */
