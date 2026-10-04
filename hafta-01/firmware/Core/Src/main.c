/**
 * @file    main.c
 * @brief   Faz 0b duman testi: donanim ilklendirme + UART selam + LED gorevi.
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

#include "FreeRTOS.h"
#include "task.h"

#define LED_TASK_PERIOD_MS   500U
#define LED_TASK_STACK_WORDS configMINIMAL_STACK_SIZE
#define LED_TASK_PRIORITY    (tskIDLE_PRIORITY + 1U)

static void led_blink_task(void *argument);

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

    if (xTaskCreate(led_blink_task, "LedBlink", LED_TASK_STACK_WORDS, NULL,
                    LED_TASK_PRIORITY, NULL) != pdPASS)
    {
        Error_Handler();
    }

    /* Idle gorevini olusturur, SysTick'i 1 kHz kurar, ilk gorevi SVC ile
     * baslatir. Basarili olursa geri donmez. */
    vTaskStartScheduler();

    /* Buraya gelindiyse idle gorevi icin heap yetmedi. */
    Error_Handler();
}

/* PD12 (yesil LED) her 500 ms'de bir durum degistirir -> 1 Hz yanip sonme.
 * xTaskDelayUntil: periyot, gorevin kendi calisma suresinden bagimsiz sabit. */
static void led_blink_task(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_GREEN_Pin);
        (void)xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LED_TASK_PERIOD_MS));
    }
}

/* CubeMX ile ayni davranis: kesmeleri kapat ve dur. Debugger'da cagri yigini
 * hatayi ureten init fonksiyonunu gosterir. */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) {}
}
