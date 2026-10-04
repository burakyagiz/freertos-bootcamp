/**
 * @file    freertos_hooks.c
 * @brief   FreeRTOSConfig.h'de acilan hook'larin uygulamasi.
 */
#include "FreeRTOS.h"
#include "task.h"

/* configCHECK_FOR_STACK_OVERFLOW = 2: her baglam degisiminde yigin isaretcisi
 * ve yigin sonundaki desen kontrol edilir; bozulma varsa burasi cagrilir. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;  /* debugger'da hangi gorevin tastigini gosterir */
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

/* configUSE_MALLOC_FAILED_HOOK = 1: pvPortMalloc NULL donerse (heap_4 doldu). */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
