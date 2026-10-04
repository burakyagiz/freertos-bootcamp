/**
 * @file    stm32f4xx_it.c
 * @brief   Kesme servis rutinleri.
 *
 * Startup dosyasi her vektoru zayif (weak) olarak Default_Handler'a yonlendirir;
 * buradaki guclu tanimlar onlari ezer. SVC_Handler, PendSV_Handler ve
 * SysTick_Handler burada BILEREK yok: FreeRTOSConfig.h'deki #define'lar ile
 * port.c'deki vPortSVCHandler/xPortPendSVHandler/xPortSysTickHandler bu
 * adlarla derlenir. Burada da tanimlansaydi linker "multiple definition" verirdi.
 */
#include "stm32f4xx_it.h"
#include "main.h"
#include "usart.h"

extern TIM_HandleTypeDef htim6;

/* ------------------------------------------------------- Cortex-M4 istisnalari */
/* Ayri sonsuz donguler: debugger durdugunda hangi hatanin oldugu PC'den
 * hemen anlasilir (hepsi Default_Handler'da olsaydi ayirt edilemezdi). */
void NMI_Handler(void)
{
    for (;;) {}
}

void HardFault_Handler(void)
{
    for (;;) {}
}

void MemManage_Handler(void)
{
    for (;;) {}
}

void BusFault_Handler(void)
{
    for (;;) {}
}

void UsageFault_Handler(void)
{
    for (;;) {}
}

void DebugMon_Handler(void)
{
}

/* ------------------------------------------------------- STM32F4 cevre kesmeleri */
void EXTI0_IRQHandler(void)
{
    /* Bekleyen bayragi temizler ve HAL_GPIO_EXTI_Callback(GPIO_PIN_0) cagirir. */
    HAL_GPIO_EXTI_IRQHandler(BUTTON_Pin);
}

void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void TIM6_DAC_IRQHandler(void)
{
    /* DAC kullanilmiyor; vektor TIM6 ile paylasiliyor. */
    HAL_TIM_IRQHandler(&htim6);
}
