/**
 * @file    usart.c
 * @brief   USART2: PA2 TX / PA3 RX (AF7), 115200 8N1, TX icin DMA1 Stream6 Ch4.
 */
#include "usart.h"

UART_HandleTypeDef huart2;
DMA_HandleTypeDef  hdma_usart2_tx;

void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    /* HAL_UART_Init -> HAL_UART_MspInit (asagida) -> BRR hesaplama (PCLK1=42 MHz) */
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance != USART2)
    {
        return;
    }

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA2 = USART2_TX, PA3 = USART2_RX; DS8626 alternate function tablosunda AF7. */
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* DMA1 Stream6 Channel4 = USART2_TX (RM0090, DMA1 request mapping tablosu). */
    hdma_usart2_tx.Instance                 = DMA1_Stream6;
    hdma_usart2_tx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;   /* hep USART2->DR */
    hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;    /* tampon uzerinde ilerle */
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode                = DMA_NORMAL;         /* her mesaj tek seferlik */
    hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_usart2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE; /* dogrudan mod */
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK)
    {
        Error_Handler();
    }

    /* Iki yonlu baglanti: huart2.hdmatx = &hdma, hdma.Parent = &huart2.
     * HAL_UART_Transmit_DMA ve DMA callback'leri birbirini bu sekilde bulur. */
    __HAL_LINKDMA(huart, hdmatx, hdma_usart2_tx);

    /* DMA modunda bile USART2 kesmesi gerekli: DMA bitince HAL TC kesmesini
     * acar, son bayt hattan cikinca HAL_UART_TxCpltCallback cagrilir. */
    HAL_NVIC_SetPriority(USART2_IRQn, APP_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}
