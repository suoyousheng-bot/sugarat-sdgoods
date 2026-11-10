/*
 * Sugarat battery component: filtered voltage, percentage, level events and trend.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sugarat_battery.h"

#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdgoods_hw_info.h"

static const char *TAG = "sugarat_battery";

#define READS_PER_SAMPLE   5
#define MAX_AVERAGE        32
#define TREND_SLOTS        60
#define TREND_MIN_US       (30LL * 1000 * 1000)
#define PERCENT_SETTLE     5  /* samples a one-point change must persist before it is reported */

/* Typical single-cell Li-ion/LiPo rest voltage curve; replace with measurements of the fitted cell. */
static const sugarat_battery_point_t k_default_curve[] = {
    { 4200, 100 }, { 4150, 95 }, { 4110, 90 }, { 4080, 85 }, { 4020, 80 }, { 3980, 75 }, { 3950, 70 },
    { 3910, 65 }, { 3870, 60 }, { 3850, 55 }, { 3840, 50 }, { 3820, 45 }, { 3800, 40 }, { 3790, 35 },
    { 3770, 30 }, { 3750, 25 }, { 3730, 20 }, { 3710, 15 }, { 3690, 10 }, { 3610, 5 }, { 3300, 0 },
};

static sugarat_battery_config_t s_cfg;
static sugarat_battery_state_t s_state;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_done;
static volatile bool s_stop;

uint8_t sugarat_battery_percent_from_mv(uint16_t mv, const sugarat_battery_point_t *curve, size_t curve_len)
{
    if (!curve || curve_len < 2) {
        curve = k_default_curve;
        curve_len = sizeof(k_default_curve) / sizeof(k_default_curve[0]);
    }
    if (mv >= curve[0].mv) {
        return curve[0].percent;
    }
    for (size_t i = 1; i < curve_len; i++) {
        if (mv >= curve[i].mv) {
            const sugarat_battery_point_t hi = curve[i - 1];
            const sugarat_battery_point_t lo = curve[i];
            const uint32_t span = (uint32_t)(hi.mv - lo.mv);
            const uint32_t pos = (uint32_t)(mv - lo.mv);
            return (uint8_t)(lo.percent + ((hi.percent - lo.percent) * pos + span / 2) / span);
        }
    }
    return curve[curve_len - 1].percent;
}

static bool curve_valid(const sugarat_battery_point_t *curve, size_t len)
{
    if (!curve) {
        return true;
    }
    if (len < 2 || curve[0].percent != 100 || curve[len - 1].percent != 0) {
        return false;
    }
    for (size_t i = 1; i < len; i++) {
        if (curve[i].mv >= curve[i - 1].mv || curve[i].percent > curve[i - 1].percent) {
            return false;
        }
    }
    return true;
}

/* Median of the non-zero reads; sdgoods_hw_bat_v() returns 0 when the ADC is unavailable or busy. */
static uint16_t read_median_mv(void)
{
    uint16_t v[READS_PER_SAMPLE];
    int n = 0;
    for (int i = 0; i < READS_PER_SAMPLE; i++) {
        const float volts = sdgoods_hw_bat_v();
        if (volts > 0.5f && volts < 6.0f) {
            v[n++] = (uint16_t)(volts * 1000.0f + 0.5f);
        }
    }
    if (n == 0) {
        return 0;
    }
    for (int i = 1; i < n; i++) {
        const uint16_t key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
    return v[n / 2];
}

static sugarat_battery_level_t next_level(sugarat_battery_level_t cur, uint8_t pct)
{
    const uint8_t low = s_cfg.low_percent;
    const uint8_t crit = s_cfg.critical_percent;
    const uint8_t hyst = s_cfg.hysteresis_percent;
    switch (cur) {
    case SUGARAT_BATTERY_LEVEL_UNAVAILABLE:
        return pct <= crit ? SUGARAT_BATTERY_LEVEL_CRITICAL
             : pct <= low  ? SUGARAT_BATTERY_LEVEL_LOW : SUGARAT_BATTERY_LEVEL_OK;
    case SUGARAT_BATTERY_LEVEL_OK:
        return pct <= crit ? SUGARAT_BATTERY_LEVEL_CRITICAL
             : pct <= low  ? SUGARAT_BATTERY_LEVEL_LOW : cur;
    case SUGARAT_BATTERY_LEVEL_LOW:
        return pct <= crit ? SUGARAT_BATTERY_LEVEL_CRITICAL
             : pct >= low + hyst ? SUGARAT_BATTERY_LEVEL_OK : cur;
    case SUGARAT_BATTERY_LEVEL_CRITICAL:
        return pct >= low + hyst ? SUGARAT_BATTERY_LEVEL_OK
             : pct >= crit + hyst ? SUGARAT_BATTERY_LEVEL_LOW : cur;
    }
    return cur;
}

static void emit(sugarat_battery_event_type_t type)
{
    if (!s_cfg.on_event) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    const sugarat_battery_event_t ev = {
        .type = type, .percent = s_state.percent, .level = s_state.level,
        .usb_connected = s_state.usb_connected, .mv = s_state.average_mv,
    };
    portEXIT_CRITICAL(&s_lock);
    s_cfg.on_event(&ev, s_cfg.user_data);
}

static void battery_task(void *arg)
{
    (void)arg;
    uint16_t window[MAX_AVERAGE];
    uint32_t window_sum = 0;
    uint8_t window_n = 0;
    uint8_t window_pos = 0;
    int64_t trend_t[TREND_SLOTS];
    uint16_t trend_mv[TREND_SLOTS];
    uint8_t trend_n = 0;
    uint8_t trend_pos = 0;
    uint8_t pending = 0;
    bool have_percent = false;
    bool usb_known = false;
    TickType_t last_wake = xTaskGetTickCount();

    while (!s_stop) {
        const uint16_t mv = read_median_mv();
        const int64_t now = esp_timer_get_time();
        bool percent_changed = false;
        bool level_changed = false;
        bool usb_changed = false;

        if (s_cfg.monitor_usb) {
            const bool usb = usb_serial_jtag_is_connected();
            portENTER_CRITICAL(&s_lock);
            usb_changed = usb_known && usb != s_state.usb_connected;
            s_state.usb_connected = usb;
            portEXIT_CRITICAL(&s_lock);
            usb_known = true;
        }

        if (mv == 0) {
            portENTER_CRITICAL(&s_lock);
            s_state.failed_reads++;
            portEXIT_CRITICAL(&s_lock);
        } else {
            if (window_n == s_cfg.average_samples) {
                window_sum -= window[window_pos];
            } else {
                window_n++;
            }
            window[window_pos] = mv;
            window_sum += mv;
            window_pos = (uint8_t)((window_pos + 1) % s_cfg.average_samples);
            const uint16_t avg = (uint16_t)((window_sum + window_n / 2) / window_n);
            const uint8_t pct = sugarat_battery_percent_from_mv(avg, s_cfg.curve, s_cfg.curve_len);

            /* Trend over the oldest retained sample (up to TREND_SLOTS samples back). */
            const uint8_t oldest = trend_n == TREND_SLOTS ? trend_pos : 0;
            int32_t trend = 0;
            bool trend_valid = false;
            if (trend_n > 0 && now - trend_t[oldest] >= TREND_MIN_US) {
                trend = (int32_t)(((int64_t)avg - trend_mv[oldest]) * 60000000LL / (now - trend_t[oldest]));
                trend_valid = true;
            }
            trend_t[trend_pos] = now;
            trend_mv[trend_pos] = avg;
            trend_pos = (uint8_t)((trend_pos + 1) % TREND_SLOTS);
            if (trend_n < TREND_SLOTS) {
                trend_n++;
            }

            portENTER_CRITICAL(&s_lock);
            const bool first = !s_state.available;
            s_state.available = true;
            s_state.last_mv = mv;
            s_state.average_mv = avg;
            s_state.min_mv = first || avg < s_state.min_mv ? avg : s_state.min_mv;
            s_state.max_mv = first || avg > s_state.max_mv ? avg : s_state.max_mv;
            s_state.trend_mv_per_min = trend;
            s_state.trend_valid = trend_valid;
            s_state.samples++;
            if (!have_percent) {
                s_state.percent = pct;
                percent_changed = true;
            } else if (pct != s_state.percent) {
                const int diff = (int)pct - (int)s_state.percent;
                if (diff >= 2 || diff <= -2 || ++pending >= PERCENT_SETTLE) {
                    s_state.percent = pct;
                    percent_changed = true;
                }
            }
            if (percent_changed || pct == s_state.percent) {
                pending = 0;
            }
            const sugarat_battery_level_t level = next_level(s_state.level, s_state.percent);
            level_changed = level != s_state.level;
            s_state.level = level;
            portEXIT_CRITICAL(&s_lock);
            have_percent = true;
        }

        if (percent_changed) {
            emit(SUGARAT_BATTERY_EVENT_PERCENT);
        }
        if (level_changed) {
            emit(SUGARAT_BATTERY_EVENT_LEVEL);
        }
        if (usb_changed) {
            emit(SUGARAT_BATTERY_EVENT_USB);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(s_cfg.sample_period_ms));
    }
    portENTER_CRITICAL(&s_lock);
    s_state.running = false;
    portEXIT_CRITICAL(&s_lock);
    xSemaphoreGive(s_done);
    vTaskDelete(NULL);
}

sugarat_battery_config_t sugarat_battery_default_config(void)
{
    return (sugarat_battery_config_t){
        .sample_period_ms = 1000,
        .average_samples = 10,
        .low_percent = 15,
        .critical_percent = 5,
        .hysteresis_percent = 3,
        .curve = NULL,
        .curve_len = 0,
        .monitor_usb = true,
        .on_event = NULL,
        .user_data = NULL,
    };
}

esp_err_t sugarat_battery_start(const sugarat_battery_config_t *config)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }
    const sugarat_battery_config_t cfg = config ? *config : sugarat_battery_default_config();
    if (cfg.sample_period_ms < 200 || cfg.sample_period_ms > 60000 || cfg.average_samples == 0
        || cfg.average_samples > MAX_AVERAGE || cfg.critical_percent >= cfg.low_percent || cfg.low_percent > 90
        || !curve_valid(cfg.curve, cfg.curve_len)) {
        ESP_LOGE(TAG, "invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_done) {
        s_done = xSemaphoreCreateBinary();
        if (!s_done) {
            return ESP_ERR_NO_MEM;
        }
    }
    s_cfg = cfg;
    portENTER_CRITICAL(&s_lock);
    memset(&s_state, 0, sizeof(s_state));
    s_state.running = true;
    s_state.level = SUGARAT_BATTERY_LEVEL_UNAVAILABLE;
    portEXIT_CRITICAL(&s_lock);
    s_stop = false;
    if (xTaskCreate(battery_task, "sugarat_bat", 3072, NULL, 2, &s_task) != pdPASS) {
        s_task = NULL;
        portENTER_CRITICAL(&s_lock);
        s_state.running = false;
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "started: every %lu ms, average %u, low %u%%, critical %u%%",
             (unsigned long)cfg.sample_period_ms, cfg.average_samples, cfg.low_percent, cfg.critical_percent);
    return ESP_OK;
}

void sugarat_battery_stop(void)
{
    if (!s_task) {
        return;
    }
    s_stop = true;
    xTaskAbortDelay(s_task);
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "battery task did not stop within 1 s");
        return; /* keep s_task set: a second start must not race the old task */
    }
    s_task = NULL;
}

bool sugarat_battery_get_state(sugarat_battery_state_t *out)
{
    if (!out) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    *out = s_state;
    portEXIT_CRITICAL(&s_lock);
    return true;
}

const char *sugarat_battery_level_name(sugarat_battery_level_t level)
{
    switch (level) {
    case SUGARAT_BATTERY_LEVEL_OK: return "OK";
    case SUGARAT_BATTERY_LEVEL_LOW: return "LOW";
    case SUGARAT_BATTERY_LEVEL_CRITICAL: return "CRITICAL";
    case SUGARAT_BATTERY_LEVEL_UNAVAILABLE: return "N/A";
    }
    return "N/A";
}
