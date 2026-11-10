/* Modified for Sugarat Radio by Deotaland, 2026. */
/*
 * 谷仓共创计划 · 谷仓 SDGOODS 开放平台基础工程
 * 应用层示例
 * https://github.com/SDGOODS/SDGOODS-ESP32S3
 *
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * 「谷仓共创计划」与「谷仓 SDGOODS 开放平台」项目、谷仓次元屏（谷仓电子徽章）设备，
 *   以及本基础代码的著作权与相关权利，均归深圳希德创新网络有限公司所有。
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件属于应用层，以 Apache-2.0 发布：可自由商用、可闭源分发，
 * 只需保留本声明并携带 NOTICE 文件。详见 LICENSING.md。
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdgoods_board.h"
#include "apps_registry.h"
#include "wifi_prov.h"
#include "ui_radio.h"
#include "build_version.h"

/** Start the standalone radio. Network, LVGL and I2S each have one owner. */
void app_main(void) {
    ESP_LOGI("Sugarat Radio", "Sugarat Radio 0.1.0 by Deotaland, build %s", BUILD_VERSION_STR);
    ESP_ERROR_CHECK(nvs_flash_init());
    gpio_config_t power = {
        .pin_bit_mask = 1ULL << BOARD_BAT_CONTROL_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&power));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_BAT_CONTROL_GPIO, BOARD_BAT_CONTROL_LATCH_LEVEL));
    sdgoods_lcd_init_panel();
    sdgoods_lcd_apply_vendor_madctl();
    sdgoods_key_init();
    sdg_i18n_init();
    /* Reserve display/DMA memory before Wi-Fi and audio allocate internal buffers. */
    sdgoods_lvgl_init();
    ESP_ERROR_CHECK(sdgoods_touch_init());
    ESP_ERROR_CHECK(sdgoods_hw_info_init());
    ESP_ERROR_CHECK(sdgoods_audio_init());
    ESP_ERROR_CHECK(wifi_prov_start());
    apps_register();
    sdgoods_app_shell_init();
    if (wifi_prov_get_state() == WIFI_STATE_CONNECTED) ui_radio_start();
    else wifi_prov_show(NULL);
    lv_refr_now(NULL);
    sdgoods_lcd_set_backlight(60);
    sdgoods_lvgl_loop();
}
