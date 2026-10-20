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
#define SCENARIO  1   /* varsayilan S1; her senaryo olcumunde burayi degistir */
#endif

/* S4/S5 CPU isi iterasyonlari. Her acilista PC'ye gelen CAL satirindaki
 * iter_per_ms degeri x hedef ms (2 ve 5) ile doldurulur. Kalibrasyon bu
 * derleme ayarina (Debug, -O0) gore gecerlidir; optimizasyon degisirse
 * yeniden yapilmali. 0 iken S4/S5 derlenmez (asagidaki #error).
 * Kalibrasyon 2026-10-20 (S1, Debug): n=100000, min 12537 us, max 12539 us
 * -> 7976 iter/ms. */
#ifndef WORK_ITERATIONS_2MS
#define WORK_ITERATIONS_2MS        15952U   /* 7976 x 2 */
#endif
#ifndef WORK_ITERATIONS_5MS
#define WORK_ITERATIONS_5MS        39880U   /* 7976 x 5 */
#endif

/* Senaryo tablosu (CLAUDE.md). Periyot 0 = telemetri kapali (S0). */
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
#define TELEMETRY_WORK_ITERATIONS  WORK_ITERATIONS_2MS
#elif SCENARIO == 5
#define TELEMETRY_PERIOD_MS        10U    /* 100 Hz + ~5 ms CPU isi */
#define TELEMETRY_WORK_ITERATIONS  WORK_ITERATIONS_5MS
#else
#error "SCENARIO 0..5 araliginda olmali"
#endif

/* Kalibre edilmemis S4/S5 yuksuz calisir ve S3'ten farksiz olcum uretirdi;
 * bu yanlislikla olmasin diye derleme durdurulur. */
#if (SCENARIO >= 4) && (TELEMETRY_WORK_ITERATIONS == 0U)
#error "S4/S5: once kalibrasyon yap, WORK_ITERATIONS_2MS / WORK_ITERATIONS_5MS degerlerini gir"
#endif

/* Kalibrasyon (her acilista, isinma sirasinda TelemetryTask'ta). Tek tur
 * -O0'da ~8 ms surer; CAL_REPEATS tur icinden en kisasi alinir. */
#define CAL_ITERATIONS             100000U
#define CAL_REPEATS                8U

/* Deney akisi (SISTEM_ACIKLAMASI.md bolum 10).
 * Acilistan sonraki ilk EXPERIMENT_WARMUP_MS boyunca basislar kaydedilmez.
 * EXPERIMENT_TARGET_EVENTS kabulde yeni basis alinmaz; son olaydan
 * EXPERIMENT_END_DELAY_MS sonra telemetri durur ve kayitlar disa aktarilir.
 * Bekleme, durdurma isinin son olayin olculen araliklarina (t2..t3)
 * karismamasi icindir. */
#define EXPERIMENT_WARMUP_MS       5000U
#define EXPERIMENT_TARGET_EVENTS   30U
#define EXPERIMENT_END_DELAY_MS    1000U

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
/* UartTxTask: deney sonunda CNT satirini bicimler (sayac tablosu ~300 B +
 * vsnprintf); 1 KB sinirda kalirdi, 2 KB verildi. */
#define UART_TX_TASK_STACK_WORDS   512U

/* Kuyruk kapasiteleri (oge sayisi). Ogeler DEGER olarak kopyalanir. */
#define BUTTON_QUEUE_LENGTH        8U    /* ButtonEvent */
#define TX_QUEUE_LENGTH            16U   /* TxMsg */

/* UartTxTask'in tek bir aktarimin bitmesini en fazla bekledigi sure (deney
 * gozetim timeout'u). 64 bayt normalde ~5.6 ms surer; 1 s dolduysa aktarim
 * takilmistir, UART durdurulur ve kayit "timeout" ile kapanir. */
#define UART_TX_TIMEOUT_MS         1000U

#endif /* APP_CONFIG_H */
