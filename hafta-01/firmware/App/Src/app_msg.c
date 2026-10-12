/**
 * @file    app_msg.c
 * @brief   64 baytlik mesaj bicimleyici.
 *
 * vsnprintf (newlib-nano) sadece tamsayi/metin bicimleriyle kullanilir. Bu
 * durumda bellek ayirmaz ve paylasilan durum tutmaz; bu yuzden TelemetryTask
 * ve ButtonTask ayni anda (birbirini keserek) cagirabilir. Ondalik (%f)
 * kullanma: nano'da desteklenmez, tam newlib'de ise malloc'a gidebilir.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app_msg.h"

bool msg_format(TxMsg *msg, MsgType type, uint32_t event_id, const char *fmt, ...)
{
    /* Dogrudan msg->data'ya yaz: en fazla 63 karakter + '\0' (64 bayt). */
    va_list args;
    va_start(args, fmt);
    const int len = vsnprintf((char *)msg->data, MSG_LEN, fmt, args);
    va_end(args);

    /* len, metin kesilmeseydi yazilacak karakter sayisidir. 63'ten buyukse
     * metin sigmadi; kesilmis hali sessizce gonderilmez, hata donulur. */
    if ((len < 0) || ((uint32_t)len > MSG_TEXT_MAX))
    {
        return false;
    }

    /* Sadece yazdirilabilir ASCII (0x20..0x7E): LF/CR gibi kontrol
     * karakterleri PC'deki satir cercevelemeyi bozardi. */
    for (int i = 0; i < len; i++)
    {
        const uint8_t c = msg->data[i];
        if ((c < 0x20U) || (c > 0x7EU))
        {
            return false;
        }
    }

    /* '\0' dahil kalan kismi bosluga cevir, son bayta LF koy. */
    memset(&msg->data[len], ' ', MSG_TEXT_MAX - (uint32_t)len);
    msg->data[MSG_LEN - 1U] = '\n';

    msg->type     = (uint8_t)type;
    msg->event_id = event_id;
    return true;
}
