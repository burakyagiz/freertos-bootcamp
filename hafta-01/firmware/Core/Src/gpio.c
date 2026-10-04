/**
 * @file    gpio.c
 * @brief   Buton (PA0/EXTI0) ve LED (PD12..PD15) pinleri.
 *          USART2 pinleri (PA2/PA3) usart.c'deki HAL_UART_MspInit'te ayarlanir.
 */
#include "gpio.h"
#include "main.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* LED'ler: once cikis seviyesini sifirla, sonra pini cikis yap ki
     * mod degisirken kisa bir parlama olmasin. */
    HAL_GPIO_WritePin(LED_GPIO_Port,
                      LED_GREEN_Pin | LED_ORANGE_Pin | LED_RED_Pin | LED_BLUE_Pin,
                      GPIO_PIN_RESET);
    gpio.Pin   = LED_GREEN_Pin | LED_ORANGE_Pin | LED_RED_Pin | LED_BLUE_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;   /* LED icin hizli kenar gereksiz, EMI azalir */
    HAL_GPIO_Init(LED_GPIO_Port, &gpio);

    /* B1: kartta harici pull-down direncine bagli (UM1472) -> dahili pull kapali.
     * GPIO_MODE_IT_RISING: HAL, SYSCFG_EXTICR1'de EXTI0'i PA0'a baglar,
     * EXTI_RTSR'de yukselen kenari ve EXTI_IMR'de kesmeyi acar. */
    gpio.Pin   = BUTTON_Pin;
    gpio.Mode  = GPIO_MODE_IT_RISING;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUTTON_GPIO_Port, &gpio);

    /* EXTI0 ISR ileride xQueueSendFromISR cagiracak -> oncelik >= 5 olmali. */
    HAL_NVIC_SetPriority(BUTTON_EXTI_IRQn, APP_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(BUTTON_EXTI_IRQn);
}
