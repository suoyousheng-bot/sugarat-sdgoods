/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Pocket Bird controller on the BSP LVGL thread; resources live until screen deletion or reboot. */
#pragma once
/** Create and bind the game screen, or show the existing screen without restarting its run. */
void ui_helloapp_start(void);
/** Advance a created screen from monotonic time; idle and paused screens do not redraw continuously. */
void ui_helloapp_poll(void);
