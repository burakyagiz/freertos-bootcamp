/**
 * @file    stm32f4xx_hal_msp.c
 * @brief   HAL_Init() tarafindan cagrilan global MSP (MCU Support Package) ayari.
 *          Cevre birimine ozgu MspInit'ler kendi dosyalarinda (usart.c, tim.c).
 */
#include "main.h"

void HAL_MspInit(void)
{
    /* SYSCFG: EXTI hatlarini GPIO portuna baglayan EXTICR registerlari icin.
     * PWR: SystemClock_Config'de voltaj olcegi icin. */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    /* NVIC grup 4 (4 bit preemption, 0 bit sub-priority) HAL_Init() icinde
     * zaten ayarlandi; FreeRTOS portu bunu her FromISR cagrisinda
     * vPortValidateInterruptPriority icinde configASSERT ile kontrol eder. */
}
