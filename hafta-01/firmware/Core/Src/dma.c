/**
 * @file    dma.c
 * @brief   DMA1 saati ve Stream6 kesmesi.
 *
 * MX_USART2_UART_Init'ten ONCE cagrilmali: HAL_UART_MspInit icindeki
 * HAL_DMA_Init, DMA1 saati acik degilse stream registerlarina yazamaz.
 */
#include "dma.h"
#include "main.h"

void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA TC kesmesi HAL icinde USART TC kesmesini tetikler; zincirin sonunda
     * HAL_UART_TxCpltCallback'ten FreeRTOS bildirimi gidecek -> oncelik >= 5. */
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, APP_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}
