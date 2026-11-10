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

/* NVS 保障（平台层公共工具）
 *
 * 为什么需要它：**平台层不能假设 app 做过 `nvs_flash_init()`**。最小 app（例如 Hello 样例）
 * 既不碰 WiFi 也不碰 BLE，可能从不初始化 NVS；而平台层有若干模块会写 NVS
 * （控制中心的音量/亮度、跳过开机动画标志、i18n 语言、槽清单……），一旦 NVS 没初始化，
 * 这些写入会静默失败 —— 症状是「设置不保存 / 开机动画又播一遍」，且只在某些 app 里出现，
 * 非常难查。平台层自己保证这件事，app 就不必知道 NVS 的存在。
 *
 * 调用时机（都由平台层自己调，app 无需关心）：
 *   · `sdgoods_app_shell_init()` —— 所有 app 的统一入口，开机调一次即可覆盖整机生命周期
 *   · 控制中心读写设置前 —— 保证「不调 app_shell_init 的固件」（如启动器）也安全
 *   · 跳过开机动画标志读写前（`sdgoods_boot_skip.c`）
 */

#pragma once

/** Initialize NVS once. Initialization failure stops dependent work without erasing user data. */
void sdgoods_nvs_ensure(void);
