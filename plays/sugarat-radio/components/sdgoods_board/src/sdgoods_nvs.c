/* Modified for Sugarat Radio by Deotaland, 2026: preserve existing NVS. */
/*
 * 谷仓共创计划 · 谷仓 SDGOODS 开放平台基础工程
 * 平台层（板级支持包 BSP）
 * https://github.com/SDGOODS/SDGOODS-ESP32S3
 *
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * 「谷仓共创计划」与「谷仓 SDGOODS 开放平台」项目、谷仓次元屏（谷仓电子徽章）设备，
 *   以及本基础代码的著作权与相关权利，均归深圳希德创新网络有限公司所有。
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件属于平台层，以 Apache-2.0 发布：可自由商用、可闭源分发，
 * 只需保留本声明并携带 NOTICE 文件。详见 LICENSING.md。
 */

/* NVS 保障实现。契约与理由见 sdgoods_nvs.h。 */

#include "sdgoods_nvs.h"

#include "esp_log.h"
#include "nvs_flash.h"

static bool s_inited = false;

void sdgoods_nvs_ensure(void)
{
    if (s_inited) {
        return;                     /* 本进程内已成功过一次，后续直接短路 */
    }

    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(err);
    s_inited = true;
}
