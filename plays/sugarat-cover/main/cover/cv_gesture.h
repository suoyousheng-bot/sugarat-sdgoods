/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Button presses and background swipes retain separate ownership until release. */
#pragma once
#include <stdbool.h>
typedef struct { int x,y,max_dx,max_dy,action;bool held,lost; } cv_gesture_t;
/** Capture the original action; negative action denotes an unbound background. */
void cv_gesture_press(cv_gesture_t *gesture,int x,int y,int action);
/** Track maximum displacement so returning to the start cannot revive a cancelled tap. */
void cv_gesture_move(cv_gesture_t *gesture,int x,int y);
/** @returns True for a non-dragging release inside its original button. */
bool cv_gesture_tap(const cv_gesture_t *gesture,int action,bool inside);
/** @returns -1 for next, +1 for previous, or zero for an ineligible gesture. */
int cv_gesture_swipe(const cv_gesture_t *gesture,int x,int y);
/** @returns Whether a predominantly rightward release requests page navigation, including a press begun on a button. */
bool cv_gesture_back(const cv_gesture_t *gesture,int x,int y);
