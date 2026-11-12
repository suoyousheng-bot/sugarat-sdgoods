/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Sugarat Launcher: the SDGOODS_LAUNCHER factory application. It lists the apps installed in the
 * launcher slot pool, downloads community apps by ID into app-sized slots and boots them.
 * The project name must stay SDGOODS_LAUNCHER: installed apps recognise the launcher host by it
 * and return here through their control-center Exit button.
 */
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "sdgoods_board.h"
#include "sdgoods_nvs.h"
#include "sdgoods_cc.h"
#include "sdgoods_console.h"
#include "sdgoods_screenshot.h"
#include "sdkconfig.h"
#include "sugarat_watch.h"
#include "launcher_job.h"
#include "launcher_ui.h"
#include "launcher_wifi.h"

static const char *TAG = "launcher";

void app_main(void) {
    ESP_LOGI(TAG, "Sugarat Launcher by Deotaland");
    gpio_config_t power = {
        .pin_bit_mask = 1ULL << BOARD_BAT_CONTROL_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&power));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_BAT_CONTROL_GPIO, BOARD_BAT_CONTROL_LATCH_LEVEL));

    /* Shared NVS holds every app's settings; it is never erased here. */
    sdgoods_nvs_ensure();
    sdgoods_lcd_init_panel();
    sdgoods_lcd_apply_vendor_madctl();
    sdgoods_key_init();
    sdg_i18n_init();
    /* Reserve display/DMA memory before Wi-Fi allocates internal buffers. */
    sdgoods_lvgl_init();
    ESP_ERROR_CHECK(sdgoods_touch_init());
    ESP_ERROR_CHECK(sdgoods_hw_info_init());
    ESP_ERROR_CHECK(sdgoods_audio_init());
    esp_err_t wifi = launcher_wifi_start();
    if (wifi != ESP_OK) ESP_LOGE(TAG, "Wi-Fi unavailable (%s); downloads are disabled", esp_err_to_name(wifi));

    sw_init();
    static const sdgoods_nav_t nav = { .home_create_show = launcher_ui_home, .home_show = launcher_ui_home, .apps_show = launcher_ui_home };
    sdgoods_ui_set_nav(&nav);
    sdgoods_set_power_short_handler(launcher_ui_power_short);
    launcher_job_start();
    launcher_ui_start();
    lv_refr_now(NULL);
    sdgoods_lcd_set_backlight(60);
    sdgoods_cc_settings_restore();
#if CONFIG_LAUNCHER_SERIAL_TEST_INPUT
    sdgoods_console_init();
    sdgoods_screenshot_init();
#endif
    sdgoods_lvgl_loop();
}
