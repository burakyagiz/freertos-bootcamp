/**
 * @file    app_tasks.c
 * @brief   Kuyruk ve gorevlerin olusturulmasi; mimari tek bakista burada.
 *
 *   [Buton ISR] --buttonQ--> [ButtonTask p2] --+
 *                                              +--txQ--> [UartTxTask p1] --> USART2
 *   [TelemetryTask p3] ---- TEL mesajlari -----+
 */
#include "app_tasks.h"
#include "app_config.h"
#include "app_button.h"
#include "app_msg.h"
#include "app_telemetry.h"
#include "app_uart_tx.h"
#include "main.h"

#include "queue.h"

/* Oncelik duzeni derleme aninda dogrulanir. FreeRTOS gecersiz bir onceligi
 * (>= configMAX_PRIORITIES) calisma aninda configASSERT ile yakalar; assert
 * kapali olsaydi sessizce en yuksek oncelige kirpardi. */
_Static_assert(configMAX_PRIORITIES >= 4,
               "idle + 3 uygulama onceligi icin en az 4 seviye gerekir");
_Static_assert(TELEMETRY_TASK_PRIORITY < configMAX_PRIORITIES,
               "TelemetryTask onceligi configMAX_PRIORITIES'i asiyor");
_Static_assert((TELEMETRY_TASK_PRIORITY > BUTTON_TASK_PRIORITY) &&
               (BUTTON_TASK_PRIORITY > UART_TX_TASK_PRIORITY) &&
               (UART_TX_TASK_PRIORITY > tskIDLE_PRIORITY),
               "oncelik sirasi Telemetry > Button > UartTx > idle olmali");

static QueueHandle_t s_button_queue;
static QueueHandle_t s_tx_queue;

static void create_task(TaskFunction_t function, const char *name,
                        configSTACK_DEPTH_TYPE stack_words, UBaseType_t priority)
{
    /* Basarisizlik = heap yetmedi. Sessizce devam etmek eksik bir sistemle
     * olcum yapmak olurdu; bu yuzden dur. */
    if (xTaskCreate(function, name, stack_words, NULL, priority, NULL) != pdPASS)
    {
        Error_Handler();
    }
}

void app_tasks_create(void)
{
    /* FIFO kuyruklar; ogeler DEGER olarak kopyalanir. */
    s_button_queue = xQueueCreate(BUTTON_QUEUE_LENGTH, sizeof(ButtonEvent));
    s_tx_queue     = xQueueCreate(TX_QUEUE_LENGTH, sizeof(TxMsg));
    if ((s_button_queue == NULL) || (s_tx_queue == NULL))
    {
        Error_Handler();
    }
    /* CubeIDE debugger'inin kuyruk gorunumunde adlariyla cikarlar. */
    vQueueAddToRegistry(s_button_queue, "buttonQ");
    vQueueAddToRegistry(s_tx_queue, "txQ");

    app_button_init(s_button_queue, s_tx_queue);
    app_telemetry_init(s_tx_queue);
    app_uart_tx_init(s_tx_queue);

    create_task(telemetry_task, "Telemetry", TELEMETRY_TASK_STACK_WORDS, TELEMETRY_TASK_PRIORITY);
    create_task(button_task,    "Button",    BUTTON_TASK_STACK_WORDS,    BUTTON_TASK_PRIORITY);
    create_task(uart_tx_task,   "UartTx",    UART_TX_TASK_STACK_WORDS,   UART_TX_TASK_PRIORITY);
}
