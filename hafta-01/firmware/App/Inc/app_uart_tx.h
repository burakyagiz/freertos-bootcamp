/**
 * @file    app_uart_tx.h
 * @brief   UartTxTask: txQ'daki mesajlari UART'a veren TEK gorev.
 *
 * Kural: USART2'ye sadece bu modul dokunur. Uretici gorevler (Telemetry,
 * Button) mesajlarini txQ'ya birakir; tek tuketici sirayla gonderir. Boylece
 * mesajlar karismaz ve UART icin mutex gerekmez.
 *
 * Faz 2: mesaj alinir, BTN icin t3 yazilir; gercek gonderim Faz 3'te.
 */
#ifndef APP_UART_TX_H
#define APP_UART_TX_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

/* Her sayacin tek yazari UartTxTask. */
typedef struct
{
    uint32_t rx_tel;   /* txQ'dan alinan TEL mesaji */
    uint32_t rx_btn;   /* txQ'dan alinan BTN mesaji */
} UartTxStats;

extern volatile UartTxStats g_uart_tx_stats;

/* Mesajlarin alinacagi kuyrugu baglar; scheduler'dan once bir kez. */
void app_uart_tx_init(QueueHandle_t tx_queue);

/* FreeRTOS gorev fonksiyonu (oncelik UART_TX_TASK_PRIORITY). */
void uart_tx_task(void *argument);

#endif /* APP_UART_TX_H */
