/**
 * @file    app_uart_tx.c
 * @brief   UartTxTask (oncelik 1). En dusuk oncelikte oldugu icin sadece
 *          Telemetry ve Button gorevleri bloklu iken calisir.
 */
#include "app_uart_tx.h"
#include "app_msg.h"
#include "app_record.h"
#include "app_time.h"

volatile UartTxStats g_uart_tx_stats;

static QueueHandle_t s_tx_queue;

void app_uart_tx_init(QueueHandle_t tx_queue)
{
    s_tx_queue = tx_queue;
}

void uart_tx_task(void *argument)
{
    (void)argument;
    TxMsg msg;

    for (;;)
    {
        /* Mesaj gelene kadar CPU harcamadan bloklu bekler. */
        if (xQueueReceive(s_tx_queue, &msg, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        if (msg.type == MSG_TYPE_BTN)
        {
            g_uart_tx_stats.rx_btn++;

            /* t3: UART baslatma cagrisindan hemen once. Faz 3'te
             * HAL_UART_Transmit_DMA bu satirin hemen altina gelecek. */
            record_set_time(msg.event_id, REC_T3, timer_us());
        }
        else
        {
            g_uart_tx_stats.rx_tel++;
        }

        /* Faz 2: gercek gonderim yok. BTN kaydinda t4 ve status bos kalir
         * (kayit "acik"); Faz 3'te TC callback'i kaydi kapatacak. */
    }
}
