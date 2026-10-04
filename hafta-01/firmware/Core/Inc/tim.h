/**
 * @file    tim.h
 * @brief   TIM2: 1 MHz serbest sayan 32-bit zaman damgasi kaynagi.
 */
#ifndef TIM_H
#define TIM_H

#include "main.h"

extern TIM_HandleTypeDef htim2;

void MX_TIM2_Init(void);

/* Mikrosaniye zaman damgasi. 32 bit, ~71.6 dakikada bir tasar; farklar
 * uint32 isaretsiz cikarma ile alinirsa tasma dogru hesaplanir. */
static inline uint32_t timer_us(void)
{
    return TIM2->CNT;
}

#endif /* TIM_H */
