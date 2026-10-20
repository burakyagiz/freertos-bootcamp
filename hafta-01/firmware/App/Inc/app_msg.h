/**
 * @file    app_msg.h
 * @brief   txQ mesaji (TxMsg) ve 64 baytlik sabit uzunluklu satir bicimleyici.
 *
 * Satir bicimi: ASCII metin + bosluklarla 63 bayta tamamlama + son bayt LF.
 * Sabit uzunluk her mesajin UART hattinda ayni sureyi (64 bayt @115200
 * ~5.6 ms) almasini saglar; PC tarafi da satiri LF ile ayirip 64 bayt
 * oldugunu dogrulayabilir.
 */
#ifndef APP_MSG_H
#define APP_MSG_H

#include <stdbool.h>
#include <stdint.h>

#define MSG_LEN        64U
#define MSG_TEXT_MAX   (MSG_LEN - 1U)   /* 63: son bayt LF'ye ayrildi */

typedef enum
{
    MSG_TYPE_TEL    = 1,
    MSG_TYPE_BTN    = 2,
    MSG_TYPE_CAL    = 3,   /* acilista kalibrasyon sonucu (64 bayt, isinmada) */
    MSG_TYPE_EXPORT = 4,   /* hatta gitmez: UartTxTask'a "olcum bitti, disa aktar" isareti */
} MsgType;

/* txQ'ya DEGER olarak kopyalanir: icinde pointer yok, gonderenin yerel
 * degiskeni sonradan degisse de kuyruktaki kopya bozulmaz. */
typedef struct
{
    uint8_t  data[MSG_LEN];   /* hatta gidecek 64 bayt */
    uint8_t  type;            /* MsgType */
    uint32_t event_id;        /* BTN: olay kimligi (kayit havuzu anahtari); TEL: sira no */
} TxMsg;

/* fmt/... ile metni uretir, 63 bayta bosluklarla tamamlar, 64. bayta LF koyar.
 * Metin 63 bayttan uzunsa KESMEZ, false doner. Metinde yazdirilamayan
 * karakter (LF dahil) varsa da false doner: LF PC'de satir ayiracidir.
 * false donerse msg icerigi gecersizdir, gonderilmemelidir.
 * format attribute'u sayesinde derleyici fmt ile argumanlari karsilastirir. */
bool msg_format(TxMsg *msg, MsgType type, uint32_t event_id, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

#endif /* APP_MSG_H */
