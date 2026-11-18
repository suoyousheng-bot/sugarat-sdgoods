/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Application gestures retain BSP press ownership and the top-edge system zone. */
#pragma once
#include "pet_view.h"
/** Bind after creating the view's interactive objects; no global input hooks are installed. */
void pet_gestures_create(pet_view_t *view);
/** Cancel the view's member-slide animation before destroying its screen. */
void pet_gestures_dispose(pet_view_t *view);
