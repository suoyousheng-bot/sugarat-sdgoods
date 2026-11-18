/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Start the reviewed BSP without unnecessary audio allocations, then enter the photo application. */
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "sdgoods_board.h"
#include "sdgoods_cc.h"
#include "apps_registry.h"
void app_main(void)
{
    ESP_LOGI("sugarat_cover","Sugarat Cover %s; default language English",esp_app_get_description()->version);
    gpio_config_t power={.pin_bit_mask=1ULL<<BOARD_BAT_CONTROL_GPIO,.mode=GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&power));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_BAT_CONTROL_GPIO,BOARD_BAT_CONTROL_LATCH_LEVEL));
    sdgoods_lcd_init_panel();sdgoods_lcd_apply_vendor_madctl();sdgoods_key_init();sdg_i18n_init();
    sdgoods_lvgl_init();ESP_ERROR_CHECK(sdgoods_touch_init());ESP_ERROR_CHECK(sdgoods_hw_info_init());
    sdgoods_app_shell_init();apps_register();sdgoods_ui_home_create_show();lv_refr_now(NULL);
    sdgoods_lcd_set_backlight(sdgoods_cc_brightness_get());sdgoods_lvgl_loop();
}
