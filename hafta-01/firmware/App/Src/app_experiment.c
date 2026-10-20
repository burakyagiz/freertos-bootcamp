/**
 * @file    app_experiment.c
 * @brief   Deney akisi bayraklari ve disa aktarma satirlari. Akis ve
 *          satir bicimleri app_experiment.h'de.
 *
 * Bu modul UART'a dokunmaz: satirlari sadece metin olarak uretir, hatta
 * UartTxTask gonderir.
 */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include "app_experiment.h"
#include "app_button.h"
#include "app_config.h"
#include "app_record.h"
#include "app_telemetry.h"
#include "app_uart_tx.h"

#include "FreeRTOS.h"
#include "task.h"

_Static_assert(EXPERIMENT_TARGET_EVENTS <= RECORD_POOL_LEN,
               "hedef olay sayisi kayit havuzuna sigmali, yoksa eski kayitlar ezilir");

volatile ExperimentState g_experiment;

bool experiment_warmup_done(void)
{
    /* xTickCount scheduler basinda 0'dir ve 1 ms'de bir artar. FromISR
     * surumu gorevden de guvenle cagrilir: ISR disinda oncelik kontrolu
     * atlanir (vPortValidateInterruptPriority) ve 32-bit okuma tek komuttur. */
    return xTaskGetTickCountFromISR() >= pdMS_TO_TICKS(EXPERIMENT_WARMUP_MS);
}

void experiment_request_stop(void)
{
    g_experiment.stop_requested = 1U;
}

bool experiment_stop_requested(void)
{
    return g_experiment.stop_requested != 0U;
}

void experiment_stop_telemetry(void)
{
    g_experiment.telemetry_stopped = 1U;
}

bool experiment_telemetry_stopped(void)
{
    return g_experiment.telemetry_stopped != 0U;
}

void experiment_mark_exported(void)
{
    g_experiment.exported = 1U;
}

/* ------------------------------------------------------- satir olusturma */

/* Tampona parca parca yazar. Sigmazsa ok = false olur ve sonraki eklemeler
 * yok sayilir; yarim satir gonderilmez. */
typedef struct
{
    char    *buf;
    uint32_t size;
    uint32_t len;
    bool     ok;
} Line;

static void line_append(Line *line, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void line_append(Line *line, const char *fmt, ...)
{
    if (!line->ok)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(&line->buf[line->len], line->size - line->len, fmt, args);
    va_end(args);

    if ((n < 0) || ((uint32_t)n >= (line->size - line->len)))
    {
        line->ok = false;
        return;
    }
    line->len += (uint32_t)n;
}

static uint32_t line_finish(Line *line)
{
    line_append(line, "\n");
    if (!line->ok)
    {
        /* Tampon boyutu (app_uart_tx.c) satirlara gore secildi; buraya
         * gelinmesi programlama hatasidir. */
        configASSERT(0);
        return 0U;
    }
    return line->len;
}

/* Disa aktarilacak olaylar: id'ler 1..accepted (id sadece kaydedilen olaya
 * verilir). Havuz tasmissa sadece son RECORD_POOL_LEN olay havuzda kalir. */
static uint32_t record_range(uint32_t *first_id)
{
    const uint32_t last_id = g_button_stats.accepted;
    if (last_id == 0U)
    {
        *first_id = 1U;
        return 0U;
    }
    *first_id = (last_id > RECORD_POOL_LEN) ? (last_id - RECORD_POOL_LEN + 1U) : 1U;
    return last_id - *first_id + 1U;
}

static const char *status_name(uint8_t status)
{
    switch (status)
    {
    case REC_STATUS_OK:       return "ok";
    case REC_STATUS_BTN_DROP: return "btn_drop";
    case REC_STATUS_TX_DROP:  return "tx_drop";
    case REC_STATUS_TX_ERROR: return "tx_error";
    case REC_STATUS_TIMEOUT:  return "timeout";
    default:                  return "";   /* REC_STATUS_OPEN */
    }
}

static uint32_t format_rec(uint32_t event_id, char *buf, uint32_t size)
{
    Line line = { .buf = buf, .size = size, .len = 0U, .ok = true };
    const Record *rec = &g_records[event_id % RECORD_POOL_LEN];
    /* Slotta baska olay varsa (olmamali) bu olay icin bilinen bir sey yok:
     * zamanlar ve durum bos yazilir. */
    const bool own = (rec->event_id == event_id);

    line_append(&line, "REC,S%u,%" PRIu32, (unsigned)SCENARIO, event_id);
    for (uint32_t i = 0U; i < REC_T_COUNT; i++)
    {
        if (own && (rec->has_t[i] != 0U))
        {
            line_append(&line, ",%" PRIu32, rec->t_us[i]);
        }
        else
        {
            line_append(&line, ",");   /* eksik zaman: bos alan, 0 degil */
        }
    }
    line_append(&line, ",%s", own ? status_name(rec->status) : "");
    return line_finish(&line);
}

typedef struct
{
    const char *name;
    uint32_t    value;
    bool        valid;   /* false: olculmedi, deger bos yazilir */
} Counter;

static uint32_t avg_u32(uint64_t sum, uint32_t count)
{
    return (count == 0U) ? 0U : (uint32_t)(sum / count);
}

static uint32_t format_cnt(char *buf, uint32_t size)
{
    /* Okumalar olcum bittikten sonra: yazarlarin hepsi durmus, degerler
     * artik degismiyor. */
    const uint32_t period_n = g_telemetry_stats.period_count;
    const uint32_t work_n   = g_telemetry_stats.work_count;
    const uint32_t hwm_tel  = g_telemetry_stats.txq_max;
    const uint32_t hwm_btn  = g_button_stats.txq_max;

    const Counter counters[] = {
        { "accepted",          g_button_stats.accepted,        true },
        { "rise_edges",        g_button_stats.rise_edges,      true },
        { "filtered",          g_button_stats.filtered,        true },
        { "release_glitch",    g_button_stats.release_glitch,  true },
        { "warmup_ignored",    g_button_stats.warmup_ignored,  true },
        { "late_ignored",      g_button_stats.late_ignored,    true },
        { "btn_drop",          g_button_stats.btn_drop,        true },
        { "tx_drop",           g_button_stats.tx_drop,         true },
        { "tx_done",           g_uart_tx_stats.tx_done,        true },
        { "start_fail",        g_uart_tx_stats.start_fail,     true },
        { "tx_error",          g_uart_tx_stats.tx_error,       true },
        { "timeout",           g_uart_tx_stats.tx_timeout,     true },
        { "rec_overflow",      g_record_overflow,              true },
        { "txq_hwm",           (hwm_tel > hwm_btn) ? hwm_tel : hwm_btn, true },
        { "tel_sent",          g_telemetry_stats.sent,         true },
        { "tel_drop",          g_telemetry_stats.tel_drop,     true },
        { "tel_period_min_us", g_telemetry_stats.period_min_us, period_n > 0U },
        { "tel_period_avg_us", avg_u32(g_telemetry_stats.period_sum_us, period_n), period_n > 0U },
        { "tel_period_max_us", g_telemetry_stats.period_max_us, period_n > 0U },
        { "work_iterations",   TELEMETRY_WORK_ITERATIONS,      true },
        { "work_min_us",       g_telemetry_stats.work_min_us,  work_n > 0U },
        { "work_avg_us",       avg_u32(g_telemetry_stats.work_sum_us, work_n), work_n > 0U },
        { "work_max_us",       g_telemetry_stats.work_max_us,  work_n > 0U },
        { "cal_iter_per_ms",   g_calibration.iter_per_ms,      g_calibration.iter_per_ms > 0U },
    };

    Line line = { .buf = buf, .size = size, .len = 0U, .ok = true };
    line_append(&line, "CNT,S%u", (unsigned)SCENARIO);
    for (uint32_t i = 0U; i < (sizeof(counters) / sizeof(counters[0])); i++)
    {
        if (counters[i].valid)
        {
            line_append(&line, ",%s=%" PRIu32, counters[i].name, counters[i].value);
        }
        else
        {
            line_append(&line, ",%s=", counters[i].name);
        }
    }
    return line_finish(&line);
}

static uint32_t format_end(uint32_t rec_lines, char *buf, uint32_t size)
{
    Line line = { .buf = buf, .size = size, .len = 0U, .ok = true };
    line_append(&line, "END,S%u,rec=%" PRIu32, (unsigned)SCENARIO, rec_lines);
    return line_finish(&line);
}

uint32_t experiment_export_line(uint32_t index, char *buf, uint32_t size)
{
    uint32_t first_id;
    const uint32_t rec_lines = record_range(&first_id);

    if (index < rec_lines)
    {
        return format_rec(first_id + index, buf, size);
    }
    if (index == rec_lines)
    {
        return format_cnt(buf, size);
    }
    if (index == (rec_lines + 1U))
    {
        return format_end(rec_lines, buf, size);
    }
    return 0U;
}
