/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Initialize the SDGoods BSP and enter the English pet in standalone or launcher mode. */
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdgoods_board.h"
#include "sdgoods_cc.h"
#include "apps_registry.h"

void app_main(void)
{
    ESP_LOGI("sugarat_pet", "Sugarat Pocket Companion 0.2.1; epet MIT; English");
    gpio_config_t power = {.pin_bit_mask = 1ULL << BOARD_BAT_CONTROL_GPIO, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&power));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_BAT_CONTROL_GPIO, BOARD_BAT_CONTROL_LATCH_LEVEL));
    sdgoods_lcd_init_panel();
    sdgoods_lcd_apply_vendor_madctl();
    sdgoods_key_init();
    sdg_i18n_init();
    sdgoods_lvgl_init();
    ESP_ERROR_CHECK(sdgoods_touch_init());
    ESP_ERROR_CHECK(sdgoods_hw_info_init());
    ESP_ERROR_CHECK(sdgoods_audio_init());
    sdgoods_app_shell_init();
    apps_register();
    sdgoods_ui_home_create_show();
    lv_refr_now(NULL);
    sdgoods_lcd_set_backlight(sdgoods_cc_brightness_get());
    sdgoods_lvgl_loop();
}
