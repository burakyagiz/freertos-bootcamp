/**
 * @file    tim.c
 * @brief   TIM2: 32-bit serbest sayan 1 MHz zaman damgasi (kesme yok).
 */
#include "tim.h"

TIM_HandleTypeDef htim2;

void MX_TIM2_Init(void)
{
    /* APB1 timer saati 84 MHz -> PSC 83 ile 84 MHz / (83+1) = 1 MHz = 1 us/sayim. */
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 83;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFFFFFFU;  /* tam 32-bit sarma */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    /* HAL_TIM_Base_Init, EGR.UG ile bir update olayi uretir; PSC tamponlu
     * oldugu icin yeni prescaler ancak bu sayede hemen gecerli olur. */
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    /* CubeMX gibi: burada sadece ayar yapilir, sayac baslatilmaz.
     * Baslatma app_time_init() icinde (App/Src/app_time.c). */
}

/* HAL_TIM_Base_Init ilk cagrida bunu cagirir. TIM6 (timebase) de buraya
 * duser ama TIM6'nin saati HAL_InitTick'te acildigi icin burada islem yok. */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}
