/**
 * @file    app_experiment.h
 * @brief   Deney akisi (isinma -> olcum -> durdurma -> disa aktarma) ve
 *          disa aktarma satirlarinin bicimi.
 *
 * Akis (sureler ve hedef app_config.h'de):
 *  1. Isinma: scheduler basladiktan sonraki EXPERIMENT_WARMUP_MS boyunca
 *     telemetri ve yuk calisir, basislar kaydedilmez (warmup_ignored).
 *  2. Olcum: EXPERIMENT_TARGET_EVENTS kabule kadar. Hedefe ulasan buton ISR'i
 *     durdurma ister; sonraki basislar kaydedilmez (late_ignored).
 *  3. Durdurma: ButtonTask buttonQ'yu bitirdikten EXPERIMENT_END_DELAY_MS
 *     sonra txQ'ya EXPORT isareti koyar. txQ FIFO'dur ve UartTxTask bir
 *     mesajin aktarimi bitmeden sonrakini almaz; bu yuzden isaret
 *     alindiginda her olcum olayinin kaydi kapanmistir. Telemetri bu ana
 *     kadar calisir: son olay da digerleriyle ayni yuk altinda olculur.
 *  4. Disa aktarma: UartTxTask telemetriyi durdurur, kuyrukta kalan TEL
 *     mesajlarini gonderir, sonra REC, CNT ve END satirlarini.
 *
 * Disa aktarma satirlari 64 bayt kuralinin disindadir; olcumu etkilemez:
 *  - 64 bayt kurali olculen mesajlar icindir: her BTN yaniti hatta ayni
 *    sureyi (t4 - t3) alsin, TEL yuku senaryolar arasinda sadece frekansla
 *    degissin.
 *  - REC/CNT/END ancak 3. adimdan sonra gonderilir: her olayin kaydi
 *    kapanmis, yeni olay kabulu kapanmis ve telemetri durmustur. Hicbir
 *    olculen aralikla zamanda cakismazlar; uzunluklari hicbir t degerine
 *    giremez. "Olcum sirasinda kayit yazdirma yok" kurali da boyle saglanir.
 *  - PC 64 bayt kontrolunu TEL/BTN/CAL satirlarinda yapar, REC/CNT/END'de
 *    yapmaz (satirin ilk alanindan ayirt eder).
 *
 * Satir bicimleri (hepsi LF ile biter):
 *   REC,S<n>,<event_id>,<t0>,<t1>,<t2>,<t3>,<t4>,<status>
 *       t degerleri ham TIM2 (us); eksik zaman bos alan (0 degil). Farklar
 *       analizde modulo 2^32 alinmali (TIM2 71.6 dakikada sifira doner).
 *       status: ok, btn_drop, tx_drop, tx_error, timeout (kapanmamis: bos).
 *   CNT,S<n>,ad=deger,ad=deger,...
 *       Olculmemis deger bos (orn. S0'da telemetri periyodu). tx_done,
 *       start_fail, tx_error, timeout TUM mesajlari (TEL dahil) sayar; BTN'e
 *       ozgu durumlar REC satirlarindadir.
 *   END,S<n>,rec=<REC satiri sayisi>
 */
#ifndef APP_EXPERIMENT_H
#define APP_EXPERIMENT_H

#include <stdbool.h>
#include <stdint.h>

/* Debugger'da g_experiment ile izlenebilir. Her alanin tek yazari var ve
 * sadece 0 -> 1 degisir. */
typedef struct
{
    uint8_t stop_requested;      /* buton ISR: hedef sayiya ulasildi */
    uint8_t telemetry_stopped;   /* UartTxTask: EXPORT isareti alindi */
    uint8_t exported;            /* UartTxTask: END satiri gonderildi */
} ExperimentState;

extern volatile ExperimentState g_experiment;

/* Isinma bitti mi? Scheduler baslangicindan beri gecen tick'e bakar; durum
 * tutmaz. Hem ISR'dan hem gorevden cagrilabilir. */
bool experiment_warmup_done(void);

/* SADECE buton ISR'i cagirir: hedef sayidaki olay kuyruga verildikten sonra. */
void experiment_request_stop(void);
bool experiment_stop_requested(void);

/* SADECE UartTxTask cagirir: EXPORT isareti alindiginda. */
void experiment_stop_telemetry(void);
bool experiment_telemetry_stopped(void);

/* SADECE UartTxTask cagirir: son satir gonderildiginde. */
void experiment_mark_exported(void);

/* index'inci disa aktarma satirini buf'a yazar (LF dahil) ve uzunlugunu
 * doner. Sira: REC satirlari (olay sirasiyla), CNT, END. Satir kalmadiysa 0.
 * Sadece olcum bittikten sonra cagrilmali (kayitlar artik degismez). */
uint32_t experiment_export_line(uint32_t index, char *buf, uint32_t size);

#endif /* APP_EXPERIMENT_H */
