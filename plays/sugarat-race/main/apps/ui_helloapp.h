/*
 * 谷仓共创计划 · 谷仓 SDGOODS 开放平台基础工程
 * 应用层示例
 * https://github.com/SDGOODS/SDGOODS-ESP32S3
 *
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件属于应用层，以 Apache-2.0 发布：可自由商用、可闭源分发，
 * 只需保留本声明并携带 NOTICE 文件。详见 LICENSING.md。
 */

#pragma once

/*
 * 本工程唯一应用的入口（派生自 app_template）。
 * 开机直接进入本应用（见 apps_registry.c 的 home_create_show）。
 */

/* 进入 / 显示本应用（注册在 main/apps/apps_registry.c 的 s_apps[] 表里） */
void ui_helloapp_start(void);

/* 每帧推进（由 apps_registry.c 汇总后交给平台主循环；不在前台时立即返回） */
void ui_helloapp_poll(void);
