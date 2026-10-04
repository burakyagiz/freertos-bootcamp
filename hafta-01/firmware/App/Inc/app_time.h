/**
 * @file    app_time.h
 * @brief   Uygulama zaman kaynagi: TIM2 tabanli 1 us cozunurluklu sayac.
 *
 * TIM2 ayari (PSC 83, ARR 0xFFFFFFFF) Core/Src/tim.c'deki MX_TIM2_Init'te
 * yapilir; bu modul sayaci baslatir ve okur.
 */
#ifndef APP_TIME_H
#define APP_TIME_H

#include <stdint.h>
#include "main.h"

/* TIM2 sayacini baslatir. MX_TIM2_Init'ten sonra, olcum yapilmadan once
 * bir kez cagrilir. */
void app_time_init(void);

/* Mikrosaniye zaman damgasi. Tek bir 32-bit register okumasi: kesme
 * icinden de gorevden de guvenle cagrilabilir. ~71.6 dakikada bir sifira
 * doner (2^32 us). */
static inline uint32_t timer_us(void)
{
    return TIM2->CNT;
}

/* start'tan end'e gecen sure (us). Isaretsiz cikarma modulo 2^32 calistigi
 * icin sayac arada sifira donmus olsa bile dogru sonuc verir; tek sart
 * gercek surenin 2^32 us'den kisa olmasi. ASLA "end < start + x" gibi
 * toplama ile karsilastirma yapma: toplam tasarsa sonuc yanlis olur. */
static inline uint32_t elapsed_us(uint32_t start, uint32_t end)
{
    return (uint32_t)(end - start);
}

#endif /* APP_TIME_H */
