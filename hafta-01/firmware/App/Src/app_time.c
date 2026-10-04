/**
 * @file    app_time.c
 * @brief   TIM2 zaman kaynaginin baslatilmasi.
 */
#include "app_time.h"
#include "tim.h"

void app_time_init(void)
{
    /* _IT degil: tasma kesmesine gerek yok, sadece CNT okunacak. Kesmesiz
     * oldugu icin olcum ISR'larina ek yuk/jitter getirmez. */
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        Error_Handler();
    }
}
