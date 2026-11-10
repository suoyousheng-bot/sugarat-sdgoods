/*
 * Sugarat IMU component for the SDGOODS ESP32-S3 round badge (QMI8658, shared I2C bus).
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ownership: one sampling task owns QMI8658 configuration while running. The I2C
 * port itself stays owned by the BSP touch driver (sdgoods_touch_init installs it);
 * this component only issues transactions on that port.
 *
 * Start order: sdgoods_touch_init() -> sdgoods_hw_info_init() -> sugarat_imu_start().
 * sdgoods_hw_info_init() writes its own QMI8658 ranges; this component replaces them,
 * so sdgoods_hw_gyro() scaling is invalid while the component runs.
 *
 * Bus sharing: reads use a 90 ms timeout and back off (20 ms steps, up to 300 ms) after a
 * slow or failed read, matching the touch driver, so flash writes never trigger repeated
 * I2C bus resets. Samples are skipped during back-off.
 *
 * Axes: values are reported in the device frame produced by axis_map: +X toward the
 * screen's right edge, +Y toward its top edge, +Z out of the screen. The default map
 * {2, -1, 3} matches the SDGOODS round badge mounting; other boards supply their own.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Three-axis value in the device frame. */
typedef struct {
    float x;
    float y;
    float z;
} sugarat_imu_vec3_t;

/** Coarse resting direction of the screen, derived from gravity on the device Z axis. */
typedef enum {
    SUGARAT_IMU_ORIENTATION_UNKNOWN = 0,
    SUGARAT_IMU_ORIENTATION_FACE_UP,   /* screen faces the sky */
    SUGARAT_IMU_ORIENTATION_FACE_DOWN, /* screen faces the ground */
    SUGARAT_IMU_ORIENTATION_UPRIGHT,   /* screen is vertical, e.g. worn on clothing */
    SUGARAT_IMU_ORIENTATION_TILTED,    /* between the other states */
} sugarat_imu_orientation_t;

/** Motion events published on the IMU task. */
typedef enum {
    SUGARAT_IMU_EVENT_SHAKE = 0,       /* several strong acceleration peaks in a short window */
    SUGARAT_IMU_EVENT_FLIP_DOWN,       /* orientation became FACE_DOWN */
    SUGARAT_IMU_EVENT_FLIP_UP,         /* orientation left FACE_DOWN for FACE_UP */
    SUGARAT_IMU_EVENT_PICKUP,          /* motion started after a rest period */
    SUGARAT_IMU_EVENT_STILL,           /* device came to rest */
    SUGARAT_IMU_EVENT_STEP,            /* one or more counted steps */
    SUGARAT_IMU_EVENT_ORIENTATION,     /* stable orientation changed */
} sugarat_imu_event_type_t;

/** One published event; count is the running total for that event kind. */
typedef struct {
    sugarat_imu_event_type_t type;
    uint32_t count;
    sugarat_imu_orientation_t orientation;
    int64_t time_us;
} sugarat_imu_event_t;

/**
 * Event callback. Runs on the IMU sampling task: never call LVGL here; copy the
 * event or post it to the LVGL thread (sdgoods_lvgl_post) instead. Keep it short.
 */
typedef void (*sugarat_imu_event_cb_t)(const sugarat_imu_event_t *event, void *user_data);

/** Runtime configuration; obtain defaults from sugarat_imu_default_config(). */
typedef struct {
    /** Sampling rate in Hz (10..200). The effective rate is limited by the FreeRTOS tick. */
    uint16_t sample_hz;
    /**
     * Device axis i takes chip axis |axis_map[i]| (1=X, 2=Y, 3=Z) with the sign of
     * axis_map[i]. {1, 2, 3} reports the chip frame; {2, -1, 3} rotates by 90 degrees.
     */
    int8_t axis_map[3];
    /** Acceleration deviation from 1 g that counts as one shake peak. */
    float shake_threshold_g;
    /** Peaks required inside shake_window_ms to publish SHAKE. */
    uint8_t shake_peaks;
    uint16_t shake_window_ms;
    /** Minimum time between two SHAKE events. */
    uint16_t shake_cooldown_ms;
    /** Rest duration that publishes STILL and arms PICKUP. */
    uint16_t still_ms;
    /** Rest limits: gyro magnitude and acceleration deviation from 1 g. */
    float still_gyro_dps;
    float still_accel_g;
    /** Gyro magnitude that ends a rest period as PICKUP. */
    float pickup_gyro_dps;
    /** Count steps with the software pedometer. */
    bool step_detection;
    /** Optional event callback and its argument. */
    sugarat_imu_event_cb_t on_event;
    void *user_data;
} sugarat_imu_config_t;

/** Snapshot copied out of the sampling task. */
typedef struct {
    bool running;
    bool sensor_ok;              /* last reads succeeded */
    uint8_t chip_id;             /* WHO_AM_I, 0x05 for QMI8658 */
    sugarat_imu_vec3_t accel_g;  /* filtered acceleration */
    sugarat_imu_vec3_t gyro_dps; /* bias-corrected angular rate */
    float pitch_deg;             /* rotation about device X, from gravity */
    float roll_deg;              /* rotation about device Y, from gravity */
    float temperature_c;
    sugarat_imu_orientation_t orientation;
    bool still;
    bool gyro_bias_learned;      /* at least one rest period refined the gyro bias */
    sugarat_imu_vec3_t gyro_bias_dps;
    uint32_t shake_count;
    uint32_t flip_count;
    uint32_t pickup_count;
    uint32_t step_count;
    uint32_t samples;
    uint32_t read_errors;
    uint32_t config_repairs;     /* times another owner changed the sensor ranges */
    float sample_hz;             /* samples counted over the last second */
} sugarat_imu_state_t;

/**
 * Default configuration tuned for a hand-held or worn badge.
 * @returns Configuration with the SDGOODS round badge axis map and no callback.
 */
sugarat_imu_config_t sugarat_imu_default_config(void);

/**
 * Probe and configure the QMI8658, then start sampling.
 * @param config Configuration copied by the component; NULL uses defaults.
 * @returns ESP_OK; ESP_ERR_INVALID_STATE if already running; ESP_ERR_NOT_FOUND when
 *          WHO_AM_I does not identify a QMI8658; ESP_ERR_INVALID_ARG for invalid
 *          values; I2C errors from the probe; ESP_ERR_NO_MEM when the task cannot start.
 */
esp_err_t sugarat_imu_start(const sugarat_imu_config_t *config);

/** Stop sampling and wait for the task to exit. Safe to call when stopped. */
void sugarat_imu_stop(void);

/**
 * Copy the latest state.
 * @param out Destination snapshot.
 * @returns false when out is NULL; otherwise true, with out->running indicating activity.
 */
bool sugarat_imu_get_state(sugarat_imu_state_t *out);

/** Reset shake, flip, pickup and step counters to zero. */
void sugarat_imu_reset_counters(void);

/**
 * Short English name for an event kind, suitable for logs.
 * @param type Event kind.
 * @returns Static string.
 */
const char *sugarat_imu_event_name(sugarat_imu_event_type_t type);

/**
 * Short English name for an orientation, suitable for logs and ASCII UI.
 * @param orientation Orientation value.
 * @returns Static string.
 */
const char *sugarat_imu_orientation_name(sugarat_imu_orientation_t orientation);

#ifdef __cplusplus
}
#endif
