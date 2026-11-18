/* Modified by Sugarat: see SUGARAT_TEMPLATE.md for source changes. */
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

/*
 * sdgoods_i18n.c —— 界面语言实现（详见 include/sdgoods_i18n.h）
 *
 * 状态就两个：当前语言 + 版本号。持久化用 NVS 的一个 u8 键。
 * 单线程访问（都在 LVGL/主循环线程），不加锁。
 */

#include "sdgoods_i18n.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "i18n";

#define I18N_NVS_NS   "sdgoods"   /* NVS 命名空间 */
#define I18N_NVS_KEY  "lang"      /* 0 = EN（默认），1 = ZH */

static sdg_lang_t s_lang = SDG_LANG_EN;   /* 出厂默认：英文 */
static uint32_t   s_seq  = 1;             /* 语言版本号，切换时 +1 */

void sdg_i18n_init(void)
{
    /* NVS 可能已被别的模块初始化过；重复调用是安全的。
       ⚠️ 这一手原来也由 sdgoods_wifi_init() 顺带做过，Wi-Fi 已随「最小体积」
       裁剪删除 —— 所以这里保留 NVS 初始化，它是本工程开机后最早的 NVS 初始化点。 */
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS 不可用（%s）：语言固定英文，不受影响", esp_err_to_name(err));
    }

    /* ［派生工程「最小体积」裁剪］本工程已移除中文字库 ⇒ 界面语言**固定英文**。
       刻意不读 NVS 的 lang 键 —— 否则上一台设备残留的 lang=1 会让界面出现方框。 */
    s_lang = SDG_LANG_EN;

    ESP_LOGI(TAG, "界面语言固定 = %s（本工程无中文字库）", sdg_i18n_code());
}

sdg_lang_t sdg_i18n_get(void)
{
    return s_lang;
}

bool sdg_i18n_is_zh(void)
{
    return (s_lang == SDG_LANG_ZH);
}

uint32_t sdg_i18n_seq(void)
{
    return s_seq;
}

const char *sdg_i18n_code(void)
{
    return (s_lang == SDG_LANG_ZH) ? "zh" : "en";
}

const char *sdg_i18n_pick(const char *zh, const char *en)
{
    return (s_lang == SDG_LANG_ZH) ? zh : en;
}

void sdg_i18n_set(sdg_lang_t lang)
{
    /* ［派生工程「最小体积」裁剪］本工程无中文字库，语言固定英文：
       任何切换请求都被忽略（不写 NVS，也不会把界面切成中文方框）。 */
    (void)lang;
}

void sdg_i18n_toggle(void)
{
    /* 同上：无中文字库可切。 */
}
