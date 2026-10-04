/**
 * @file    main.c
 * @brief   Donanim ilklendirme + UART selam + Faz 1 buton testi.
 *
 * Faz 1 (gecici): buttonQ burada olusturulur; ButtonTest gorevi her olayda
 * PD12'yi toggle eder. Faz 2'de gercek gorev mimarisiyle degistirilecek.
 *
 * FPU notu: CubeIDE taslagindaki "FPU is not initialized" uyarisi kaldirildi.
 * Startup, main'den once SystemInit()'i (system_stm32f4xx.c) cagirir; o da
 * -mfloat-abi=hard ile derlendiginde SCB->CPACR'de CP10/CP11'i acar.
 */
#include <string.h>

#include "main.h"
#include "system_clock.h"
#include "gpio.h"
#include "dma.h"
#include "usart.h"
#include "tim.h"
#include "app_time.h"
#include "app_button.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define BUTTON_TEST_STACK_WORDS  configMINIMAL_STACK_SIZE
#define BUTTON_TEST_PRIORITY     (tskIDLE_PRIORITY + 2U)  /* ileride ButtonTask onceligi */

/* 1 yapilirsa test gorevi her olaydan sonra 5 s uyur; o arada 10'dan fazla
 * basilirsa buttonQ (8) dolar ve btn_drop yolu kartta test edilmis olur. */
#ifndef BUTTON_TEST_SLOW_CONSUMER
#define BUTTON_TEST_SLOW_CONSUMER  0
#endif
#define BUTTON_TEST_SLOW_DELAY_MS  5000U

static QueueHandle_t button_queue;

/* Test gorevinin aldigi son olay; debugger'da (Live Expressions) izlemek icin
 * global (static degil) birakildi. */
volatile ButtonEvent g_test_last_event;

static void button_test_task(void *argument);

int main(void)
{
    /* Flash prefetch/cache, NVIC grup 4, HAL tick (TIM6, 16 MHz HSI ile) ve
     * HAL_MspInit. */
    HAL_Init();

    /* 168 MHz'e gecis; HAL_RCC_ClockConfig icinden HAL_InitTick tekrar
     * cagrilip TIM6 prescaler'i yeni APB1 saatine gore guncellenir. */
    SystemClock_Config();

    /* Sira onemli: DMA saati, USART MspInit icindeki HAL_DMA_Init'ten once. */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();
    MX_TIM2_Init();
    app_time_init();   /* TIM2 sayaci buradan itibaren 1 us adimla sayar */

    /* Bloklayan (polling) gonderim, scheduler'dan ve HER FreeRTOS cagrisindan
     * once. Neden: scheduler baslamadan yapilan ilk FreeRTOS API cagrisi
     * BASEPRI ile oncelik >= 5 kesmeleri maskeler; TIM6 (oncelik 15) durur,
     * HAL_GetTick donar ve HAL timeout'lari calismaz. */
    static const char hello[] = "Merhaba F407\r\n";
    if (HAL_UART_Transmit(&huart2, (const uint8_t *)hello, (uint16_t)strlen(hello),
                          100U) != HAL_OK)
    {
        Error_Handler();
    }

    /* buttonQ: 8 olay, FIFO, ButtonEvent DEGER olarak kopyalanir (ISR'daki
     * yerel degisken kuyruga kopyalandigi icin ISR bitince bozulmaz). */
    button_queue = xQueueCreate(BUTTON_QUEUE_LENGTH, sizeof(ButtonEvent));
    if (button_queue == NULL)
    {
        Error_Handler();
    }
    vQueueAddToRegistry(button_queue, "buttonQ");  /* CubeIDE debugger'da adiyla gorunur */

    /* ISR'a kuyrugu bagla. Yukaridaki ilk FreeRTOS cagrisi (xQueueCreate)
     * BASEPRI ile oncelik >= 5 kesmeleri maskeledi; bu yuzden bu arada
     * basilsa bile EXTI0 kesmesi bekler ve fiilen scheduler basladiktan
     * sonra calisir. Kuyruk o anda kesin hazirdir. */
    app_button_init(button_queue);

    if (xTaskCreate(button_test_task, "ButtonTest", BUTTON_TEST_STACK_WORDS, NULL,
                    BUTTON_TEST_PRIORITY, NULL) != pdPASS)
    {
        Error_Handler();
    }

    /* Idle gorevini olusturur, SysTick'i 1 kHz kurar, ilk gorevi SVC ile
     * baslatir. Basarili olursa geri donmez. */
    vTaskStartScheduler();

    /* Buraya gelindiyse idle gorevi icin heap yetmedi. */
    Error_Handler();
}

/* Faz 1 gecici test gorevi: buttonQ'dan olay bekler (portMAX_DELAY = olay
 * gelene kadar CPU harcamadan bloklu kalir), her olayda PD12'yi toggle eder. */
static void button_test_task(void *argument)
{
    (void)argument;
    ButtonEvent event;

    for (;;)
    {
        if (xQueueReceive(button_queue, &event, portMAX_DELAY) == pdPASS)
        {
            g_test_last_event = event;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_GREEN_Pin);
#if BUTTON_TEST_SLOW_CONSUMER
            vTaskDelay(pdMS_TO_TICKS(BUTTON_TEST_SLOW_DELAY_MS));
#endif
        }
    }
}

/* CubeMX ile ayni davranis: kesmeleri kapat ve dur. Debugger'da cagri yigini
 * hatayi ureten init fonksiyonunu gosterir. */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) {}
}
