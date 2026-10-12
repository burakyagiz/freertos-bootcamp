/**
 * @file    app_tasks.h
 * @brief   Uygulama mimarisinin kurulumu: 2 kuyruk + 3 gorev.
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

/* buttonQ ve txQ'yu, TelemetryTask/ButtonTask/UartTxTask'i olusturur ve
 * modullere kuyruklarini baglar. vTaskStartScheduler'dan once bir kez
 * cagrilir; herhangi bir olusturma basarisiz olursa Error_Handler'da durur. */
void app_tasks_create(void);

#endif /* APP_TASKS_H */
