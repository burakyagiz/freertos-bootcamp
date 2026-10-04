/**
 * @file    tim.h
 * @brief   TIM2: 1 MHz serbest sayan 32-bit zaman damgasi kaynagi (sadece ayar).
 *          Baslatma ve timer_us() uygulama katmaninda: App/Inc/app_time.h.
 */
#ifndef TIM_H
#define TIM_H

#include "main.h"

extern TIM_HandleTypeDef htim2;

void MX_TIM2_Init(void);

#endif /* TIM_H */
