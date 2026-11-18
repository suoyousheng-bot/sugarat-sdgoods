/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Persistent icon tray and its gesture-safe dismissal. */
#pragma once
#include "pet_view.h"
/** Build a hidden tray and reparent the care buttons into six fixed icon positions. */
void pet_controls_create(pet_view_t *view);
/** Hide the tray synchronously before an action changes the current page. */
void pet_controls_dismiss(pet_view_t *view);
/** Place the tray at a clamped drag position without recomputing the background. */
void pet_controls_drag(pet_view_t *view, int y);
/** Cancel view-owned animation callbacks before deleting its screen. */
void pet_controls_dispose(pet_view_t *view);
