#pragma once
#include "nvs.h"
const char *sdgoods_device_mode_str(void);
#include <stddef.h>
esp_err_t sdgoods_appdata_begin(const char *id, char *out, size_t size);
