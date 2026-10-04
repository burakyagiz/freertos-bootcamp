/**
 * @file    system_clock.c
 * @brief   Saat agaci: HSE 8 MHz -> PLL -> SYSCLK 168 MHz.
 *
 *   HSE 8 MHz / PLLM 8  = 1 MHz   (VCO girisi; RM0090: 1..2 MHz araliginda olmali)
 *   1 MHz * PLLN 336    = 336 MHz (VCO cikisi; 100..432 MHz)
 *   336 / PLLP 2        = 168 MHz SYSCLK
 *   336 / PLLQ 7        = 48 MHz  (USB OTG FS/SDIO/RNG; kullanilmasa da gecerli tutuldu)
 *   AHB  /1 = HCLK 168 MHz
 *   APB1 /4 = PCLK1 42 MHz  (maks. 42) -> APB1 timer saati 84 MHz (TIM2, TIM6)
 *   APB2 /2 = PCLK2 84 MHz  (maks. 84)
 */
#include "system_clock.h"
#include "main.h"

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* 168 MHz icin regulator Scale 1 sart (RM0090, PWR bolumu). PWR saati acilmadan
     * VOS bitine yazilamaz. */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;          /* kristal (bypass degil) */
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 336;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;

    /* 2.7-3.6 V'ta 150 < HCLK <= 168 MHz icin 5 bekleme durumu (RM0090, Flash bolumu,
     * HCLK frekansina gore wait state tablosu).
     * HAL, frekansi yukseltmeden ONCE latency'yi arttirir. */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}
