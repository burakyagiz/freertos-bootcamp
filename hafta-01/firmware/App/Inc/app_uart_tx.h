/**
 * @file    app_uart_tx.h
 * @brief   UartTxTask: txQ'daki mesajlari UART'a veren TEK gorev.
 *
 * Kural: USART2'ye sadece bu modul dokunur. Uretici gorevler (Telemetry,
 * Button) mesajlarini txQ'ya birakir; tek tuketici sirayla gonderir. Boylece
 * mesajlar karismaz ve UART icin mutex gerekmez.
 *
 * Gonderim DMA ile yapilir; gorev aktarim bitene kadar (en fazla
 * UART_TX_TIMEOUT_MS) task notification ile bloklu bekler. HAL_UART_TxCpltCallback
 * ve HAL_UART_ErrorCallback da bu modulde tanimlidir.
 */
#ifndef APP_UART_TX_H
#define APP_UART_TX_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

/* Sayaclarin yazari UartTxTask; last_error_code'u ErrorCallback yazar.
 * Kontrol: aktarim yokken rx_tel + rx_btn =
 *          tx_done + start_fail + tx_error + tx_timeout. */
typedef struct
{
    uint32_t rx_tel;          /* txQ'dan alinan TEL mesaji */
    uint32_t rx_btn;          /* txQ'dan alinan BTN mesaji */
    uint32_t tx_done;         /* TxCpltCallback ile biten aktarim (TEL + BTN) */
    uint32_t start_fail;      /* HAL_UART_Transmit_DMA HAL_OK donmedi */
    uint32_t tx_error;        /* HAL_UART_ErrorCallback geldi */
    uint32_t tx_timeout;      /* UART_TX_TIMEOUT_MS icinde bitmedi */
    uint32_t last_error_code; /* son ErrorCallback'teki huart->ErrorCode */
} UartTxStats;

extern volatile UartTxStats g_uart_tx_stats;

/* Mesajlarin alinacagi kuyrugu baglar; scheduler'dan once bir kez. */
void app_uart_tx_init(QueueHandle_t tx_queue);

/* FreeRTOS gorev fonksiyonu (oncelik UART_TX_TASK_PRIORITY). */
void uart_tx_task(void *argument);

#endif /* APP_UART_TX_H */
