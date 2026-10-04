/**
 * @file    usart.h
 * @brief   USART2 (PA2 TX / PA3 RX, 115200 8N1) + DMA1 Stream6 Channel4 TX.
 */
#ifndef USART_H
#define USART_H

#include "main.h"

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef  hdma_usart2_tx;

void MX_USART2_UART_Init(void);

#endif /* USART_H */
