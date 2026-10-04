/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS V11.3.1 yapilandirmasi (STM32F407, GCC ARM_CM4F portu).
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* configCPU_CLOCK_HZ calisma aninda SystemCoreClock'tan okunur
 * (SystemClock_Config sonrasi 168 MHz). */
#if defined(__GNUC__) || defined(__ICCARM__) || defined(__CC_ARM)
  #include <stdint.h>
  extern uint32_t SystemCoreClock;
#endif

/* ---------------------------------------------------------------- Zamanlayici */
#define configUSE_PREEMPTION                     1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1   /* CLZ komutu ile hazir-liste secimi */
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     5   /* 0=idle, 1..3 uygulama, 4 yedek */
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)   /* word = 512 B */
#define configMAX_TASK_NAME_LEN                  16
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATIONS             1   /* UartTxTask TC beklemesi icin */
#define configQUEUE_REGISTRY_SIZE                8   /* CubeIDE debugger kuyruk adlarini gosterir */
#define configUSE_MUTEXES                        0
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configUSE_TIMERS                         0   /* timers.c derlenmiyor */
#define configUSE_CO_ROUTINES                    0

/* ---------------------------------------------------------------- Bellek */
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configSUPPORT_STATIC_ALLOCATION          0
#define configTOTAL_HEAP_SIZE                    ((size_t)(30U * 1024U))  /* heap_4 */

/* ---------------------------------------------------------------- Hook / hata ayiklama */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2   /* sinir + 16 baytlik desen kontrolu */
#define configUSE_TRACE_FACILITY                 1   /* CubeIDE FreeRTOS Task List gorunumu icin */
#define configRECORD_STACK_HIGH_ADDRESS          1   /* CubeIDE yigin kullanimini gosterebilsin */

/* configASSERT: kosul yanlissa kesmeleri kapat ve dur; debugger'da cagri
 * yigini hatanin yerini gosterir. */
#define configASSERT(x)  if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;) {} }

/* ---------------------------------------------------------------- Dahil edilen API */
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskSuspend                     1   /* portMAX_DELAY = suresiz bekleme */
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_vTaskDelete                      0
#define INCLUDE_vTaskPrioritySet                 0
#define INCLUDE_uxTaskPriorityGet                0

/* ---------------------------------------------------------------- Cortex-M kesme oncelikleri */
#ifdef __NVIC_PRIO_BITS
  #define configPRIO_BITS                        __NVIC_PRIO_BITS
#else
  #define configPRIO_BITS                        4   /* STM32F4: 16 seviye */
#endif

/* En dusuk oncelik (15): PendSV ve SysTick bu seviyede calisir. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15
/* FromISR API cagirabilecek en yuksek oncelik. 0..4 arasi kesmeler
 * FreeRTOS tarafindan hic maskelenmez ama API de cagiramaz. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5

/* NVIC registerlarina yazilacak ham degerler (ust 4 bit kullaniliyor). */
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---------------------------------------------------------------- Handler eslemesi */
/* port.c bu uc handler'i tanimlar; burada vektor tablosundaki CMSIS adlarina
 * esleniyor. stm32f4xx_it.c'de SVC/PendSV/SysTick TANIMLANMAZ (cift tanim
 * olmasin). HAL tick'i TIM6'da oldugu icin SysTick tamamen FreeRTOS'a ait. */
#define vPortSVCHandler      SVC_Handler
#define xPortPendSVHandler   PendSV_Handler
#define xPortSysTickHandler  SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
