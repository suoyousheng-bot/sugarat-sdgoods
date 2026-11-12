/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Launcher screens: home (status bar, storage hero, Apps / Download / Settings tiles), the installed
 * app grid, the download keypad, settings, play confirmation, install progress and results.
 * Every function runs on the LVGL thread. Only one screen exists at a time apart from a transition.
 */
#pragma once
#include <stdbool.h>

/** Show the home screen. Called once after LVGL, Wi-Fi and the job worker start. */
void launcher_ui_start(void);

/** Return to the home screen unless a download, install, uninstall or launch is running. */
void launcher_ui_home(void);

/** Power-key short press: step back toward the home screen.
 * @returns False on the home screen so the board turns the screen off. */
bool launcher_ui_power_short(void);
