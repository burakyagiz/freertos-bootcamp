/**
 * @file    app_record.h
 * @brief   Olcum kayit havuzu: her kabul edilen buton olayi icin t0..t4 + durum.
 *
 * Slot = event_id % RECORD_POOL_LEN. Olcum sirasinda sadece RAM'e yazilir;
 * disa aktarma deney bitince yapilir (Faz 4).
 *
 * Alan sahipligi (her alani TEK bir yer yazar):
 *   event_id, t0, status=btn_drop  -> buton ISR (HAL_GPIO_EXTI_Callback)
 *   t1, t2, status=tx_drop         -> ButtonTask
 *   t3, status=tx_error/timeout    -> UartTxTask (tx_error/timeout Faz 3)
 *   t4, status=ok                  -> HAL_UART_TxCpltCallback (Faz 3)
 *
 * Neden kilit gerekmiyor:
 *  1. Her alanin tek yazari var.
 *  2. Ayni olayin yazarlari zamanda sirali: olay kuyruklarla elden ele gecer
 *     (ISR -> buttonQ -> ButtonTask -> txQ -> UartTxTask -> TC). Bir sonraki
 *     yazar, oncekinin isi bitmeden olayi eline alamaz.
 *  3. Hizali 32-bit yazma Cortex-M4'te tek komuttur. "Gecerli" bayraklari
 *     ayni baytin bitleri degil, ayri baytlardir: iki yazar birbirinin bitini
 *     oku-degistir-yaz ile silemez.
 * Tek istisna kayit tasmasi: slot hala aciktayken RECORD_POOL_LEN olay sonra
 * yeniden kullanilirsa g_record_overflow artar ve eski olayin gec gelen
 * yazmalari event_id kontrolu ile atilir. Normal deneyde olmamali.
 */
#ifndef APP_RECORD_H
#define APP_RECORD_H

#include <stdint.h>

#define RECORD_POOL_LEN  64U   /* 2'nin kuvveti: % islemi tek AND komutuna doner */

typedef enum
{
    REC_T0 = 0,   /* buton ISR girisi (filtre kabul ettiyse) */
    REC_T1,       /* ButtonTask xQueueReceive dondukten hemen sonra */
    REC_T2,       /* xQueueSend(txQ) cagrisindan hemen once */
    REC_T3,       /* UART IT/DMA baslatma cagrisindan hemen once */
    REC_T4,       /* HAL_UART_TxCpltCallback (Faz 3) */
    REC_T_COUNT
} RecordTime;

typedef enum
{
    REC_STATUS_OPEN = 0,   /* olay suruyor; disa aktarimda status bos */
    REC_STATUS_OK,
    REC_STATUS_BTN_DROP,
    REC_STATUS_TX_DROP,
    REC_STATUS_TX_ERROR,
    REC_STATUS_TIMEOUT,
} RecordStatus;

typedef struct
{
    uint32_t event_id;              /* 0: slot hic kullanilmadi (id'ler 1'den baslar) */
    uint32_t t_us[REC_T_COUNT];     /* TIM2 zaman damgalari */
    uint8_t  has_t[REC_T_COUNT];    /* 1: t_us[i] gecerli; 0: bos (0 us DEGIL) */
    uint8_t  status;                /* RecordStatus */
} Record;

/* Debugger'da g_records[event_id % 64] ile incelenebilir. */
extern Record g_records[RECORD_POOL_LEN];
extern volatile uint32_t g_record_overflow;

/* Yeni olay icin slotu hazirlar ve t0'i yazar. SADECE buton ISR'i cagirir. */
void record_open(uint32_t event_id, uint32_t t0_us);

/* t1..t4'ten birini yazar. Slot artik baska bir olaya aitse yazmaz. */
void record_set_time(uint32_t event_id, RecordTime which, uint32_t t_us);

/* Olayin son durumunu yazar (kaydi kapatir). Slot baskasina aitse yazmaz. */
void record_close(uint32_t event_id, RecordStatus status);

#endif /* APP_RECORD_H */
