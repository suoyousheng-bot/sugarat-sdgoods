/* SPDX-License-Identifier: Apache-2.0 */
/** LVGL presentation shared by the device and offscreen preview. */
#pragma once
#include "lvgl.h"
#include "fluid/link.h"
typedef struct {
    lv_obj_t *screen, *image, *title, *hint, *menu;
    lv_img_dsc_t descriptor;
} bottle_view_t;
/** Create a view using caller-owned RGB565 pixels. Both view and pixels must outlive its screen.
 * @param view Zero-initialized view storage.
 * @param pixels Persistent 360x360 RGB565 frame.
 */
void bottle_view_create(bottle_view_t *view, uint16_t *pixels);

/** Menu actions dispatched to the worker; no LVGL object is shared with that worker. */
typedef enum { BOTTLE_ACTION_BACK=1, BOTTLE_ACTION_NEARBY, BOTTLE_ACTION_HOST,
    BOTTLE_ACTION_SCAN, BOTTLE_ACTION_JOIN, BOTTLE_ACTION_ACCEPT, BOTTLE_ACTION_SOLO,
    BOTTLE_ACTION_COLOR, BOTTLE_ACTION_FILL, BOTTLE_ACTION_RESET } bottle_action_t;
/** Rebuild the safe-area menu on the LVGL thread. Text is copied by LVGL.
 * @param view View created by bottle_view_create().
 * @param link Last worker-published session snapshot.
 * @param nearby Show room creation/discovery instead of practice controls.
 * @param radio_failed Show the transport error and Solo recovery action.
 */
void bottle_view_menu(bottle_view_t *view, const bottle_link_t *link, bool nearby, bool radio_failed);
/** Remove the menu and reveal the bottle. */
void bottle_view_hide_menu(bottle_view_t *view);
