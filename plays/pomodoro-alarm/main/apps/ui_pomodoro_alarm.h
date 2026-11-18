/* SPDX-License-Identifier: Apache-2.0 */
/** Pomodoro alarm timer application for the SDGOODS round badge. */
#ifndef UI_POMODORO_ALARM_H
#define UI_POMODORO_ALARM_H

/** Create (once) and show the Pomodoro alarm screen. */
void ui_pomodoro_alarm_start(void);

/** Per-frame hook; the screen drives itself, so this only keeps the loop awake. */
void ui_pomodoro_alarm_poll(void);

#endif
