/*
 * Sugarat battery component for the SDGOODS ESP32-S3 round badge.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ownership: the BSP owns ADC1 (sdgoods_hw_info_init creates the one-shot unit);
 * this component samples through sdgoods_hw_bat_v() and never creates an ADC unit.
 * Start after sdgoods_hw_info_init().
 *
 * Percentages come from an open-circuit voltage curve. Voltage sags under load
 * (backlight, speaker) and rises while charging, so the percentage is an estimate.
 * The board exposes no documented charge-status pin; usb_connected reports a USB
 * data host (computer), not a charger.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Coarse charge level with hysteresis. */
typedef enum {
    SUGARAT_BATTERY_LEVEL_UNAVAILABLE = 0, /* no valid ADC reading yet */
    SUGARAT_BATTERY_LEVEL_OK,
    SUGARAT_BATTERY_LEVEL_LOW,
    SUGARAT_BATTERY_LEVEL_CRITICAL,
} sugarat_battery_level_t;

/** One point of a voltage-to-percentage curve. */
typedef struct {
    uint16_t mv;
    uint8_t percent;
} sugarat_battery_point_t;

/** Events published on the battery task. */
typedef enum {
    SUGARAT_BATTERY_EVENT_PERCENT = 0, /* reported percentage changed */
    SUGARAT_BATTERY_EVENT_LEVEL,       /* level changed (OK/LOW/CRITICAL/UNAVAILABLE) */
    SUGARAT_BATTERY_EVENT_USB,         /* USB host connection changed */
} sugarat_battery_event_type_t;

/** Event payload with the values after the change. */
typedef struct {
    sugarat_battery_event_type_t type;
    uint8_t percent;
    sugarat_battery_level_t level;
    bool usb_connected;
    uint16_t mv;
} sugarat_battery_event_t;

/** Event callback; runs on the battery task, so never call LVGL here. */
typedef void (*sugarat_battery_event_cb_t)(const sugarat_battery_event_t *event, void *user_data);

/** Runtime configuration; obtain defaults from sugarat_battery_default_config(). */
typedef struct {
    /** Sampling period in milliseconds (200..60000). */
    uint32_t sample_period_ms;
    /** Moving-average window in samples (1..32). */
    uint8_t average_samples;
    /** Level thresholds in percent; recovery needs the threshold plus hysteresis_percent. */
    uint8_t low_percent;
    uint8_t critical_percent;
    uint8_t hysteresis_percent;
    /**
     * Curve sorted by descending mv, first point 100 %, last point 0 %.
     * NULL selects the built-in single-cell Li-ion rest curve.
     */
    const sugarat_battery_point_t *curve;
    size_t curve_len;
    /** Report USB Serial/JTAG host connection. */
    bool monitor_usb;
    sugarat_battery_event_cb_t on_event;
    void *user_data;
} sugarat_battery_config_t;

/** Snapshot copied out of the battery task. */
typedef struct {
    bool running;
    bool available;            /* at least one valid reading */
    uint16_t last_mv;          /* latest median reading */
    uint16_t average_mv;       /* moving average used for percent */
    uint16_t min_mv;           /* lowest average since start */
    uint16_t max_mv;           /* highest average since start */
    uint8_t percent;
    sugarat_battery_level_t level;
    int32_t trend_mv_per_min;  /* positive while voltage rises (often charging) */
    bool trend_valid;          /* at least 30 s of history */
    bool usb_connected;
    uint32_t samples;
    uint32_t failed_reads;     /* ADC reads that returned no value */
} sugarat_battery_state_t;

/**
 * Default configuration: 1 s samples, 10-sample average, LOW at 15 %, CRITICAL at 5 %.
 * @returns Configuration using the built-in curve and no callback.
 */
sugarat_battery_config_t sugarat_battery_default_config(void);

/**
 * Start periodic sampling.
 * @param config Configuration copied by the component; NULL uses defaults. A custom
 *               curve must remain valid while the component runs.
 * @returns ESP_OK; ESP_ERR_INVALID_STATE if already running; ESP_ERR_INVALID_ARG for
 *          invalid values or curves; ESP_ERR_NO_MEM when the task cannot start.
 */
esp_err_t sugarat_battery_start(const sugarat_battery_config_t *config);

/** Stop sampling and wait for the task to exit. Safe to call when stopped. */
void sugarat_battery_stop(void);

/**
 * Copy the latest state.
 * @param out Destination snapshot.
 * @returns false when out is NULL; otherwise true.
 */
bool sugarat_battery_get_state(sugarat_battery_state_t *out);

/**
 * Convert a voltage with a curve.
 * @param mv Battery voltage in millivolts.
 * @param curve Curve sorted by descending mv, or NULL for the built-in curve.
 * @param curve_len Number of points in curve.
 * @returns Percentage 0..100 by linear interpolation, clamped at both ends.
 */
uint8_t sugarat_battery_percent_from_mv(uint16_t mv, const sugarat_battery_point_t *curve, size_t curve_len);

/**
 * Short English name for a level, suitable for logs and ASCII UI.
 * @param level Level value.
 * @returns Static string.
 */
const char *sugarat_battery_level_name(sugarat_battery_level_t level);

#ifdef __cplusplus
}
#endif
