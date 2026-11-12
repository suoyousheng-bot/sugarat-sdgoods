/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Sugarat Watch: watchOS-style LVGL 8.3 components for 360 x 360 round displays.
 *
 * Sugarat Game (sugarat_game.h) exposes the same functions, enums and icon names
 * with the sg_ prefix, so a screen written with one library switches style by
 * renaming the prefix. Build screens from these components instead of styling raw
 * LVGL widgets: every component removes LVGL theme styles and applies its own.
 *
 * Usage rules:
 * - Call every function on the LVGL thread after lv_init() and sw_init().
 * - Text arguments are copied unless a comment says otherwise.
 * - Text fonts bundle common Chinese characters plus literals found in sources;
 *   run tools/make_fonts.py (see README) when an app needs others.
 * - Interactive parts report taps as LV_EVENT_CLICKED with LV_OBJ_FLAG_EVENT_BUBBLE,
 *   so one handler on a component (or the screen) receives them; read which part
 *   was tapped with sw_event_action(). Value widgets send LV_EVENT_VALUE_CHANGED.
 * - Returned objects are ordinary lv_obj_t children; position them with
 *   lv_obj_align() or place them in sw_page()/sw_row()/sw_column().
 */
#ifndef SUGARAT_WATCH_H
#define SUGARAT_WATCH_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "sw_icons.h"

#define SW_SCREEN 360

/** Semantic colors; sw_color() maps them to the watchOS palette. */
typedef enum {
    SW_TONE_BG,      /**< Screen background (black). */
    SW_TONE_SURFACE, /**< Cards and list rows. */
    SW_TONE_RAISED,  /**< Buttons and tracks on cards. */
    SW_TONE_TEXT,    /**< Primary text. */
    SW_TONE_MUTED,   /**< Secondary text. */
    SW_TONE_ACCENT,  /**< App tint (green). */
    SW_TONE_RED,
    SW_TONE_ORANGE,
    SW_TONE_YELLOW,
    SW_TONE_GREEN,
    SW_TONE_TEAL,
    SW_TONE_BLUE,
    SW_TONE_PURPLE,
    SW_TONE_PINK,
    SW_TONE_COUNT,
} sw_tone_t;

/** Text roles. NUM_* fonts hold digits and ": % . - + / °"; other characters fall back to text fonts. */
typedef enum {
    SW_FONT_CAPTION,  /**< 14 px secondary text. */
    SW_FONT_BODY,     /**< 16 px body text. */
    SW_FONT_HEADLINE, /**< 20 px bold headings. */
    SW_FONT_TITLE,    /**< 28 px bold titles. */
    SW_FONT_NUM_S,    /**< 24 px digits. */
    SW_FONT_NUM_M,    /**< 40 px digits. */
    SW_FONT_NUM_L,    /**< 80 px digits for clocks. */
    SW_FONT_COUNT,
} sw_font_t;

/** Icon sizes: 16, 20, 28 and 40 px. */
typedef enum { SW_SIZE_S, SW_SIZE_M, SW_SIZE_L, SW_SIZE_XL } sw_size_t;

/** Button looks: FILLED uses the tone as background, TINTED a translucent tone, PLAIN a neutral fill. */
typedef enum { SW_BUTTON_FILLED, SW_BUTTON_TINTED, SW_BUTTON_PLAIN } sw_button_variant_t;

/** Voice assistant states shown by sw_voice_button(). */
typedef enum { SW_VOICE_IDLE, SW_VOICE_LISTENING, SW_VOICE_THINKING, SW_VOICE_SPEAKING, SW_VOICE_OFFLINE } sw_voice_t;

/** Built-in action ids reported by sw_event_action(); app-assigned ids use 0..999. */
typedef enum {
    SW_ACTION_NONE = -1,
    SW_ACTION_CONFIRM = 1000,
    SW_ACTION_CANCEL,
    SW_ACTION_BACK,
    SW_ACTION_PREV,
    SW_ACTION_PLAY,
    SW_ACTION_NEXT,
    SW_ACTION_VOICE,
} sw_action_t;

/* ---------------------------------------------------------------- Foundation */

/** Prepare the library; safe to call more than once. */
void sw_init(void);

/** @returns The color of a tone. */
lv_color_t sw_color(sw_tone_t tone);

/** @returns The font of a text role. */
const lv_font_t *sw_font(sw_font_t font);

/** @returns A new black 360 x 360 screen; load it with lv_scr_load(). */
lv_obj_t *sw_screen(void);

/** @returns A label in the given role and tone. */
lv_obj_t *sw_label(lv_obj_t *parent, const char *text, sw_font_t font, sw_tone_t tone);

/** @returns An icon glyph; SW_ICON_NONE creates an empty label. */
lv_obj_t *sw_icon(lv_obj_t *parent, sw_icon_t icon, sw_size_t size, sw_tone_t tone);

/** Change the glyph and tone of an sw_icon(). */
void sw_icon_set(lv_obj_t *icon_obj, sw_icon_t icon, sw_tone_t tone);

/** Tag a button, list item or app icon with an action id (0..999 or sw_action_t); uses the object's user data. */
void sw_set_action(lv_obj_t *obj, int action);

/** @returns The action id of the tapped part for a bubbled LV_EVENT_CLICKED, or SW_ACTION_NONE. */
int sw_event_action(lv_event_t *e);

/* ---------------------------------------------------------------- Layout */

/** @returns A full-screen vertical scroller; children stack centered and fade near the round edge. Title may be NULL. */
lv_obj_t *sw_page(lv_obj_t *screen, const char *title);

/** @returns A transparent horizontal group that sizes to its content. */
lv_obj_t *sw_row(lv_obj_t *parent);

/** @returns A transparent vertical group that sizes to its content. */
lv_obj_t *sw_column(lv_obj_t *parent);

/** @returns A 280 px wide rounded card that stacks children vertically. */
lv_obj_t *sw_card(lv_obj_t *parent);

/** @returns A small section heading for sw_page(). */
lv_obj_t *sw_section(lv_obj_t *parent, const char *title);

/** @returns A status bar at the top of the screen with time, Wi-Fi and battery. */
lv_obj_t *sw_status_bar(lv_obj_t *screen, const char *time);
void sw_status_bar_set_time(lv_obj_t *bar, const char *time);
/** Show battery level; below 20 % the icon turns red, while charging it turns green. */
void sw_status_bar_set_battery(lv_obj_t *bar, uint8_t percent, bool charging);
void sw_status_bar_set_connected(lv_obj_t *bar, bool connected);

/** @returns Page indicator dots aligned to the bottom of the parent. */
lv_obj_t *sw_page_dots(lv_obj_t *parent, uint8_t count, uint8_t active);
void sw_page_dots_set(lv_obj_t *dots, uint8_t active);

/** @returns A crown-style arc on the right screen edge that follows the page's scroll position. */
lv_obj_t *sw_scroll_indicator(lv_obj_t *page);

/* ---------------------------------------------------------------- Controls */

/** @returns A capsule button; icon or text may be SW_ICON_NONE / NULL. Width follows content; call lv_obj_set_width() to fix it. */
lv_obj_t *sw_button(lv_obj_t *parent, sw_icon_t icon, const char *text, sw_button_variant_t variant, sw_tone_t tone);

/** @returns A 56 px round icon button. */
lv_obj_t *sw_icon_button(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone);

/** @returns A 64 px round on/off button (control-center style); toggles LV_STATE_CHECKED and sends VALUE_CHANGED. */
lv_obj_t *sw_toggle_button(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, bool on);

/** @returns An on/off switch; read with lv_obj_has_state(obj, LV_STATE_CHECKED). */
lv_obj_t *sw_toggle(lv_obj_t *parent, bool on);

/** @returns A round checkbox with a label. */
lv_obj_t *sw_checkbox(lv_obj_t *parent, const char *text, bool checked);

/** @returns A horizontal slider; read with lv_slider_get_value(). */
lv_obj_t *sw_slider(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, sw_tone_t tone);

/** @returns A "- value +" stepper that sends VALUE_CHANGED. */
lv_obj_t *sw_stepper(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, int32_t step);
int32_t sw_stepper_get(lv_obj_t *stepper);
void sw_stepper_set(lv_obj_t *stepper, int32_t value);

/** @returns A single-choice segmented control; options must have static lifetime. Sends VALUE_CHANGED. */
lv_obj_t *sw_segmented(lv_obj_t *parent, const char *const *options, uint8_t count, uint8_t selected);
uint8_t sw_segmented_get(lv_obj_t *segmented);

/** @returns A linear progress bar of 0..100. */
lv_obj_t *sw_progress(lv_obj_t *parent, uint8_t percent, sw_tone_t tone);
void sw_progress_set(lv_obj_t *progress, uint8_t percent);

/** @returns A circular progress ring of 0..100 with an empty center for other content. */
lv_obj_t *sw_ring(lv_obj_t *parent, lv_coord_t size, uint8_t percent, sw_tone_t tone);
void sw_ring_set(lv_obj_t *ring, uint8_t percent);

/** @returns An indeterminate spinner. */
lv_obj_t *sw_spinner(lv_obj_t *parent, lv_coord_t size, sw_tone_t tone);

/** @returns A wheel picker; options are newline-separated. Read with lv_roller_get_selected(). */
lv_obj_t *sw_roller(lv_obj_t *parent, const char *options, uint16_t selected, uint8_t visible_rows);

/* ---------------------------------------------------------------- Lists */

/** @returns A 280 px tappable row with optional icon (SW_ICON_NONE) and subtitle (NULL). */
lv_obj_t *sw_list_item(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *title, const char *subtitle);
/** @returns A right-aligned value label added to a list item. */
lv_obj_t *sw_list_item_value(lv_obj_t *item, const char *text);
/** @returns A switch added to a list item. */
lv_obj_t *sw_list_item_toggle(lv_obj_t *item, bool on);
/** Add a disclosure chevron to a list item. */
void sw_list_item_chevron(lv_obj_t *item);

/* ---------------------------------------------------------------- Feedback */

/** @returns A count pill, or a dot when text is NULL. */
lv_obj_t *sw_badge(lv_obj_t *parent, const char *text, sw_tone_t tone);

/** @returns A top banner that deletes itself after duration_ms (0 keeps it). parent NULL uses lv_layer_top(). */
lv_obj_t *sw_toast(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *text, uint32_t duration_ms);

/** @returns A notification card with app icon, app name, title and body. */
lv_obj_t *sw_notification(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *app, const char *title, const char *body);

/**
 * @returns A full-screen modal with confirm (and optional cancel, NULL hides it) buttons.
 * Taps report SW_ACTION_CONFIRM or SW_ACTION_CANCEL on the returned object, which then deletes itself.
 * parent NULL uses lv_layer_top().
 */
lv_obj_t *sw_alert(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *title, const char *message,
                   const char *confirm, const char *cancel);

/** @returns A centered empty-state block with icon, title and hint (hint may be NULL). */
lv_obj_t *sw_empty_state(lv_obj_t *parent, sw_icon_t icon, const char *title, const char *hint);

/** @returns A spinner with a caption below. */
lv_obj_t *sw_loading(lv_obj_t *parent, const char *text);

/* ---------------------------------------------------------------- Data */

/** @returns A metric: caption label, large value and unit (unit may be NULL). */
lv_obj_t *sw_stat(lv_obj_t *parent, const char *value, const char *unit, const char *label, sw_tone_t tone);
void sw_stat_set(lv_obj_t *stat, const char *value);

/** @returns A circular gauge complication showing value and a label in its center. */
lv_obj_t *sw_gauge(lv_obj_t *parent, lv_coord_t size, int32_t min, int32_t max, int32_t value, const char *label, sw_tone_t tone);
void sw_gauge_set(lv_obj_t *gauge, int32_t value);

/** @returns A compact card with icon, value and label. */
lv_obj_t *sw_tile(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *value, const char *label);
void sw_tile_set(lv_obj_t *tile, const char *value);

/** @returns A bar chart scaled to the largest value. */
lv_obj_t *sw_bar_chart(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sw_tone_t tone);
void sw_bar_chart_set(lv_obj_t *chart, const int16_t *values, uint8_t count);

/** @returns A line chart without axes, scaled to the value range. */
lv_obj_t *sw_sparkline(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sw_tone_t tone);
void sw_sparkline_set(lv_obj_t *chart, const int16_t *values, uint8_t count);

/* ---------------------------------------------------------------- Time */

/** @returns A digital clock: date line above large time. */
lv_obj_t *sw_clock(lv_obj_t *parent, const char *time, const char *date);
void sw_clock_set(lv_obj_t *clock, const char *time, const char *date);

/** @returns An analog clock face with hour, minute and second hands. */
lv_obj_t *sw_analog_clock(lv_obj_t *parent, lv_coord_t size, uint8_t hour, uint8_t minute, uint8_t second);
void sw_analog_clock_set(lv_obj_t *clock, uint8_t hour, uint8_t minute, uint8_t second);

/** @returns A countdown ring with mm:ss in the center. */
lv_obj_t *sw_timer(lv_obj_t *parent, lv_coord_t size, uint32_t total_s, uint32_t remaining_s, sw_tone_t tone);
void sw_timer_set(lv_obj_t *timer, uint32_t remaining_s);

/* ---------------------------------------------------------------- Media */

/** @returns Title, artist and prev / play-pause / next controls (actions PREV, PLAY, NEXT). */
lv_obj_t *sw_now_playing(lv_obj_t *parent, const char *title, const char *artist, bool playing);
void sw_now_playing_set(lv_obj_t *player, const char *title, const char *artist, bool playing);

/** @returns A volume level of 0..100 drawn as an arc on the screen's right edge. */
lv_obj_t *sw_volume(lv_obj_t *screen, uint8_t percent);
void sw_volume_set(lv_obj_t *volume, uint8_t percent);

/* ---------------------------------------------------------------- Companion */

/** @returns A talk button whose icon, color and text follow the voice state (action VOICE). */
lv_obj_t *sw_voice_button(lv_obj_t *parent, sw_voice_t state);
void sw_voice_button_set(lv_obj_t *button, sw_voice_t state);

/** @returns Animated audio level bars; inactive bars rest flat. */
lv_obj_t *sw_waveform(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, sw_tone_t tone);
void sw_waveform_set_active(lv_obj_t *waveform, bool active);

/** @returns A 280 px row holding one message bubble: user messages right in blue, replies left in gray. */
lv_obj_t *sw_chat_bubble(lv_obj_t *parent, const char *text, bool from_user);

/** @returns A round frame for an avatar or image; add the picture as its child. */
lv_obj_t *sw_avatar_frame(lv_obj_t *parent, lv_coord_t size, sw_tone_t tone);

/* ---------------------------------------------------------------- Launcher */

/** @returns A full-screen honeycomb that arranges sw_app_icon() children, shrinking icons toward the edge. */
lv_obj_t *sw_app_grid(lv_obj_t *screen);

/** @returns A round app icon; tag it with sw_set_action() to tell apps apart. Label is used by styles that show it. */
lv_obj_t *sw_app_icon(lv_obj_t *grid, sw_icon_t icon, sw_tone_t tone, const char *label);

/* ---------------------------------------------------------------- Watch only */

/** @returns Move, exercise and stand rings (percent, may exceed 100). */
lv_obj_t *sw_activity_rings(lv_obj_t *parent, lv_coord_t size, uint16_t move, uint16_t exercise, uint16_t stand);
void sw_activity_rings_set(lv_obj_t *rings, uint16_t move, uint16_t exercise, uint16_t stand);

#endif
