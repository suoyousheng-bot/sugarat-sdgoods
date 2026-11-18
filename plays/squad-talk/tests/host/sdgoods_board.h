#pragma once
#include <stdbool.h>
#include "lvgl.h"
void sdgoods_app_shell_bind(lv_obj_t *o);
void sdgoods_app_shell_set_pause_cb(void (*cb)(void));
void sdgoods_app_shell_set_resume_cb(void (*cb)(void));
bool sdgoods_app_shell_menu_is_open(void);
bool sdgoods_power_is_suspended(void);
void sdgoods_lvgl_fast_loop(bool b);
