/*
 * Sugarat IMU component: QMI8658 sampling plus software motion detectors.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sugarat_imu.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "board_pins.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "sugarat_imu";

/* QMI8658 register map (datasheet / SensorLib QMI8658 constants). */
#define QMI_ADDR         0x6B
#define QMI_CHIP_ID      0x05
#define REG_WHO_AM_I     0x00
#define REG_CTRL1        0x02
#define REG_CTRL2        0x03
#define REG_CTRL3        0x04
#define REG_CTRL5        0x06
#define REG_CTRL7        0x08
#define REG_TEMP_L       0x33 /* TEMP_L..TEMP_H, AX_L..AZ_H, GX_L..GZ_H are contiguous */

/* CTRL1: register auto-increment, little-endian output, oscillator enabled. */
#define CTRL1_VALUE      0x40
/* CTRL2: accelerometer +-8 g (aFS=010), ODR code 5. */
#define CTRL2_VALUE      0x25
/* CTRL3: gyroscope +-512 dps (gFS=101), ODR code 5 (224.2 Hz in 6-axis mode). */
#define CTRL3_VALUE      0x55
/* CTRL5: accelerometer and gyroscope low-pass filters on, mode 3 (13.37 % of ODR). */
#define CTRL5_VALUE      0x77
/* CTRL7: accelerometer + gyroscope enabled; bit 6 keeps the value the BSP ships and tested. */
#define CTRL7_VALUE      0x43

#define ACCEL_G_PER_LSB  (8.0f / 32768.0f)
#define GYRO_DPS_PER_LSB (512.0f / 32768.0f)

/* Bus policy shared with the BSP touch driver (sdgoods_input.c): flash writes stall I2C
 * transactions for up to ~80 ms, and a legacy-driver timeout runs a non-yielding bus reset
 * (~50 ms each) that freezes the UI when repeated. So the timeout stays above the measured
 * worst case, and slow or failed reads back off instead of polling at the sample rate. */
#define I2C_TIMEOUT_MS        90
#define SLOW_READ_US          20000
#define BACKOFF_STEP_MS       20
#define BACKOFF_MAX_MS        300
#define ERRORS_BEFORE_FAULT   10
#define CONFIG_CHECK_US       (2 * 1000 * 1000)
#define WARMUP_SAMPLES        20
#define ORIENTATION_HOLD_US   (400 * 1000)
#define PICKUP_WINDOW_US      (1500 * 1000)
#define STEP_MIN_INTERVAL_US  (250 * 1000)
#define STEP_MAX_INTERVAL_US  (2000 * 1000)
#define STEP_MIN_G            0.10f
#define STEP_MAX_G            1.00f
#define STEP_STREAK           4      /* steps buffered before counting: rejects isolated bumps */
#define STEP_SHAKE_GUARD_US   (1500 * 1000)
#define RAD_TO_DEG            57.2957795f
#define MAX_EVENTS_PER_SAMPLE 4

typedef struct {
    bool acc_init;
    sugarat_imu_vec3_t acc_lp;
    float mag_lp;
    float mag_mean;
    sugarat_imu_vec3_t bias;
    bool bias_learned;
    uint32_t bias_samples;
    bool still;
    int64_t calm_since_us;
    bool pickup_armed;
    int64_t still_end_us;
    sugarat_imu_orientation_t orientation;
    sugarat_imu_orientation_t candidate;
    int64_t candidate_since_us;
    bool shake_above;
    uint8_t peaks;
    int64_t first_peak_us;
    int64_t last_shake_us;
    float step_prev;
    float step_prev2;
    int64_t last_step_us;
    uint8_t streak;
    uint32_t warmup;
} detector_t;

static sugarat_imu_config_t s_cfg;
static sugarat_imu_state_t s_state;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_done;
static volatile bool s_stop;
static volatile bool s_reset_counters;

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    const uint8_t buf[2] = { reg, value };
    return i2c_master_write_to_device((i2c_port_t)BOARD_I2C_PORT, QMI_ADDR, buf, sizeof(buf),
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device((i2c_port_t)BOARD_I2C_PORT, QMI_ADDR, &reg, 1, data, len,
                                        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t write_ranges(void)
{
    static const uint8_t regs[][2] = {
        { REG_CTRL1, CTRL1_VALUE }, { REG_CTRL2, CTRL2_VALUE }, { REG_CTRL3, CTRL3_VALUE },
        { REG_CTRL5, CTRL5_VALUE }, { REG_CTRL7, CTRL7_VALUE },
    };
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
        esp_err_t err = reg_write(regs[i][0], regs[i][1]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

/* Another owner (for example sdgoods_hw_info_init) may rewrite the ranges; detect and restore. */
static void verify_ranges(void)
{
    uint8_t ctrl[6] = { 0 }; /* CTRL2..CTRL7 */
    if (reg_read(REG_CTRL2, ctrl, sizeof(ctrl)) != ESP_OK) {
        return;
    }
    if (ctrl[0] == CTRL2_VALUE && ctrl[1] == CTRL3_VALUE && ctrl[3] == CTRL5_VALUE && ctrl[5] == CTRL7_VALUE) {
        return;
    }
    ESP_LOGW(TAG, "sensor ranges changed by another owner (CTRL2=0x%02x CTRL3=0x%02x CTRL7=0x%02x); restoring",
             ctrl[0], ctrl[1], ctrl[5]);
    if (write_ranges() == ESP_OK) {
        portENTER_CRITICAL(&s_lock);
        s_state.config_repairs++;
        portEXIT_CRITICAL(&s_lock);
    }
}

static float vec_len(sugarat_imu_vec3_t v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static sugarat_imu_vec3_t map_axes(const float chip[3])
{
    float out[3];
    for (int i = 0; i < 3; i++) {
        const int8_t m = s_cfg.axis_map[i];
        const float v = chip[abs(m) - 1];
        out[i] = m < 0 ? -v : v;
    }
    return (sugarat_imu_vec3_t){ out[0], out[1], out[2] };
}

static sugarat_imu_orientation_t classify(sugarat_imu_vec3_t acc_lp)
{
    const float g = vec_len(acc_lp);
    if (g < 0.7f || g > 1.3f) {
        return SUGARAT_IMU_ORIENTATION_UNKNOWN; /* accelerating: gravity direction unreliable */
    }
    const float z = acc_lp.z / g;
    if (z > 0.8f) {
        return SUGARAT_IMU_ORIENTATION_FACE_UP;
    }
    if (z < -0.8f) {
        return SUGARAT_IMU_ORIENTATION_FACE_DOWN;
    }
    if (fabsf(z) < 0.35f) {
        return SUGARAT_IMU_ORIENTATION_UPRIGHT;
    }
    return SUGARAT_IMU_ORIENTATION_TILTED;
}

typedef struct {
    sugarat_imu_event_t items[MAX_EVENTS_PER_SAMPLE];
    int n;
} event_batch_t;

static void push_event(event_batch_t *batch, sugarat_imu_event_type_t type, uint32_t count,
                       sugarat_imu_orientation_t orientation, int64_t now)
{
    if (batch->n < MAX_EVENTS_PER_SAMPLE) {
        batch->items[batch->n++] = (sugarat_imu_event_t){ type, count, orientation, now };
    }
}

static void detect_still(detector_t *d, sugarat_imu_vec3_t gyro_raw, float gyro_mag, int64_t now,
                         event_batch_t *ev, uint32_t *pickups)
{
    /* Before a bias is learned the gyro offset is unknown, so rest is judged from acceleration only. */
    const bool accel_calm = fabsf(d->mag_lp - d->mag_mean) < s_cfg.still_accel_g;
    const bool calm = accel_calm && (!d->bias_learned || gyro_mag < s_cfg.still_gyro_dps);
    if (calm) {
        if (d->calm_since_us == 0) {
            d->calm_since_us = now;
        }
        if (!d->still && now - d->calm_since_us >= (int64_t)s_cfg.still_ms * 1000) {
            d->still = true;
            d->pickup_armed = true;
            push_event(ev, SUGARAT_IMU_EVENT_STILL, 0, d->orientation, now);
        }
        if (d->still) {
            if (d->bias_samples == 0) {
                d->bias = gyro_raw;
            } else {
                d->bias.x += 0.02f * (gyro_raw.x - d->bias.x);
                d->bias.y += 0.02f * (gyro_raw.y - d->bias.y);
                d->bias.z += 0.02f * (gyro_raw.z - d->bias.z);
            }
            if (++d->bias_samples >= 100) {
                d->bias_learned = true;
            }
        }
        return;
    }
    d->calm_since_us = 0;
    if (d->still) {
        d->still = false;
        d->still_end_us = now;
    }
    if (d->pickup_armed) {
        if (gyro_mag > s_cfg.pickup_gyro_dps || fabsf(d->mag_lp - d->mag_mean) > 0.15f) {
            d->pickup_armed = false;
            (*pickups)++;
            push_event(ev, SUGARAT_IMU_EVENT_PICKUP, *pickups, d->orientation, now);
        } else if (now - d->still_end_us > PICKUP_WINDOW_US) {
            d->pickup_armed = false; /* slow drift, not a pickup */
        }
    }
}

static void detect_orientation(detector_t *d, int64_t now, event_batch_t *ev, uint32_t *flips)
{
    const sugarat_imu_orientation_t now_class = classify(d->acc_lp);
    if (now_class == SUGARAT_IMU_ORIENTATION_UNKNOWN) {
        d->candidate_since_us = 0;
        return;
    }
    if (now_class != d->candidate) {
        d->candidate = now_class;
        d->candidate_since_us = now;
        return;
    }
    if (now_class == d->orientation || now - d->candidate_since_us < ORIENTATION_HOLD_US) {
        return;
    }
    const sugarat_imu_orientation_t old = d->orientation;
    d->orientation = now_class;
    push_event(ev, SUGARAT_IMU_EVENT_ORIENTATION, 0, now_class, now);
    if (old == SUGARAT_IMU_ORIENTATION_UNKNOWN) {
        return; /* first classification after start is not a flip */
    }
    if (now_class == SUGARAT_IMU_ORIENTATION_FACE_DOWN) {
        (*flips)++;
        push_event(ev, SUGARAT_IMU_EVENT_FLIP_DOWN, *flips, now_class, now);
    } else if (old == SUGARAT_IMU_ORIENTATION_FACE_DOWN && now_class == SUGARAT_IMU_ORIENTATION_FACE_UP) {
        push_event(ev, SUGARAT_IMU_EVENT_FLIP_UP, *flips, now_class, now);
    }
}

static void detect_shake(detector_t *d, float mag, int64_t now, event_batch_t *ev, uint32_t *shakes)
{
    const float dev = fabsf(mag - d->mag_mean);
    if (d->shake_above) {
        if (dev < s_cfg.shake_threshold_g * 0.5f) {
            d->shake_above = false;
        }
        return;
    }
    if (dev <= s_cfg.shake_threshold_g) {
        return;
    }
    d->shake_above = true;
    if (d->peaks == 0 || now - d->first_peak_us > (int64_t)s_cfg.shake_window_ms * 1000) {
        d->peaks = 1;
        d->first_peak_us = now;
    } else {
        d->peaks++;
    }
    if (d->peaks >= s_cfg.shake_peaks && now - d->last_shake_us >= (int64_t)s_cfg.shake_cooldown_ms * 1000) {
        d->peaks = 0;
        d->last_shake_us = now;
        (*shakes)++;
        push_event(ev, SUGARAT_IMU_EVENT_SHAKE, *shakes, d->orientation, now);
    }
}

static void detect_step(detector_t *d, int64_t now, event_batch_t *ev, uint32_t *steps)
{
    const float cur = d->mag_lp;
    const float peak = d->step_prev;
    const bool is_peak = peak > d->step_prev2 && peak >= cur;
    d->step_prev2 = d->step_prev;
    d->step_prev = cur;
    if (!is_peak) {
        return;
    }
    const float height = peak - d->mag_mean;
    if (height < STEP_MIN_G || height > STEP_MAX_G) {
        return;
    }
    if (d->last_shake_us != 0 && now - d->last_shake_us < STEP_SHAKE_GUARD_US) {
        d->streak = 0;
        return;
    }
    const int64_t interval = now - d->last_step_us;
    if (interval < STEP_MIN_INTERVAL_US) {
        return; /* same footfall, second bump */
    }
    d->last_step_us = now;
    if (interval > STEP_MAX_INTERVAL_US) {
        d->streak = 1;
        return;
    }
    d->streak++;
    if (d->streak < STEP_STREAK) {
        return;
    }
    *steps += (d->streak == STEP_STREAK) ? STEP_STREAK : 1;
    push_event(ev, SUGARAT_IMU_EVENT_STEP, *steps, d->orientation, now);
}

static void process_sample(detector_t *d, const uint8_t b[14], int64_t now)
{
    const float temp = (float)(int8_t)b[1] + (float)b[0] / 256.0f;
    float chip_acc[3];
    float chip_gyr[3];
    for (int i = 0; i < 3; i++) {
        const int16_t a = (int16_t)((uint16_t)b[3 + i * 2] << 8 | b[2 + i * 2]);
        const int16_t g = (int16_t)((uint16_t)b[9 + i * 2] << 8 | b[8 + i * 2]);
        chip_acc[i] = a * ACCEL_G_PER_LSB;
        chip_gyr[i] = g * GYRO_DPS_PER_LSB;
    }
    const sugarat_imu_vec3_t acc = map_axes(chip_acc);
    const sugarat_imu_vec3_t gyro_raw = map_axes(chip_gyr);
    const float mag = vec_len(acc);

    if (!d->acc_init) {
        d->acc_init = true;
        d->acc_lp = acc;
        d->mag_lp = mag;
        d->mag_mean = mag;
        d->step_prev = d->step_prev2 = mag;
    }
    d->acc_lp.x += 0.25f * (acc.x - d->acc_lp.x);
    d->acc_lp.y += 0.25f * (acc.y - d->acc_lp.y);
    d->acc_lp.z += 0.25f * (acc.z - d->acc_lp.z);
    d->mag_lp += 0.35f * (mag - d->mag_lp);
    d->mag_mean += 0.02f * (d->mag_lp - d->mag_mean);

    const sugarat_imu_vec3_t gyro = { gyro_raw.x - d->bias.x, gyro_raw.y - d->bias.y, gyro_raw.z - d->bias.z };
    const float gyro_mag = vec_len(gyro);

    if (s_reset_counters) {
        s_reset_counters = false;
        portENTER_CRITICAL(&s_lock);
        s_state.shake_count = s_state.flip_count = s_state.pickup_count = s_state.step_count = 0;
        portEXIT_CRITICAL(&s_lock);
    }
    portENTER_CRITICAL(&s_lock);
    uint32_t shakes = s_state.shake_count, flips = s_state.flip_count;
    uint32_t pickups = s_state.pickup_count, steps = s_state.step_count;
    portEXIT_CRITICAL(&s_lock);

    event_batch_t ev = { .n = 0 };
    if (d->warmup < WARMUP_SAMPLES) {
        d->warmup++;
    } else {
        detect_still(d, gyro_raw, gyro_mag, now, &ev, &pickups);
        detect_orientation(d, now, &ev, &flips);
        detect_shake(d, mag, now, &ev, &shakes);
        if (s_cfg.step_detection) {
            detect_step(d, now, &ev, &steps);
        }
    }

    const float g = vec_len(d->acc_lp);
    float pitch = 0.0f;
    float roll = 0.0f;
    if (g > 0.1f) {
        roll = atan2f(d->acc_lp.y, d->acc_lp.z) * RAD_TO_DEG;
        pitch = atan2f(-d->acc_lp.x, sqrtf(d->acc_lp.y * d->acc_lp.y + d->acc_lp.z * d->acc_lp.z)) * RAD_TO_DEG;
    }

    portENTER_CRITICAL(&s_lock);
    s_state.sensor_ok = true;
    s_state.accel_g = d->acc_lp;
    s_state.gyro_dps = gyro;
    s_state.pitch_deg = pitch;
    s_state.roll_deg = roll;
    s_state.temperature_c = temp;
    s_state.orientation = d->orientation;
    s_state.still = d->still;
    s_state.gyro_bias_learned = d->bias_learned;
    s_state.gyro_bias_dps = d->bias;
    s_state.shake_count = shakes;
    s_state.flip_count = flips;
    s_state.pickup_count = pickups;
    s_state.step_count = steps;
    s_state.samples++;
    portEXIT_CRITICAL(&s_lock);

    if (s_cfg.on_event) {
        for (int i = 0; i < ev.n; i++) {
            s_cfg.on_event(&ev.items[i], s_cfg.user_data);
        }
    }
}

static void imu_task(void *arg)
{
    (void)arg;
    detector_t d = { 0 };
    TickType_t period = pdMS_TO_TICKS(1000 / s_cfg.sample_hz);
    if (period == 0) {
        period = 1;
    }
    TickType_t last_wake = xTaskGetTickCount();
    int64_t rate_start_us = esp_timer_get_time();
    uint32_t rate_samples = 0;
    int64_t next_check_us = rate_start_us + CONFIG_CHECK_US;
    uint32_t consecutive_errors = 0;
    uint32_t slow_run = 0;
    while (!s_stop) {
        if (xTaskDelayUntil(&last_wake, period) == pdFALSE) {
            last_wake = xTaskGetTickCount(); /* fell behind: resume the cadence, do not burst-read */
        }
        uint8_t b[14];
        const int64_t t0 = esp_timer_get_time();
        const esp_err_t err = reg_read(REG_TEMP_L, b, sizeof(b));
        const int64_t now = esp_timer_get_time();
        if (err != ESP_OK || now - t0 >= SLOW_READ_US) {
            slow_run = slow_run < 1000 ? slow_run + 1 : slow_run;
            const uint32_t backoff = slow_run * BACKOFF_STEP_MS > BACKOFF_MAX_MS ? BACKOFF_MAX_MS
                                                                                 : slow_run * BACKOFF_STEP_MS;
            if (slow_run == 1 || slow_run % 20 == 0) {
                ESP_LOGW(TAG, "slow I2C read (%lld us, %s): backing off %lu ms", (long long)(now - t0),
                         esp_err_to_name(err), (unsigned long)backoff);
            }
            vTaskDelay(pdMS_TO_TICKS(backoff));
            last_wake = xTaskGetTickCount();
        } else {
            slow_run = 0;
        }
        if (err != ESP_OK) {
            consecutive_errors++;
            portENTER_CRITICAL(&s_lock);
            s_state.read_errors++;
            if (consecutive_errors >= ERRORS_BEFORE_FAULT) {
                s_state.sensor_ok = false;
            }
            portEXIT_CRITICAL(&s_lock);
            if (consecutive_errors == ERRORS_BEFORE_FAULT) {
                ESP_LOGE(TAG, "QMI8658 reads failing (%s); readings are stale", esp_err_to_name(err));
            }
            continue;
        }
        if (consecutive_errors >= ERRORS_BEFORE_FAULT) {
            ESP_LOGI(TAG, "QMI8658 reads recovered");
        }
        consecutive_errors = 0;
        process_sample(&d, b, now);
        rate_samples++;
        if (now - rate_start_us >= 1000000) {
            const float hz = (float)rate_samples * 1e6f / (float)(now - rate_start_us);
            portENTER_CRITICAL(&s_lock);
            s_state.sample_hz = hz;
            portEXIT_CRITICAL(&s_lock);
            rate_start_us = now;
            rate_samples = 0;
        }
        if (now >= next_check_us) {
            next_check_us = now + CONFIG_CHECK_US;
            verify_ranges();
        }
    }
    portENTER_CRITICAL(&s_lock);
    s_state.running = false;
    portEXIT_CRITICAL(&s_lock);
    xSemaphoreGive(s_done);
    vTaskDelete(NULL);
}

sugarat_imu_config_t sugarat_imu_default_config(void)
{
    return (sugarat_imu_config_t){
        .sample_hz = 100,
        /* SDGOODS round badge: chip +Y points to the screen's right edge and chip +X to its
         * bottom edge (right edge lowered moved the gravity reading along chip -Y on hardware). */
        .axis_map = { 2, -1, 3 },
        .shake_threshold_g = 0.9f,
        .shake_peaks = 4,
        .shake_window_ms = 1000,
        .shake_cooldown_ms = 1200,
        .still_ms = 1500,
        .still_gyro_dps = 4.0f,
        .still_accel_g = 0.04f,
        .pickup_gyro_dps = 25.0f,
        .step_detection = true,
        .on_event = NULL,
        .user_data = NULL,
    };
}

static bool config_valid(const sugarat_imu_config_t *c)
{
    if (c->sample_hz < 10 || c->sample_hz > 200 || c->shake_peaks == 0 || c->shake_threshold_g <= 0.0f
        || c->still_gyro_dps <= 0.0f || c->still_accel_g <= 0.0f || c->pickup_gyro_dps <= 0.0f) {
        return false;
    }
    bool seen[3] = { false, false, false };
    for (int i = 0; i < 3; i++) {
        const int a = abs(c->axis_map[i]);
        if (a < 1 || a > 3 || seen[a - 1]) {
            return false;
        }
        seen[a - 1] = true;
    }
    return true;
}

esp_err_t sugarat_imu_start(const sugarat_imu_config_t *config)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }
    const sugarat_imu_config_t cfg = config ? *config : sugarat_imu_default_config();
    if (!config_valid(&cfg)) {
        ESP_LOGE(TAG, "invalid configuration (sample_hz=%u, axis_map=%d,%d,%d)", cfg.sample_hz,
                 cfg.axis_map[0], cfg.axis_map[1], cfg.axis_map[2]);
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t id = 0;
    esp_err_t err = reg_read(REG_WHO_AM_I, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 probe failed on I2C port %d addr 0x%02x: %s (touch driver must install the bus first)",
                 BOARD_I2C_PORT, QMI_ADDR, esp_err_to_name(err));
        return err;
    }
    if (id != QMI_CHIP_ID) {
        ESP_LOGE(TAG, "unexpected WHO_AM_I 0x%02x (QMI8658 reports 0x%02x)", id, QMI_CHIP_ID);
        return ESP_ERR_NOT_FOUND;
    }
    err = write_ranges();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 configuration failed: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(30)); /* sensor start-up after enabling */
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
    s_state.chip_id = id;
    s_state.sample_hz = cfg.sample_hz;
    portEXIT_CRITICAL(&s_lock);
    s_stop = false;
    s_reset_counters = false;
    if (xTaskCreate(imu_task, "sugarat_imu", 4096, NULL, 4, &s_task) != pdPASS) {
        s_task = NULL;
        portENTER_CRITICAL(&s_lock);
        s_state.running = false;
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "started: QMI8658 id=0x%02x, +-8 g, +-512 dps, %u Hz requested", id, cfg.sample_hz);
    return ESP_OK;
}

void sugarat_imu_stop(void)
{
    if (!s_task) {
        return;
    }
    s_stop = true;
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGE(TAG, "sampling task did not stop within 500 ms");
        return; /* keep s_task set: a second start must not race the old task */
    }
    s_task = NULL;
}

bool sugarat_imu_get_state(sugarat_imu_state_t *out)
{
    if (!out) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    *out = s_state;
    portEXIT_CRITICAL(&s_lock);
    return true;
}

void sugarat_imu_reset_counters(void)
{
    s_reset_counters = true;
}

const char *sugarat_imu_event_name(sugarat_imu_event_type_t type)
{
    switch (type) {
    case SUGARAT_IMU_EVENT_SHAKE: return "shake";
    case SUGARAT_IMU_EVENT_FLIP_DOWN: return "flip_down";
    case SUGARAT_IMU_EVENT_FLIP_UP: return "flip_up";
    case SUGARAT_IMU_EVENT_PICKUP: return "pickup";
    case SUGARAT_IMU_EVENT_STILL: return "still";
    case SUGARAT_IMU_EVENT_STEP: return "step";
    case SUGARAT_IMU_EVENT_ORIENTATION: return "orientation";
    }
    return "unknown";
}

const char *sugarat_imu_orientation_name(sugarat_imu_orientation_t orientation)
{
    switch (orientation) {
    case SUGARAT_IMU_ORIENTATION_FACE_UP: return "Face up";
    case SUGARAT_IMU_ORIENTATION_FACE_DOWN: return "Face down";
    case SUGARAT_IMU_ORIENTATION_UPRIGHT: return "Upright";
    case SUGARAT_IMU_ORIENTATION_TILTED: return "Tilted";
    case SUGARAT_IMU_ORIENTATION_UNKNOWN: return "Unknown";
    }
    return "Unknown";
}
