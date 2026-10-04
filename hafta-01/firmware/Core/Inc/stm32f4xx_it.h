/**
 * @file    stm32f4xx_it.h
 * @brief   Kesme servis rutini prototipleri.
 *          SVC/PendSV/SysTick burada yok: FreeRTOSConfig.h ile port.c'ye eslenir.
 */
#ifndef STM32F4xx_IT_H
#define STM32F4xx_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);

void EXTI0_IRQHandler(void);
void DMA1_Stream6_IRQHandler(void);
void USART2_IRQHandler(void);
void TIM6_DAC_IRQHandler(void);

#endif /* STM32F4xx_IT_H */
