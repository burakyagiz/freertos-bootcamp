/**
 * @file    app_record.c
 * @brief   Kayit havuzu. Sahiplik kurallari ve kilitsiz calismanin nedeni
 *          app_record.h'de.
 */
#include <string.h>

#include "app_record.h"

Record g_records[RECORD_POOL_LEN];   /* .bss: acilista sifir = hepsi kullanilmamis */
volatile uint32_t g_record_overflow;

static inline Record *slot_of(uint32_t event_id)
{
    return &g_records[event_id % RECORD_POOL_LEN];
}

void record_open(uint32_t event_id, uint32_t t0_us)
{
    Record *rec = slot_of(event_id);

    /* Slottaki onceki olay henuz kapanmadiysa (status hala OPEN) onun kaydi
     * ezilecek: bunu gizleme, say. */
    if ((rec->event_id != 0U) && (rec->status == REC_STATUS_OPEN))
    {
        g_record_overflow++;
    }

    /* ISR'dan cagrildigi icin gorevler bu fonksiyonun ortasinda araya
     * giremez; alanlarin yazilma sirasi onemsiz. */
    memset(rec, 0, sizeof(*rec));
    rec->event_id      = event_id;
    rec->t_us[REC_T0]  = t0_us;
    rec->has_t[REC_T0] = 1U;
}

void record_set_time(uint32_t event_id, RecordTime which, uint32_t t_us)
{
    Record *rec = slot_of(event_id);

    /* Slot bu arada daha yeni bir olaya verildiyse (kayit tasmasi) eski
     * olayin zamani yeni olayin kaydina yazilmasin. */
    if (rec->event_id != event_id)
    {
        return;
    }
    rec->t_us[which]  = t_us;
    rec->has_t[which] = 1U;
}

void record_close(uint32_t event_id, RecordStatus status)
{
    Record *rec = slot_of(event_id);

    if (rec->event_id != event_id)
    {
        return;
    }
    rec->status = (uint8_t)status;
}
