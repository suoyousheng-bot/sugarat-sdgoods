/* SPDX-License-Identifier: Apache-2.0 */
/** Telemetry acceptance and bounded history; stale/offline readings never become zeros. */
#include "data_model.h"
#include <assert.h>

static void append(sd_model_t *m, uint64_t now, bool valid, int16_t value)
{
    m->history[m->next] = (sd_point_t){value, valid, now};
    m->next = (m->next + 1) % SD_HISTORY_CAPACITY;
    if (m->count < SD_HISTORY_CAPACITY) ++m->count;
}

bool sd_model_init(sd_model_t *m, sd_limits_t c)
{
    if (c.minimum_tenths >= c.maximum_tenths || c.alarm_tenths > c.maximum_tenths ||
        c.hysteresis_tenths <= 0 || (int32_t)c.alarm_tenths - c.hysteresis_tenths < c.minimum_tenths ||
        c.stale_after_ms == 0) return false;
    *m = (sd_model_t){.limits = c, .status = SD_WAITING};
    return true;
}

static bool ordered(const sd_model_t *m, uint64_t now)
{
    return !m->count || now >= sd_model_point(m, m->count - 1).at_ms;
}

bool sd_model_push(sd_model_t *m, sd_sample_t sample, uint64_t now)
{
    if (!ordered(m, now)) return false;
    if (sample.temperature_tenths < m->limits.minimum_tenths || sample.temperature_tenths > m->limits.maximum_tenths ||
        sample.humidity_percent > 100 || sample.battery_percent > 100) {
        m->status = SD_INVALID;
        append(m, now, false, 0);
        return false;
    }
    if (m->status == SD_LIVE) sd_model_tick(m, now);
    m->sample = sample;
    m->received_ms = now;
    m->has_sample = true;
    m->status = SD_LIVE;
    if (sample.temperature_tenths >= m->limits.alarm_tenths) m->alarm_active = true;
    else if (sample.temperature_tenths <= m->limits.alarm_tenths - m->limits.hysteresis_tenths) {
        m->alarm_active = false;
        m->acknowledged = false;
    }
    append(m, now, true, sample.temperature_tenths);
    return true;
}

void sd_model_disconnect(sd_model_t *m, uint64_t now)
{
    if (!ordered(m, now) || m->status == SD_OFFLINE) return;
    m->status = SD_OFFLINE;
    append(m, now, false, 0);
}

void sd_model_tick(sd_model_t *m, uint64_t now)
{
    if (m->status != SD_LIVE || now < m->received_ms || now - m->received_ms < m->limits.stale_after_ms) return;
    m->status = SD_STALE;
    append(m, now, false, 0);
}

void sd_model_acknowledge(sd_model_t *m) { if (m->alarm_active) m->acknowledged = true; }

sd_point_t sd_model_point(const sd_model_t *m, uint8_t index)
{
    assert(index < m->count);
    return m->history[(m->next + SD_HISTORY_CAPACITY - m->count + index) % SD_HISTORY_CAPACITY];
}
