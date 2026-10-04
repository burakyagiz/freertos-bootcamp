/**
 * @file    main.h
 * @brief   Ortak include ve pin etiketleri (CubeMX "User Label" karsiligi).
 */
#ifndef MAIN_H
#define MAIN_H

#include "stm32f4xx_hal.h"

/* B1 mavi buton: basinca HIGH -> yukselen kenar = basis */
#define BUTTON_Pin         GPIO_PIN_0
#define BUTTON_GPIO_Port   GPIOA
#define BUTTON_EXTI_IRQn   EXTI0_IRQn

/* Discovery LED'leri (PD12..PD15) */
#define LED_GREEN_Pin      GPIO_PIN_12
#define LED_ORANGE_Pin     GPIO_PIN_13
#define LED_RED_Pin        GPIO_PIN_14
#define LED_BLUE_Pin       GPIO_PIN_15
#define LED_GPIO_Port      GPIOD

/* FromISR API cagiran kesmelerin NVIC onceligi.
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5) degerinden sayisal olarak
 * kucuk olamaz; 6 secildi ki 5 ile aralarinda bir seviye bos kalsin. */
#define APP_IRQ_PRIORITY   6U

void Error_Handler(void);

#endif /* MAIN_H */
