#pragma once
#include <stdbool.h>
#include "lvgl.h"
void sdgoods_app_shell_bind(lv_obj_t *);
void sdgoods_app_shell_set_pause_cb(void (*)(void));
void sdgoods_app_shell_set_resume_cb(void (*)(void));
void sdgoods_set_power_short_handler(bool (*)(void));
