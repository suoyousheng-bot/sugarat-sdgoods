/* SPDX-License-Identifier: Apache-2.0 */
/* LVGL view and input routing for the English SDGoods recorder. */

#include "ui_helloapp.h"

#include "recorder.h"
#include "sdgoods_board.h"
#include "sdgoods_swipe_up.h"
#include "sdgoods_tap.h"

#include <inttypes.h>
#include <stdio.h>

LV_FONT_DECLARE(sw_font_num_40);

#define INK     0xF8F0E8
#define MUTED   0xB8A69A
#define BG      0x141315
#define PANEL   0x242124
#define EDGE    0x3B3335
#define ACCENT  0xFF795A
#define WARM    0x412A28
#define ROWS    5
#define PITCH   64
#define STOP_HOLD_MS 1200
#define STOP_RING_FULL_MS 70
#define SAVE_SWEEP_STEPS 36
#define SAVE_STEP_MS 40
#define SAVE_WAVE_SPACING 12

typedef enum { HOME, MENU, LIBRARY } page_t;

static const uint8_t SAVE_WAVE_HEIGHTS[] = {10, 20, 34, 48, 62, 48, 34, 20, 10};

typedef struct {
    lv_obj_t *box, *name, *detail, *duration, *symbol;
    voice_clip_t clip;
    sdgoods_tap_ctx_t tap;
    bool held;
} row_t;

static page_t s_page, s_drawn_page;
static recorder_view_t s_drawn;
static lv_obj_t *s_screen, *s_content, *s_clock, *s_list, *s_slider, *s_volume_value;
static lv_obj_t *s_bars[8], *s_stop_ring;
static lv_obj_t *s_save_wave[sizeof SAVE_WAVE_HEIGHTS];
static uint32_t s_save_started, s_save_step;
static lv_point_t s_stop_origin;
static uint32_t s_stop_started, s_stop_completed;
static bool s_stop_held, s_stop_armed;
static sdgoods_tap_ctx_t s_primary_tap, s_entry_tap, s_library_tap, s_scrim_tap, s_cancel_tap, s_confirm_tap;
static row_t s_rows[ROWS];
static uint32_t s_confirm_id;
static bool s_consumed_hold, s_show_confirm, s_redraw = true;
static lv_coord_t s_scroll;
static uint32_t s_last_meter;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, int radius,
                     unsigned fill, unsigned border, bool button)
{
    lv_obj_t *obj = button ? lv_btn_create(parent) : lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    if (!button) lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    if (border) {
        lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
        lv_obj_set_style_border_width(obj, 1, 0);
    }
    if (button) lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(fill), 32), LV_STATE_PRESSED);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, const char *text,
                       unsigned color, lv_align_t align, int x, int y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, text);
    lv_obj_align(obj, align, x, y);
    return obj;
}

static void centered(lv_obj_t *parent, const char *text, unsigned color, int y)
{
    label(parent, &lv_font_montserrat_16, text, color, LV_ALIGN_TOP_MID, 0, y);
}

static void format_time(char buf[16], uint32_t seconds)
{
    snprintf(buf, 16, "%02" PRIu32 ":%02" PRIu32, seconds / 60, seconds % 60);
}

static void notice(lv_obj_t *parent, const char *text, int y)
{
    if (!text) return;
    lv_obj_t *message = label(parent, &lv_font_montserrat_14, text, MUTED, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_width(message, 224);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
}

static void open_menu(void)
{
    const recorder_state_t state = recorder_view().state;
    if (s_page == HOME && !s_show_confirm &&
        (state == REC_IDLE || state == REC_UNSAVED || state == REC_UNAVAILABLE)) s_page = MENU;
}

static void menu_tap(void *unused)
{
    (void)unused;
    open_menu();
}

static void close_menu(void *unused)
{
    (void)unused;
    if (s_page == MENU) s_page = HOME;
}

static void enter_library(void *unused)
{
    (void)unused;
    s_scroll = 0;
    s_page = LIBRARY;
}

static void gesture(lv_event_t *event)
{
    lv_indev_t *input = lv_indev_get_act();
    if (!input) return;
    const lv_dir_t dir = lv_indev_get_gesture_dir(input);
    if (s_show_confirm) return;
    if (s_page == HOME && dir == LV_DIR_TOP &&
        lv_event_get_target(event) != s_screen) {
        open_menu();
    } else if (s_page == MENU && dir == LV_DIR_BOTTOM) {
        close_menu(NULL);
    } else if (s_page == LIBRARY && dir == LV_DIR_RIGHT) {
        recorder_stop_playback();
        s_page = HOME;
    } else {
        return;
    }
    lv_indev_wait_release(input);
}

static void capture_tap(void *unused)
{
    (void)unused;
    if (!s_consumed_hold) recorder_toggle_capture();
    s_consumed_hold = false;
}

static void capture_hold(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
        s_consumed_hold = false;
    } else if (recorder_view().state == REC_UNSAVED &&
               (!s_primary_tap.valid || s_primary_tap.max_disp <= SDGOODS_TAP_SLOP)) {
        s_consumed_hold = true;
        s_show_confirm = true;
        s_confirm_id = 0;
        s_redraw = true;
    }
}

static void stop_hold(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *input = lv_indev_get_act();
    if (s_stop_armed || !input) return;
    lv_point_t point;
    lv_indev_get_point(input, &point);
    if (code == LV_EVENT_PRESSED) {
        s_stop_origin = point;
        s_stop_started = lv_tick_get();
        s_stop_held = true;
        return;
    }

    const int dx = point.x - s_stop_origin.x;
    const int dy = point.y - s_stop_origin.y;
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST ||
        LV_ABS(dx) > SDGOODS_TAP_SLOP || LV_ABS(dy) > SDGOODS_TAP_SLOP) {
        s_stop_held = false;
        lv_arc_set_value(s_stop_ring, 0);
        return;
    }
    if (!s_stop_held) return;
    const uint32_t elapsed = lv_tick_elaps(s_stop_started);
    lv_arc_set_value(s_stop_ring, elapsed < STOP_HOLD_MS ? elapsed : STOP_HOLD_MS);
    if (elapsed >= STOP_HOLD_MS) {
        s_stop_armed = true;
        s_stop_completed = lv_tick_get();
        lv_indev_wait_release(input);
    }
}

static void draw_stop_ring(void)
{
    s_stop_ring = lv_arc_create(s_content);
    lv_obj_remove_style_all(s_stop_ring);
    lv_obj_set_pos(s_stop_ring, 119, 132);
    lv_obj_set_size(s_stop_ring, 122, 122);
    lv_obj_clear_flag(s_stop_ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_arc_set_rotation(s_stop_ring, 270);
    lv_arc_set_bg_angles(s_stop_ring, 0, 360);
    lv_arc_set_range(s_stop_ring, 0, STOP_HOLD_MS);
    const int parts[] = {LV_PART_MAIN, LV_PART_INDICATOR};
    for (size_t i = 0; i < 2; ++i) {
        lv_obj_set_style_arc_color(s_stop_ring, lv_color_hex(i == 0 ? EDGE : ACCENT), parts[i]);
        lv_obj_set_style_arc_width(s_stop_ring, 4, parts[i]);
        lv_obj_set_style_arc_rounded(s_stop_ring, true, parts[i]);
    }
    lv_arc_set_value(s_stop_ring, 0);
}

static void mic_icon(lv_obj_t *button)
{
    box(button, 37, 46, 30, 24, 15, BG, 0, false);
    box(button, 40, 43, 24, 24, 12, ACCENT, 0, false);
    box(button, 44, 32, 16, 31, 8, BG, 0, false);
    box(button, 51, 69, 3, 8, 1, BG, 0, false);
    box(button, 45, 76, 15, 3, 1, BG, 0, false);
}

static void update_save_wave(uint32_t step)
{
    const lv_color_t dim = lv_color_hex(0x754138);
    const lv_color_t lit = lv_color_hex(0xFFAA8E);
    const int sweep_x = (int)step * 4 - 22;
    for (size_t i = 0; i < sizeof SAVE_WAVE_HEIGHTS; ++i) {
        const int distance = LV_ABS(sweep_x - (int)i * SAVE_WAVE_SPACING);
        const int glow = LV_MAX(0, 28 - distance);
        lv_obj_set_style_bg_color(s_save_wave[i], lv_color_mix(lit, dim, glow * 255 / 28), 0);
    }
    s_save_step = step;
}

static void draw_saving(uint32_t elapsed_ms)
{
    lv_obj_t *medallion = box(s_content, 109, 86, 142, 142, LV_RADIUS_CIRCLE, PANEL, EDGE, false);
    const int bar_width = 6;
    const int wave_width = (sizeof SAVE_WAVE_HEIGHTS - 1) * SAVE_WAVE_SPACING + bar_width;
    for (size_t i = 0; i < sizeof SAVE_WAVE_HEIGHTS; ++i) {
        const int height = SAVE_WAVE_HEIGHTS[i];
        const int x = (142 - wave_width) / 2 + (int)i * SAVE_WAVE_SPACING;
        s_save_wave[i] = box(medallion, x, (142 - height) / 2, bar_width, height, LV_RADIUS_CIRCLE,
                             ACCENT, 0, false);
    }
    s_save_started = lv_tick_get();
    update_save_wave(0);

    centered(s_content, "Saving recording", INK, 244);
    char time[16];
    format_time(time, elapsed_ms / 1000);
    notice(s_content, time, 271);
}

static void draw_home(recorder_view_t view)
{
    const bool recording = view.state == REC_RECORDING || view.state == REC_STARTING ||
                           view.state == REC_FINISHING;
    const bool waiting = view.state == REC_BOOT || view.state == REC_STARTING ||
                         view.state == REC_FINISHING ||
                         view.state == REC_PLAY_STOPPING || view.state == REC_PLAY_STARTING ||
                         view.state == REC_LOADING || view.state == REC_DELETING ||
                         view.state == REC_UNAVAILABLE;
    centered(s_content, recording ? "Recording" : "Voice Recorder", recording ? ACCENT : INK, 60);
    char time[16];
    format_time(time, view.elapsed_ms / 1000);
    s_clock = label(s_content, &sw_font_num_40, time, INK, LV_ALIGN_TOP_MID, 0, 89);

    if (view.state == REC_RECORDING) draw_stop_ring();
    lv_obj_t *record = box(s_content, 128, 141, 104, 104, LV_RADIUS_CIRCLE,
                          recording || waiting ? WARM : ACCENT, recording ? ACCENT : 0, !waiting);
    if (recording) {
        box(record, 34, 34, 34, 34, 7, ACCENT, 0, false);
        for (int i = 0; i < 8; ++i) {
            const int x = i < 4 ? 76 + i * 9 : 254 + (i - 4) * 9;
            s_bars[i] = box(s_content, x, 192, 3, 3, 2, ACCENT, 0, false);
        }
    } else if (!waiting) {
        mic_icon(record);
    } else {
        label(record, &lv_font_montserrat_16, view.state == REC_UNAVAILABLE ? "!" : "...",
              ACCENT, LV_ALIGN_CENTER, 0, 0);
    }

    if (view.state == REC_RECORDING) {
        sdgoods_tap_lock(record);
        lv_obj_add_event_cb(record, stop_hold, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(record, stop_hold, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(record, stop_hold, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(record, stop_hold, LV_EVENT_PRESS_LOST, NULL);
    } else if (!waiting) {
        sdgoods_tap_bind(record, &s_primary_tap, capture_tap, NULL);
        lv_obj_add_event_cb(record, capture_hold, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(record, capture_hold, LV_EVENT_LONG_PRESSED, NULL);
    }
    const bool unsaved = view.state == REC_UNSAVED;
    const char *action = view.state == REC_RECORDING ? "Hold to Stop & Save" : !waiting && !unsaved ? "Start Recording" : NULL;
    if (action) centered(s_content, action, INK, 254);
    if (view.message) notice(s_content, view.message, action ? 276 : 256);
    if (unsaved) notice(s_content, "Hold record to discard", 282);

    if (!recording && (!waiting || view.state == REC_UNAVAILABLE) && !unsaved && !view.message) {
        lv_obj_t *entry = box(s_content, 94, 280, 172, 48, 0, BG, 0, true);
        static const lv_point_t chevron[] = {{0, 5}, {6, 0}, {12, 5}};
        lv_obj_t *arrow = lv_line_create(entry);
        lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
        lv_line_set_points(arrow, chevron, 3);
        lv_obj_set_style_line_color(arrow, lv_color_hex(MUTED), 0);
        lv_obj_set_style_line_width(arrow, 2, 0);
        lv_obj_set_style_line_rounded(arrow, true, 0);
        lv_obj_align(arrow, LV_ALIGN_TOP_MID, 0, 9);
        label(entry, &lv_font_montserrat_14, "Swipe up for menu", MUTED, LV_ALIGN_TOP_MID, 0, 22);
        sdgoods_tap_bind(entry, &s_entry_tap, menu_tap, NULL);
        lv_obj_clear_flag(entry, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(entry, gesture, LV_EVENT_GESTURE, NULL);
    } else if (!recording && (!waiting || view.state == REC_UNAVAILABLE)) {
        lv_obj_t *entry = box(s_content, 102, 306, 156, 42, 0, BG, 0, true);
        label(entry, &lv_font_montserrat_14, "Swipe up for menu", MUTED, LV_ALIGN_TOP_MID, 0, 8);
        sdgoods_tap_bind(entry, &s_entry_tap, menu_tap, NULL);
        lv_obj_clear_flag(entry, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(entry, gesture, LV_EVENT_GESTURE, NULL);
    }
    if (!recording) sdgoods_swipe_up_bind(s_content, open_menu);
}

static lv_obj_t *scrim(void)
{
    lv_obj_t *shade = box(s_content, 0, 0, 360, 360, 0, 0, 0, true);
    lv_obj_set_style_bg_opa(shade, LV_OPA_60, 0);
    return shade;
}

static void draw_menu(void)
{
    sdgoods_tap_bind(scrim(), &s_scrim_tap, close_menu, NULL);
    lv_obj_t *sheet = box(s_content, 0, 213, 360, 185, 30, 0x1D1A1D, EDGE, false);
    lv_obj_t *handle = box(sheet, 88, 0, 182, 31, 0, 0x1D1A1D, 0, false);
    lv_obj_add_flag(handle, LV_OBJ_FLAG_CLICKABLE);
    box(handle, 69, 12, 44, 4, 2, MUTED, 0, false);

    lv_obj_t *entry = box(sheet, 65, 32, 228, 58, 16, PANEL, EDGE, true);
    lv_obj_t *icon = box(entry, 11, 10, 36, 36, 18, WARM, 0, false);
    for (int i = 0; i < 3; ++i) {
        box(icon, 10, 12 + i * 6, 2, 2, 1, ACCENT, 0, false);
        box(icon, 15, 12 + i * 6, 11, 2, 1, ACCENT, 0, false);
    }
    label(entry, &lv_font_montserrat_16, "Recordings", INK, LV_ALIGN_LEFT_MID, 58, 0);
    label(entry, &lv_font_montserrat_14, LV_SYMBOL_RIGHT, MUTED, LV_ALIGN_RIGHT_MID, -14, 0);
    sdgoods_tap_bind(entry, &s_library_tap, enter_library, NULL);
}

static void row_tap(void *data)
{
    row_t *row = data;
    if (!row->held) recorder_toggle_playback(row->clip);
    row->held = false;
}

static void row_hold(lv_event_t *event)
{
    row_t *row = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
        row->held = false;
        return;
    }
    const recorder_state_t state = recorder_view().state;
    if (!row->clip.id || (state != REC_IDLE && state != REC_UNSAVED) ||
        (row->tap.valid && row->tap.max_disp > SDGOODS_TAP_SLOP)) return;
    row->held = true;
    s_show_confirm = true;
    s_confirm_id = row->clip.id;
    s_redraw = true;
}

static void create_row(row_t *row)
{
    row->box = box(s_list, 0, 0, 226, 56, 15, PANEL, EDGE, true);
    lv_obj_t *icon = box(row->box, 9, 8, 38, 38, 19, WARM, 0, false);
    row->symbol = label(icon, &lv_font_montserrat_14, LV_SYMBOL_PLAY, ACCENT, LV_ALIGN_CENTER, 0, 0);
    row->name = label(row->box, &lv_font_montserrat_14, "", INK, LV_ALIGN_TOP_LEFT, 55, 8);
    lv_obj_set_width(row->name, 117);
    lv_label_set_long_mode(row->name, LV_LABEL_LONG_DOT);
    row->detail = label(row->box, &lv_font_montserrat_14, "", MUTED, LV_ALIGN_TOP_LEFT, 55, 30);
    row->duration = label(row->box, &lv_font_montserrat_14, "", INK, LV_ALIGN_RIGHT_MID, -12, 0);
    row->clip = (voice_clip_t){0};
    sdgoods_tap_bind(row->box, &row->tap, row_tap, row);
    lv_obj_add_event_cb(row->box, row_hold, LV_EVENT_PRESSED, row);
    lv_obj_add_event_cb(row->box, row_hold, LV_EVENT_LONG_PRESSED, row);
}

static void bind_row(row_t *row, voice_clip_t clip, int y, bool playing, bool active, bool paused)
{
    lv_obj_clear_flag(row->box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(row->box, y);
    if (row->clip.id == clip.id) return;
    row->clip = clip;
    lv_obj_set_style_bg_color(row->box, lv_color_hex(active ? 0x342522 : PANEL), 0);
    lv_obj_set_style_border_color(row->box, lv_color_hex(active ? 0x906150 : EDGE), 0);
    lv_label_set_text(row->symbol, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_label_set_text_fmt(row->name, "Recording %03" PRIu32, clip.id);
    lv_label_set_text(row->detail, playing ? "Playing" : paused ? "Paused" : active ? "Working..." : "WAV");
    lv_obj_set_style_text_color(row->detail, lv_color_hex(playing ? ACCENT : MUTED), 0);
    char time[16];
    format_time(time, (clip.samples + 15999) / 16000);
    lv_label_set_text(row->duration, time);
    lv_obj_align(row->duration, LV_ALIGN_RIGHT_MID, -12, 0);
}

static int row_position(size_t index, int active)
{
    return (int)index * PITCH + (active >= 0 && index > (size_t)active ? 54 : 0);
}

static int s_volume_row = -1;

static void refresh_rows(void)
{
    const int scroll = lv_obj_get_scroll_y(s_list);
    const recorder_view_t view = recorder_view();
    size_t index = 0;
    while (index < view.count && row_position(index, s_volume_row) + 56 <= scroll) ++index;
    for (int slot = 0; slot < ROWS; ++slot, ++index) {
        row_t *row = &s_rows[index % ROWS];
        const voice_clip_t clip = recorder_clip(index);
        if (!clip.id) {
            lv_obj_add_flag(row->box, LV_OBJ_FLAG_HIDDEN);
            row->clip = (voice_clip_t){0};
            continue;
        }
        const bool active = clip.id == view.clip_id;
        bind_row(row, clip, row_position(index, s_volume_row), active && view.state == REC_PLAYING,
                 active, active && view.state == REC_PAUSED);
    }
}

static void list_scroll(lv_event_t *unused)
{
    (void)unused;
    refresh_rows();
}

static void volume_changed(lv_event_t *event)
{
    const int volume = lv_slider_get_value(lv_event_get_target(event));
    sdgoods_audio_set_volume(volume);
    lv_label_set_text_fmt(s_volume_value, "%d", volume);
    lv_obj_align(s_volume_value, LV_ALIGN_RIGHT_MID, -10, 0);
}

static void volume_control(int y)
{
    lv_obj_t *panel = box(s_list, 0, y, 226, 48, 15, PANEL, EDGE, false);
    label(panel, &lv_font_montserrat_14, "Volume", INK, LV_ALIGN_LEFT_MID, 12, 0);
    s_slider = lv_slider_create(panel);
    lv_obj_remove_style_all(s_slider);
    lv_obj_clear_flag(s_slider, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_size(s_slider, 112, 6);
    lv_obj_align(s_slider, LV_ALIGN_LEFT_MID, 72, 0);
    lv_obj_set_ext_click_area(s_slider, 16);
    const int parts[] = {LV_PART_MAIN, LV_PART_INDICATOR, LV_PART_KNOB};
    for (int i = 0; i < 3; ++i) {
        lv_obj_set_style_bg_opa(s_slider, LV_OPA_COVER, parts[i]);
        lv_obj_set_style_bg_color(s_slider, lv_color_hex(i == 0 ? EDGE : ACCENT), parts[i]);
        lv_obj_set_style_radius(s_slider, LV_RADIUS_CIRCLE, parts[i]);
    }
    lv_obj_set_style_pad_all(s_slider, 5, LV_PART_KNOB);
    const int volume = sdgoods_audio_get_volume();
    lv_slider_set_value(s_slider, volume, LV_ANIM_OFF);
    s_volume_value = label(panel, &lv_font_montserrat_14, "", MUTED, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_label_set_text_fmt(s_volume_value, "%d", volume);
    lv_obj_align(s_volume_value, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(s_slider, volume_changed, LV_EVENT_VALUE_CHANGED, NULL);
}

static void draw_library(recorder_view_t view)
{
    centered(s_content, "Recordings", INK, 61);
    notice(s_content, "Swipe right to go back", 300);
    if (!view.storage_ready || !view.count) {
        notice(s_content, view.storage_ready ? "No recordings yet" : "Storage unavailable", 168);
        return;
    }
    notice(s_content, view.state == REC_UNSAVED ? "Hold a recording to delete" : view.message, 277);
    const int height = view.message ? 174 : 185;
    s_list = box(s_content, 63, 95, 236, height, 0, BG, 0, false);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x9B8079), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(s_list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(s_list, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(s_list, 1, LV_PART_SCROLLBAR);

    s_volume_row = -1;
    if (view.state == REC_PLAYING) {
        for (size_t i = 0; i < view.count; ++i) {
            if (recorder_clip(i).id == view.clip_id) {
                s_volume_row = (int)i;
                break;
            }
        }
    }
    if (s_volume_row >= 0) volume_control(s_volume_row * PITCH + 62);
    // A noninteractive spacer keeps the content height independent of the recycled rows.
    box(s_list, 0, row_position(view.count - 1, s_volume_row) + 55, 1, 1, 0, BG, 0, false);
    for (size_t i = 0; i < ROWS; i++) create_row(&s_rows[i]);
    lv_obj_update_layout(s_screen);
    lv_obj_scroll_to_y(s_list, s_scroll, LV_ANIM_OFF);
    refresh_rows();
    lv_obj_add_event_cb(s_list, list_scroll, LV_EVENT_SCROLL, NULL);
}

static void close_confirm(void *unused)
{
    (void)unused;
    s_show_confirm = false;
    s_redraw = true;
}

static void confirm(void *unused)
{
    close_confirm(unused);
    if (s_confirm_id) recorder_delete(s_confirm_id);
    else recorder_discard();
}

static void draw_confirm(void)
{
    scrim();
    lv_obj_t *dialog = box(s_content, 62, 119, 236, 124, 24, PANEL, EDGE, false);
    centered(dialog, s_confirm_id ? "Delete recording?" : "Discard unsaved audio?", INK, 22);
    lv_obj_t *cancel = box(dialog, 12, 62, 100, 48, 14, BG, EDGE, true);
    lv_obj_t *apply = box(dialog, 122, 62, 100, 48, 14, WARM, ACCENT, true);
    label(cancel, &lv_font_montserrat_14, "Cancel", INK, LV_ALIGN_CENTER, 0, 0);
    label(apply, &lv_font_montserrat_14, s_confirm_id ? "Delete" : "Discard", ACCENT, LV_ALIGN_CENTER, 0, 0);
    sdgoods_tap_bind(cancel, &s_cancel_tap, close_confirm, NULL);
    sdgoods_tap_bind(apply, &s_confirm_tap, confirm, NULL);
}

static void redraw(recorder_view_t view)
{
    if (s_list && s_page == LIBRARY) s_scroll = lv_obj_get_scroll_y(s_list);
    lv_obj_clean(s_content);
    s_list = s_clock = s_slider = s_volume_value = s_stop_ring = NULL;
    s_stop_held = s_stop_armed = false;
    for (size_t i = 0; i < 8; ++i) s_bars[i] = NULL;

    if (view.state == REC_SAVING) {
        draw_saving(view.elapsed_ms);
    } else if (s_page == LIBRARY) {
        draw_library(view);
    } else {
        draw_home(view);
        if (s_page == MENU) draw_menu();
    }
    if (s_show_confirm) draw_confirm();
    sdgoods_tap_normalize(s_screen);
    s_drawn = view;
    s_drawn_page = s_page;
    s_redraw = false;
}

void ui_helloapp_start(void)
{
    if (!s_screen) {
        s_screen = lv_obj_create(NULL);
        lv_obj_remove_style_all(s_screen);
        lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
        s_content = box(s_screen, 0, 0, 360, 360, 0, BG, 0, false);
        lv_obj_add_event_cb(s_screen, gesture, LV_EVENT_GESTURE, NULL);
    }
    lv_scr_load(s_screen);
    sdgoods_app_shell_bind(s_screen);
    sdgoods_app_shell_set_pause_cb(recorder_pause);
    redraw(recorder_view());
}

void ui_helloapp_poll(void)
{
    recorder_poll();
    if (!s_screen || lv_scr_act() != s_screen) return;
    recorder_view_t view = recorder_view();
    if (s_stop_armed && view.state == REC_RECORDING &&
        lv_tick_elaps(s_stop_completed) >= STOP_RING_FULL_MS) {
        recorder_toggle_capture();
        view = recorder_view();
    }
    if (s_redraw || s_drawn_page != s_page || view.state != s_drawn.state ||
        view.message != s_drawn.message || view.count != s_drawn.count) redraw(view);
    if (view.state == REC_SAVING) {
        const uint32_t step = lv_tick_elaps(s_save_started) / SAVE_STEP_MS % SAVE_SWEEP_STEPS;
        if (step != s_save_step) update_save_wave(step);
    }
    if (s_clock && view.elapsed_ms / 1000 != s_drawn.elapsed_ms / 1000) {
        char time[16];
        format_time(time, view.elapsed_ms / 1000);
        lv_label_set_text(s_clock, time);
        lv_obj_align(s_clock, LV_ALIGN_TOP_MID, 0, 89);
    }
    s_drawn.elapsed_ms = view.elapsed_ms;
    if (view.state == REC_RECORDING && s_bars[0] && lv_tick_elaps(s_last_meter) >= 80) {
        s_last_meter = lv_tick_get();
        static const uint8_t weights[] = {4, 9, 22, 11, 10, 25, 13, 5};
        for (size_t i = 0; i < 8; ++i) {
            const int height = 3 + view.level * weights[i] / 100;
            lv_obj_set_height(s_bars[i], height);
            lv_obj_set_y(s_bars[i], 193 - height / 2);
        }
    }
}
