/**
 * @file    stm32f4xx_hal_conf.h
 * @brief   HAL yapilandirmasi. stm32f4xx_hal_conf_template.h (HAL v1.8.5)
 *          temel alindi; sadece projede kullanilan moduller acik.
 */
#ifndef STM32F4xx_HAL_CONF_H
#define STM32F4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Modul secimi ################################## */
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED

/* ########################## Osilator degerleri ############################ */
/* HSE_VALUE derleyiciden (-DHSE_VALUE=8000000) gelir; burasi sadece yedek. */
#if !defined(HSE_VALUE)
  #define HSE_VALUE              8000000U   /* Discovery X2 kristali: 8 MHz */
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100U       /* ms */
#endif
#if !defined(HSI_VALUE)
  #define HSI_VALUE              16000000U
#endif
#if !defined(LSI_VALUE)
  #define LSI_VALUE              32000U
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE              32768U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    5000U      /* ms */
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE   12288000U  /* I2S_CKIN; kullanilmiyor ama RCC_ex referans veriyor */
#endif

/* ########################## Sistem ayarlari ############################### */
#define VDD_VALUE                    3300U  /* mV */
/* HAL tick'i TIM6'dan gelir. En dusuk oncelik (15): olcum ISR'larini (EXTI0,
 * USART2, DMA) asla kesmesin, t0 olcumune jitter eklemesin. */
#define TICK_INT_PRIORITY            15U
#define USE_RTOS                     0U     /* HAL'de 1 desteklenmiyor */
#define PREFETCH_ENABLE              1U
#define INSTRUCTION_CACHE_ENABLE     1U
#define DATA_CACHE_ENABLE            1U

#define USE_HAL_UART_REGISTER_CALLBACKS   0U  /* zayif (weak) callback'ler kullanilir */
#define USE_HAL_TIM_REGISTER_CALLBACKS    0U

/* ########################## assert_param ################################## */
/* #define USE_FULL_ASSERT    1U */

/* ########################## Include'lar ################################### */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32f4xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32f4xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32f4xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32f4xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32f4xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32f4xx_hal_pwr.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
  #include "stm32f4xx_hal_tim.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32f4xx_hal_uart.h"
#endif

#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32F4xx_HAL_CONF_H */
