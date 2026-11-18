#pragma once
#include "lvgl.h"
void sdgoods_app_shell_bind(lv_obj_t *);
void sdgoods_app_shell_set_pause_cb(void (*)(void));
void sdgoods_app_shell_set_resume_cb(void (*)(void));
bool sdgoods_app_shell_menu_is_open(void);
bool sdgoods_power_is_suspended(void);
void sdgoods_lvgl_fast_loop(bool);
void sdgoods_touch_flash_window(uint32_t);
void sdgoods_touch_flash_window_end(void);
