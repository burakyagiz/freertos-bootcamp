/**
 * @file    main.c
 * @brief   Donanim ilklendirme + UART selam + uygulama mimarisinin baslatilmasi.
 *
 * Kuyruk ve gorevler App/Src/app_tasks.c'de olusturulur; main sadece
 * donanimi hazirlar ve scheduler'i baslatir.
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
#include "app_tasks.h"

#include "FreeRTOS.h"
#include "task.h"

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

    /* buttonQ, txQ ve 3 gorev. Icerideki ilk FreeRTOS cagrisi (xQueueCreate)
     * BASEPRI ile oncelik >= 5 kesmeleri maskeler; bu yuzden bu arada butona
     * basilsa bile EXTI0 kesmesi bekler ve fiilen scheduler basladiktan sonra
     * calisir. Kuyruklar o anda kesin hazirdir. */
    app_tasks_create();

    /* Idle gorevini olusturur, SysTick'i 1 kHz kurar, ilk gorevi SVC ile
     * baslatir. Basarili olursa geri donmez. */
    vTaskStartScheduler();

    /* Buraya gelindiyse idle gorevi icin heap yetmedi. */
    Error_Handler();
}

/* CubeMX ile ayni davranis: kesmeleri kapat ve dur. Debugger'da cagri yigini
 * hatayi ureten init fonksiyonunu gosterir. */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) {}
}
