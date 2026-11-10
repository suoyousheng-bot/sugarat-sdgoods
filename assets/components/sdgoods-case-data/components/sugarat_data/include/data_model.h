/* SPDX-License-Identifier: Apache-2.0 */
/** Bounded telemetry, freshness and hysteresis; independent of LVGL and sensor drivers. */
#ifndef SUGARAT_DATA_MODEL_H
#define SUGARAT_DATA_MODEL_H
#include <stdbool.h>
#include <stdint.h>

#define SD_HISTORY_CAPACITY 48
typedef enum { SD_WAITING, SD_LIVE, SD_STALE, SD_OFFLINE, SD_INVALID } sd_status_t;
typedef struct {
    int16_t minimum_tenths, maximum_tenths, alarm_tenths, hysteresis_tenths;
    uint32_t stale_after_ms;
} sd_limits_t;
typedef struct { int16_t temperature_tenths; uint8_t humidity_percent, battery_percent; } sd_sample_t;
typedef struct { int16_t value; bool valid; uint64_t at_ms; } sd_point_t;
typedef struct {
    sd_limits_t limits;
    sd_sample_t sample;
    sd_status_t status;
    uint64_t received_ms;
    bool has_sample, alarm_active, acknowledged;
    sd_point_t history[SD_HISTORY_CAPACITY];
    uint8_t count, next;
} sd_model_t;

/**
 * Initialize an empty model; invalid limits leave the output untouched.
 * @param model Caller-owned model.
 * @param limits Inclusive temperature range, upper alarm, positive hysteresis and expiry.
 * @returns Whether configuration is valid.
 */
bool sd_model_init(sd_model_t *model, sd_limits_t limits);
/**
 * Copy one sensor reading and append history. Invalid fields hide the current reading,
 * retain the previous sample and alarm, and append a gap. No partial reading is committed.
 * @param model Initialized model.
 * @param sample Complete reading, validated here after decoding by the provider.
 * @param now_ms Nondecreasing monotonic receive time; backward time is rejected without mutation.
 * @returns Whether the reading was accepted.
 */
bool sd_model_push(sd_model_t *model, sd_sample_t sample, uint64_t now_ms);
/**
 * Report provider loss without replacing values or clearing an active alarm.
 * @param model Initialized model; a transition to offline appends one gap.
 * @param now_ms Nondecreasing monotonic time.
 */
void sd_model_disconnect(sd_model_t *model, uint64_t now_ms);
/**
 * Expire live readings at the configured age; append a single history gap.
 * @param model Initialized model.
 * @param now_ms Nondecreasing monotonic time.
 */
void sd_model_tick(sd_model_t *model, uint64_t now_ms);
/**
 * Acknowledge the current alarm without clearing its active condition.
 * @param model Initialized model. A subsequent recovery rearms acknowledgement.
 */
void sd_model_acknowledge(sd_model_t *model);
/**
 * Read history from oldest to newest.
 * @param model Initialized model.
 * @param index Index smaller than count.
 * @returns One copied sample or gap and its monotonic receive time.
 */
sd_point_t sd_model_point(const sd_model_t *model, uint8_t index);
#endif
