/**
 * @file    stm32f4xx_hal_timebase_tim.c
 * @brief   HAL zaman tabani TIM6 uzerinden (HAL sablonu
 *          stm32f4xx_hal_timebase_tim_template.c temel alindi).
 *
 * Neden SysTick degil: SysTick FreeRTOS'un tick kaynagi. HAL de SysTick
 * kullansaydi iki taraf ayni sayacin yuklenme degerini ve onceligini
 * birbirinin uzerine yazardi. HAL_InitTick() zayif (weak) tanimli; burada
 * yeniden tanimlanarak HAL'in uwTick sayaci TIM6'ya baglanir.
 */
#include "main.h"
#include "app_button.h"

TIM_HandleTypeDef htim6;

/* HAL_Init() ve HAL_RCC_ClockConfig() tarafindan cagrilir (ikincisi yeni
 * APB1 frekansina gore prescaler'i yeniden hesaplamak icin). */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t flash_latency;
    uint32_t tim_clock;
    HAL_StatusTypeDef status;

    if (TickPriority >= (1UL << __NVIC_PRIO_BITS))
    {
        return HAL_ERROR;
    }

    __HAL_RCC_TIM6_CLK_ENABLE();

    /* APB1 boleni 1 degilse timer saati PCLK1'in 2 katidir (RM0090, RCC saat agaci). */
    HAL_RCC_GetClockConfig(&clkconfig, &flash_latency);
    if (clkconfig.APB1CLKDivider == RCC_HCLK_DIV1)
    {
        tim_clock = HAL_RCC_GetPCLK1Freq();
    }
    else
    {
        tim_clock = 2U * HAL_RCC_GetPCLK1Freq();
    }

    /* 1 MHz sayac, 1000 sayimda tasma -> 1 kHz kesme. */
    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = (tim_clock / 1000000U) - 1U;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = 1000U - 1U;
    htim6.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    status = HAL_TIM_Base_Init(&htim6);
    if (status == HAL_OK)
    {
        /* Onceligi IRQ acilmadan once ayarla (sablon tersini yapiyor; kisa bir
         * an varsayilan oncelik 0 ile calisma riski olmasin). */
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
        uwTickPrio = TickPriority;
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
        status = HAL_TIM_Base_Start_IT(&htim6);
    }
    return status;
}

void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

/* TIM6_DAC_IRQHandler -> HAL_TIM_IRQHandler(&htim6) -> buraya gelir.
 * TIM2 kesmesiz calistigi icin su an baska kaynak yok; yine de instance
 * kontrol ediliyor ki ileride baska bir TIM kesmesi eklenirse tick bozulmasin.
 * CubeMX'te bu, main.c'deki "USER CODE BEGIN Callback 1" bolumune yazilirdi. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        HAL_IncTick();
        app_button_tick_1ms();   /* buton birakma takibi (1 kHz ornekleme) */
    }
}
