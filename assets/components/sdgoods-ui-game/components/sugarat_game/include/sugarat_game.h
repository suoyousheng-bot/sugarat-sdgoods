/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Sugarat Game: retro pixel-game LVGL 8.3 components for 360 x 360 round displays.
 *
 * Sugarat Watch (sugarat_watch.h) exposes the same functions, enums and icon names
 * with the sw_ prefix, so a screen written with one library switches style by
 * renaming the prefix. Section "Game only" adds RPG components (HP bars, dialog,
 * menu, inventory). Boxes, rings, charts and icons are drawn on a 2 px pixel grid. Build screens from these components instead of styling raw
 * LVGL widgets: every component removes LVGL theme styles and applies its own.
 *
 * Usage rules:
 * - Call every function on the LVGL thread after lv_init() and sg_init().
 * - Text arguments are copied unless a comment says otherwise.
 * - Text fonts bundle common Chinese characters plus literals found in sources;
 *   run tools/make_fonts.py (see README) when an app needs others.
 * - Interactive parts report taps as LV_EVENT_CLICKED with LV_OBJ_FLAG_EVENT_BUBBLE,
 *   so one handler on a component (or the screen) receives them; read which part
 *   was tapped with sg_event_action(). Value widgets send LV_EVENT_VALUE_CHANGED.
 * - Returned objects are ordinary lv_obj_t children; position them with
 *   lv_obj_align() or place them in sg_page()/sg_row()/sg_column().
 */
#ifndef SUGARAT_GAME_H
#define SUGARAT_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "sg_icons.h"

#define SG_SCREEN 360

/** Semantic colors; sg_color() maps them to a 16-color pixel palette. */
typedef enum {
    SG_TONE_BG,      /**< Screen background (night blue). */
    SG_TONE_SURFACE, /**< Cards and list rows. */
    SG_TONE_RAISED,  /**< Buttons and tracks on cards. */
    SG_TONE_TEXT,    /**< Primary text. */
    SG_TONE_MUTED,   /**< Secondary text. */
    SG_TONE_ACCENT,  /**< Highlight (gold). */
    SG_TONE_RED,
    SG_TONE_ORANGE,
    SG_TONE_YELLOW,
    SG_TONE_GREEN,
    SG_TONE_TEAL,
    SG_TONE_BLUE,
    SG_TONE_PURPLE,
    SG_TONE_PINK,
    SG_TONE_COUNT,
} sg_tone_t;

/** Pixel text roles (1-bit). Chinese uses 12 px glyphs scaled x2/x3; Latin uses an 8 px font scaled by whole steps. */
typedef enum {
    SG_FONT_CAPTION,  /**< 16 px Chinese, 16 px Latin. */
    SG_FONT_BODY,     /**< 24 px Chinese, 16 px Latin. */
    SG_FONT_HEADLINE, /**< Same as BODY, drawn in the accent by components. */
    SG_FONT_TITLE,    /**< 36 px Chinese, 24 px Latin. */
    SG_FONT_NUM_S,    /**< 24 px digits and Latin. */
    SG_FONT_NUM_M,    /**< 32 px digits; other characters fall back to TITLE. */
    SG_FONT_NUM_L,    /**< 56 px digits for clocks. */
    SG_FONT_COUNT,
} sg_font_t;

/** Icon sizes: 8 x 8 pixel art scaled to 16, 24, 32 and 40 px. */
typedef enum { SG_SIZE_S, SG_SIZE_M, SG_SIZE_L, SG_SIZE_XL } sg_size_t;

/** Button looks: FILLED uses the tone as background, TINTED a dark fill with tone text, PLAIN a neutral fill. */
typedef enum { SG_BUTTON_FILLED, SG_BUTTON_TINTED, SG_BUTTON_PLAIN } sg_button_variant_t;

/** Voice assistant states shown by sg_voice_button(). */
typedef enum { SG_VOICE_IDLE, SG_VOICE_LISTENING, SG_VOICE_THINKING, SG_VOICE_SPEAKING, SG_VOICE_OFFLINE } sg_voice_t;

/** Built-in action ids reported by sg_event_action(); app-assigned ids use 0..999. */
typedef enum {
    SG_ACTION_NONE = -1,
    SG_ACTION_CONFIRM = 1000,
    SG_ACTION_CANCEL,
    SG_ACTION_BACK,
    SG_ACTION_PREV,
    SG_ACTION_PLAY,
    SG_ACTION_NEXT,
    SG_ACTION_VOICE,
} sg_action_t;

/** Pixel box looks for sg_box(). */
typedef enum {
    SG_BOX_PANEL,  /**< Raised panel with bevel and hard shadow. */
    SG_BOX_BUTTON, /**< Panel in the tone color; sinks into its shadow while pressed. */
    SG_BOX_FRAME,  /**< RPG window: dark outline, light border, deep fill. */
    SG_BOX_INSET,  /**< Sunken track for bars and slots. */
} sg_box_style_t;

/** Side of the pixel grid in screen pixels; outlines, bevels and shadows are one unit thick. */
#define SG_UNIT 2

/* ---------------------------------------------------------------- Foundation */

/** Prepare the library; safe to call more than once. */
void sg_init(void);

/** @returns The color of a tone. */
lv_color_t sg_color(sg_tone_t tone);

/** @returns The font of a text role. */
const lv_font_t *sg_font(sg_font_t font);

/** @returns A new night-blue 360 x 360 screen; load it with lv_scr_load(). */
lv_obj_t *sg_screen(void);

/** @returns A label in the given role and tone. */
lv_obj_t *sg_label(lv_obj_t *parent, const char *text, sg_font_t font, sg_tone_t tone);

/** @returns A pixel-art icon object; SG_ICON_NONE draws nothing. */
lv_obj_t *sg_icon(lv_obj_t *parent, sg_icon_t icon, sg_size_t size, sg_tone_t tone);

/** Change the glyph and tone of an sg_icon(). */
void sg_icon_set(lv_obj_t *icon_obj, sg_icon_t icon, sg_tone_t tone);

/**
 * Draw a pixel box behind obj's children; give obj no background, border or shadow style of its own.
 * Shadowed boxes leave SG_UNIT free on the right and bottom and sink into the shadow while pressed;
 * tone colors BUTTON fills and FRAME borders.
 */
void sg_box(lv_obj_t *obj, sg_box_style_t style, sg_tone_t tone, bool shadow);
/** Change the tone of an sg_box(). */
void sg_box_set_tone(lv_obj_t *obj, sg_tone_t tone);

/** Tag a button, list item or app icon with an action id (0..999 or sg_action_t); uses the object's user data. */
void sg_set_action(lv_obj_t *obj, int action);

/** @returns The action id of the tapped part for a bubbled LV_EVENT_CLICKED, or SG_ACTION_NONE. */
int sg_event_action(lv_event_t *e);

/* ---------------------------------------------------------------- Layout */

/** @returns A full-screen vertical scroller; children stack centered. Title (may be NULL) sits in a ribbon. */
lv_obj_t *sg_page(lv_obj_t *screen, const char *title);

/** @returns A transparent horizontal group that sizes to its content. */
lv_obj_t *sg_row(lv_obj_t *parent);

/** @returns A transparent vertical group that sizes to its content. */
lv_obj_t *sg_column(lv_obj_t *parent);

/** @returns A 280 px wide pixel panel that stacks children vertically. */
lv_obj_t *sg_card(lv_obj_t *parent);

/** @returns A small section heading for sg_page(). */
lv_obj_t *sg_section(lv_obj_t *parent, const char *title);

/** @returns A status bar at the top of the screen with time, Wi-Fi and battery. */
lv_obj_t *sg_status_bar(lv_obj_t *screen, const char *time);
void sg_status_bar_set_time(lv_obj_t *bar, const char *time);
/** Show battery level; below 20 % the icon turns red, while charging it turns green. */
void sg_status_bar_set_battery(lv_obj_t *bar, uint8_t percent, bool charging);
void sg_status_bar_set_connected(lv_obj_t *bar, bool connected);

/** @returns Page indicator dots aligned to the bottom of the parent. */
lv_obj_t *sg_page_dots(lv_obj_t *parent, uint8_t count, uint8_t active);
void sg_page_dots_set(lv_obj_t *dots, uint8_t active);

/** @returns A block scrollbar on the right screen edge that follows the page's scroll position. */
lv_obj_t *sg_scroll_indicator(lv_obj_t *page);

/* ---------------------------------------------------------------- Controls */

/** @returns A bevelled pixel button that sinks while pressed; icon or text may be SG_ICON_NONE / NULL. Width follows content; call lv_obj_set_width() to fix it. */
lv_obj_t *sg_button(lv_obj_t *parent, sg_icon_t icon, const char *text, sg_button_variant_t variant, sg_tone_t tone);

/** @returns A 56 px square icon button. */
lv_obj_t *sg_icon_button(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone);

/** @returns A 64 px square on/off button; toggles LV_STATE_CHECKED and sends VALUE_CHANGED. */
lv_obj_t *sg_toggle_button(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, bool on);

/** @returns A pixel on/off switch; read with lv_obj_has_state(obj, LV_STATE_CHECKED). */
lv_obj_t *sg_toggle(lv_obj_t *parent, bool on);

/** @returns A square checkbox with a label; sends VALUE_CHANGED and reports LV_STATE_CHECKED. */
lv_obj_t *sg_checkbox(lv_obj_t *parent, const char *text, bool checked);

/** @returns A horizontal slider; read with lv_slider_get_value(). */
lv_obj_t *sg_slider(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, sg_tone_t tone);

/** @returns A "- value +" stepper that sends VALUE_CHANGED. */
lv_obj_t *sg_stepper(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, int32_t step);
int32_t sg_stepper_get(lv_obj_t *stepper);
void sg_stepper_set(lv_obj_t *stepper, int32_t value);

/** @returns A single-choice segmented control; options must have static lifetime. Sends VALUE_CHANGED. */
lv_obj_t *sg_segmented(lv_obj_t *parent, const char *const *options, uint8_t count, uint8_t selected);
uint8_t sg_segmented_get(lv_obj_t *segmented);

/** @returns A 10-segment progress bar of 0..100. */
lv_obj_t *sg_progress(lv_obj_t *parent, uint8_t percent, sg_tone_t tone);
void sg_progress_set(lv_obj_t *progress, uint8_t percent);

/** @returns A ring of square blocks showing 0..100 with an empty center for other content. */
lv_obj_t *sg_ring(lv_obj_t *parent, lv_coord_t size, uint8_t percent, sg_tone_t tone);
void sg_ring_set(lv_obj_t *ring, uint8_t percent);

/** @returns A ring of blocks with a running highlight. */
lv_obj_t *sg_spinner(lv_obj_t *parent, lv_coord_t size, sg_tone_t tone);

/** @returns A wheel picker; options are newline-separated. Read with lv_roller_get_selected(). */
lv_obj_t *sg_roller(lv_obj_t *parent, const char *options, uint16_t selected, uint8_t visible_rows);

/* ---------------------------------------------------------------- Lists */

/** @returns A 280 px tappable pixel row with optional icon (SG_ICON_NONE) and subtitle (NULL). */
lv_obj_t *sg_list_item(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *title, const char *subtitle);
/** @returns A right-aligned value label added to a list item. */
lv_obj_t *sg_list_item_value(lv_obj_t *item, const char *text);
/** @returns A switch added to a list item. */
lv_obj_t *sg_list_item_toggle(lv_obj_t *item, bool on);
/** Add a disclosure chevron to a list item. */
void sg_list_item_chevron(lv_obj_t *item);

/* ---------------------------------------------------------------- Feedback */

/** @returns A count tag, or a square dot when text is NULL. */
lv_obj_t *sg_badge(lv_obj_t *parent, const char *text, sg_tone_t tone);

/** @returns A top banner that deletes itself after duration_ms (0 keeps it). parent NULL uses lv_layer_top(). */
lv_obj_t *sg_toast(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *text, uint32_t duration_ms);

/** @returns A notification card with app icon, app name, title and body. */
lv_obj_t *sg_notification(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *app, const char *title, const char *body);

/**
 * @returns A full-screen modal with confirm (and optional cancel, NULL hides it) buttons.
 * Taps report SG_ACTION_CONFIRM or SG_ACTION_CANCEL on the returned object, which then deletes itself.
 * parent NULL uses lv_layer_top().
 */
lv_obj_t *sg_alert(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *title, const char *message,
                   const char *confirm, const char *cancel);

/** @returns A centered empty-state block with icon, title and hint (hint may be NULL). */
lv_obj_t *sg_empty_state(lv_obj_t *parent, sg_icon_t icon, const char *title, const char *hint);

/** @returns A spinner with a caption below. */
lv_obj_t *sg_loading(lv_obj_t *parent, const char *text);

/* ---------------------------------------------------------------- Data */

/** @returns A metric: caption label, large value and unit (unit may be NULL). */
lv_obj_t *sg_stat(lv_obj_t *parent, const char *value, const char *unit, const char *label, sg_tone_t tone);
void sg_stat_set(lv_obj_t *stat, const char *value);

/** @returns A 270 degree block gauge showing value and a label in its center. */
lv_obj_t *sg_gauge(lv_obj_t *parent, lv_coord_t size, int32_t min, int32_t max, int32_t value, const char *label, sg_tone_t tone);
void sg_gauge_set(lv_obj_t *gauge, int32_t value);

/** @returns A compact card with icon, value and label. */
lv_obj_t *sg_tile(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *value, const char *label);
void sg_tile_set(lv_obj_t *tile, const char *value);

/** @returns A bar chart of stacked blocks scaled to the largest value; up to 32 values. */
lv_obj_t *sg_bar_chart(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sg_tone_t tone);
void sg_bar_chart_set(lv_obj_t *chart, const int16_t *values, uint8_t count);

/** @returns A stepped pixel line chart scaled to the value range; up to 32 values. */
lv_obj_t *sg_sparkline(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sg_tone_t tone);
void sg_sparkline_set(lv_obj_t *chart, const int16_t *values, uint8_t count);

/* ---------------------------------------------------------------- Time */

/** @returns A digital clock: date line above large time. */
lv_obj_t *sg_clock(lv_obj_t *parent, const char *time, const char *date);
void sg_clock_set(lv_obj_t *clock, const char *time, const char *date);

/** @returns A pixel analog clock with block hour marks and blocky hands. */
lv_obj_t *sg_analog_clock(lv_obj_t *parent, lv_coord_t size, uint8_t hour, uint8_t minute, uint8_t second);
void sg_analog_clock_set(lv_obj_t *clock, uint8_t hour, uint8_t minute, uint8_t second);

/** @returns A countdown block ring with mm:ss in the center. */
lv_obj_t *sg_timer(lv_obj_t *parent, lv_coord_t size, uint32_t total_s, uint32_t remaining_s, sg_tone_t tone);
void sg_timer_set(lv_obj_t *timer, uint32_t remaining_s);

/* ---------------------------------------------------------------- Media */

/** @returns Title, artist and prev / play-pause / next controls (actions PREV, PLAY, NEXT). */
lv_obj_t *sg_now_playing(lv_obj_t *parent, const char *title, const char *artist, bool playing);
void sg_now_playing_set(lv_obj_t *player, const char *title, const char *artist, bool playing);

/** @returns A volume level of 0..100 drawn as blocks along the screen's right edge. */
lv_obj_t *sg_volume(lv_obj_t *screen, uint8_t percent);
void sg_volume_set(lv_obj_t *volume, uint8_t percent);

/* ---------------------------------------------------------------- Companion */

/** @returns A talk button whose icon, color and text follow the voice state (action VOICE). */
lv_obj_t *sg_voice_button(lv_obj_t *parent, sg_voice_t state);
void sg_voice_button_set(lv_obj_t *button, sg_voice_t state);

/** @returns Animated block level bars; inactive bars rest flat. */
lv_obj_t *sg_waveform(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, sg_tone_t tone);
void sg_waveform_set_active(lv_obj_t *waveform, bool active);

/** @returns A 280 px row holding one pixel speech box: user messages right in blue, replies left. */
lv_obj_t *sg_chat_bubble(lv_obj_t *parent, const char *text, bool from_user);

/** @returns A square framed slot for an avatar or image; add the picture as its child. */
lv_obj_t *sg_avatar_frame(lv_obj_t *parent, lv_coord_t size, sg_tone_t tone);

/* ---------------------------------------------------------------- Launcher */

/** @returns A centered 3-column grid that arranges sg_app_icon() children; nine fit the round screen. */
lv_obj_t *sg_app_grid(lv_obj_t *screen);

/** @returns A square app tile with its label below; tag it with sg_set_action() to tell apps apart. */
lv_obj_t *sg_app_icon(lv_obj_t *grid, sg_icon_t icon, sg_tone_t tone, const char *label);

/* ---------------------------------------------------------------- Game only */

/** @returns A segmented stat bar such as HP or MP: icon, blocks and "value/max". */
lv_obj_t *sg_hp_bar(lv_obj_t *parent, sg_icon_t icon, int32_t value, int32_t max, sg_tone_t tone);
void sg_hp_bar_set(lv_obj_t *bar, int32_t value);

/** @returns A row of max hearts, the first value filled. */
lv_obj_t *sg_hearts(lv_obj_t *parent, uint8_t max, uint8_t value);
void sg_hearts_set(lv_obj_t *hearts, uint8_t value);

/** @returns An RPG dialog box with a name tag, typewriter text and a blinking continue arrow. name may be NULL. */
lv_obj_t *sg_dialog(lv_obj_t *parent, const char *name, const char *text);
/** Replace the text and restart the typewriter. */
void sg_dialog_set_text(lv_obj_t *dialog, const char *text);
/** Show the whole text at once. @returns true if text was still being typed. */
bool sg_dialog_skip(lv_obj_t *dialog);
/** @returns A 72 px framed portrait slot on the dialog's left; add an sp_avatar() or image as its child. */
lv_obj_t *sg_dialog_portrait(lv_obj_t *dialog);

/** @returns A framed menu with a cursor; options must have static lifetime. Taps select and report the index as action. */
lv_obj_t *sg_menu(lv_obj_t *parent, const char *const *options, uint8_t count, uint8_t selected);
void sg_menu_set_selected(lv_obj_t *menu, uint8_t selected);
uint8_t sg_menu_get_selected(lv_obj_t *menu);

/** @returns An icon with "x value", for coins, gems or keys. */
lv_obj_t *sg_counter(lv_obj_t *parent, sg_icon_t icon, int32_t value, sg_tone_t tone);
void sg_counter_set(lv_obj_t *counter, int32_t value);

/** @returns An "LV n" tag with an experience bar of 0..100. */
lv_obj_t *sg_level(lv_obj_t *parent, uint16_t level, uint8_t exp_percent);
void sg_level_set(lv_obj_t *level, uint16_t value, uint8_t exp_percent);

/** @returns A grid of empty item slots (cols x rows <= 32); slots report their index as action. */
lv_obj_t *sg_inventory(lv_obj_t *parent, uint8_t cols, uint8_t rows);
/** Put an item in a slot; count 0 hides the number and SG_ICON_NONE empties it. */
void sg_inventory_set(lv_obj_t *inventory, uint8_t slot, sg_icon_t icon, sg_tone_t tone, uint16_t count);
/** Move the selection frame to a slot. */
void sg_inventory_select(lv_obj_t *inventory, uint8_t slot);

/** @returns An "achievement unlocked" banner that deletes itself after duration_ms (0 keeps it). parent NULL uses lv_layer_top(). */
lv_obj_t *sg_achievement(lv_obj_t *parent, sg_icon_t icon, const char *title, uint32_t duration_ms);

/** @returns A title screen block: shadowed title, subtitle (may be NULL) and a blinking prompt. */
lv_obj_t *sg_title_screen(lv_obj_t *parent, const char *title, const char *subtitle, const char *prompt);

#endif
