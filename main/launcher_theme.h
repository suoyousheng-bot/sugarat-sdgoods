/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Launcher look: warm charcoal screens, rounded block tiles with an icon above a label, and the
 * terracotta brand accent of the desktop app. Fonts and icons come from Sugarat Watch; tappable
 * parts carry sw_set_action() ids and bubble LV_EVENT_CLICKED to the screen, so sw_event_action()
 * reads which part was tapped.
 * Every function runs on the LVGL thread.
 */
#pragma once
#include "lvgl.h"
#include "sugarat_watch.h"

/* Colors follow the desktop Sugarat studio dark theme (apps/desktop/lib/welcome/studio-theme.css). */
#define TH_BG        0x1D1D1B   /* sidebar surface */
#define TH_CARD      0x2B2925   /* layer 1 */
#define TH_KEY       0x38352F   /* layer 2 */
#define TH_DIM       0x514B43   /* hover / scrollbar */
#define TH_MUTED     0xAAA49B   /* tertiary label */
#define TH_TEXT      0xF3F0E8   /* primary label */
#define TH_ACCENT    0xC54B2C   /* primary button fill */
#define TH_ON_ACCENT 0xFFFDF8   /* label on primary */
#define TH_BRAND     0xE17B57   /* brand primary (dark) */
#define TH_BRAND_TEXT 0xF29A72  /* brand text (dark) */
#define TH_HERO      0x443027   /* active navigation item */
#define TH_BLUE      0x2E3A6E
#define TH_VIOLET    0x7F6CF0
#define TH_AMBER     0xC65F22
#define TH_GREEN     0x378A48
#define TH_RED       0xD9453B

typedef enum { TH_PRIMARY, TH_SECONDARY, TH_DANGER } th_button_t;

/** @returns A new unloaded screen with the launcher background. */
lv_obj_t *th_screen(void);

/** Non-scrolling container without a background.
 * @returns The container; it is not clickable and bubbles events. */
lv_obj_t *th_plain(lv_obj_t *parent);

/** Filled rounded rectangle.
 * @param radius Corner radius, or LV_RADIUS_CIRCLE.
 * @returns The box; it is not clickable until th_tap() and bubbles events. */
lv_obj_t *th_box(lv_obj_t *parent, uint32_t color, lv_coord_t radius);

/** Make `obj` tappable with action id `action` and a pressed tint of its background. */
void th_tap(lv_obj_t *obj, int action);

/** @returns A single-line label in `font` and `color`. */
lv_obj_t *th_text(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color);

/** @returns A one-line label cut with "..." at `width`, text centered. */
lv_obj_t *th_line(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color, lv_coord_t width);

/** @returns A centered label that wraps at `width`. */
lv_obj_t *th_wrap(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color, lv_coord_t width);

/** @returns A Sugarat Watch icon in `color`. */
lv_obj_t *th_icon(lv_obj_t *parent, sw_icon_t icon, sw_size_t size, uint32_t color);

/** @returns A rounded square of `size` in `color` holding a white `icon`. */
lv_obj_t *th_badge(lv_obj_t *parent, sw_icon_t icon, uint32_t color, lv_coord_t size);

/** @returns A 220 x 48 pill button with a centered label. */
lv_obj_t *th_button(lv_obj_t *parent, const char *text, th_button_t kind, int action);

/** @returns A rounded square showing `image` at its own size, corners clipped. */
lv_obj_t *th_image_badge(lv_obj_t *parent, const lv_img_dsc_t *image);

/** Block tile: a badge on top, a title and an optional second line below, all centered.
 * The badge is `image` when it is not NULL, otherwise `icon` on `icon_color`.
 * A tile 130 px or taller is an app tile: an 84 px fallback badge and a body-size title.
 * @returns The tile of `width` x `height`, tappable with `action`. */
lv_obj_t *th_tile(lv_obj_t *parent, sw_icon_t icon, uint32_t icon_color, const lv_img_dsc_t *image, const char *title,
                  const char *subtitle, lv_coord_t width, lv_coord_t height, int action);

/** Top band with a round back button (SW_ACTION_BACK), a centered title and, when `help_action` is
 * not SW_ACTION_NONE, a round "?" button. Create it after scrolling content so it stays on top. */
void th_header(lv_obj_t *screen, const char *title, int help_action);

/** @returns A full-screen vertically scrolling area whose content starts below th_header() and
 * wraps children into centered rows (tiles sit side by side, wide children take a row). */
lv_obj_t *th_grid(lv_obj_t *screen);

/** @returns A full-width row inside th_grid() that lays children out side by side, wrapping. */
lv_obj_t *th_grid_row(lv_obj_t *grid);

/** Recolor the indicator arc of an sw_ring() or sw_spinner(). */
void th_arc_colors(lv_obj_t *arc, uint32_t indicator, uint32_t track);
