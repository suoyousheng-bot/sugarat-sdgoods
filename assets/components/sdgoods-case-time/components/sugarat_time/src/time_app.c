/* SPDX-License-Identifier: Apache-2.0 */
/** Four time tools built from Sugarat Watch components, with screen-owned state. */
#include "time_app.h"
#include "time_model.h"
#include "sugarat_watch.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum { PAGE_FOCUS, PAGE_COUNTDOWN, PAGE_STOPWATCH, PAGE_CLOCK, PAGE_COUNT };
enum { ACT_LEFT = 1, ACT_RIGHT, ACT_PRIMARY, ACT_SECONDARY, ACT_PRESET_0, ACT_PRESET_1,
       ACT_PRESET_2, ACT_EDIT, ACT_SAVE, ACT_CANCEL, ACT_LANGUAGE };

typedef struct {
    time_model_t model;
    uint64_t (*now_ms)(void);
    lv_obj_t *screen, *content, *notices, *value, *detail, *status, *ring, *hours, *minutes;
    lv_timer_t *ticker;
    unsigned page;
    time_locale_t locale;
    const time_copy_t *copy;
    bool dirty, editing, gallery_navigation;
} time_app_t;

static const sw_tone_t tones[] = {SW_TONE_ORANGE, SW_TONE_TEAL, SW_TONE_GREEN, SW_TONE_BLUE};
static void render(time_app_t *app);

static lv_obj_t *text(lv_obj_t *parent, const char *value, sw_font_t font, sw_tone_t tone, int y)
{
    lv_obj_t *label = sw_label(parent, value, font, tone);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static lv_obj_t *button(time_app_t *app, const char *label, sw_icon_t icon, int action,
                        int x, int y, int width, int height, bool filled)
{
    lv_obj_t *obj = sw_button(app->content, icon, label,
                             filled ? SW_BUTTON_FILLED : SW_BUTTON_PLAIN, tones[app->page]);
    sw_set_action(obj, action);
    lv_obj_set_style_min_width(obj, 0, 0);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_pad_hor(obj, 0, 0);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static void update_values(time_app_t *app)
{
    time_model_t *model = &app->model;
    const time_copy_t *copy = app->copy;
    if (app->editing) return;
    if (app->page == PAGE_FOCUS || app->page == PAGE_COUNTDOWN) {
        time_countdown_t *timer = app->page == PAGE_FOCUS ? &model->focus : &model->countdown;
        unsigned seconds = (unsigned)((timer->remaining_ms + 999) / 1000);
        lv_label_set_text_fmt(app->value, "%02u:%02u", seconds / 60, seconds % 60);
        sw_ring_set(app->ring, (uint8_t)(timer->remaining_ms * 100 / timer->total_ms));
        lv_label_set_text(app->status, timer->done ? copy->done : timer->running ? copy->running :
                          timer->remaining_ms == timer->total_ms ? copy->ready : copy->paused);
        if (app->page == PAGE_FOCUS)
            lv_label_set_text_fmt(app->detail, copy->focus_detail, model->resting ? copy->rest : copy->focus, model->completed_focus);
        else lv_label_set_text(app->detail, copy->countdown_detail);
    } else if (app->page == PAGE_STOPWATCH) {
        uint64_t seconds = model->stopwatch_ms / 1000;
        lv_label_set_text_fmt(app->value, "%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64,
                             seconds / 3600, seconds / 60 % 60, seconds % 60);
        lv_label_set_text_fmt(app->status, ".%02u  ·  %s", (unsigned)(model->stopwatch_ms / 10 % 100),
                             model->stopwatch_running ? copy->running : copy->paused);
        if (model->laps)
            lv_label_set_text_fmt(app->detail, copy->lap_detail,
                                 model->laps, model->lap_ms / 60000, model->lap_ms / 1000 % 60,
                                 (unsigned)(model->lap_ms / 10 % 100));
        else lv_label_set_text(app->detail, copy->lap_hint);
    } else {
        unsigned seconds = (unsigned)(model->clock_ms / 1000);
        if (model->clock_set) {
            lv_label_set_text_fmt(app->value, "%02u:%02u", seconds / 3600, seconds / 60 % 60);
            lv_label_set_text_fmt(app->status, copy->seconds, seconds % 60);
        } else {
            lv_label_set_text(app->value, "--:--");
            lv_label_set_text(app->status, copy->clock_unset);
        }
        lv_label_set_text(app->detail, copy->clock_detail);
    }
    lv_obj_align(app->value, LV_ALIGN_TOP_MID, 0, app->page == PAGE_STOPWATCH ? 146 : 127);
    lv_obj_align(app->detail, LV_ALIGN_TOP_MID, 0, 104);
    lv_obj_align(app->status, LV_ALIGN_TOP_MID, 0, 211);
}

static void render_editor(time_app_t *app)
{
    bool clock = app->page == PAGE_CLOCK;
    uint64_t value_ms = clock ? app->model.clock_ms : app->model.countdown.total_ms;
    char hours[320] = "", minutes[200] = "", item[8];
    for (unsigned i = 0; i < (clock ? 24u : 100u); ++i) {
        snprintf(item, sizeof(item), "%s%02u", i ? "\n" : "", i);
        strcat(hours, item);
    }
    for (unsigned i = 0; i < 60; ++i) {
        snprintf(item, sizeof(item), "%s%02u", i ? "\n" : "", i);
        strcat(minutes, item);
    }
    text(app->content, clock ? app->copy->clock_columns : app->copy->duration_columns, SW_FONT_BODY, SW_TONE_MUTED, 103);
    app->hours = sw_roller(app->content, hours, (uint16_t)(value_ms / (clock ? 3600000 : 60000)), 3);
    app->minutes = sw_roller(app->content, minutes, (uint16_t)(value_ms / (clock ? 60000 : 1000) % 60), 3);
    lv_obj_set_width(app->hours, 88);
    lv_obj_set_width(app->minutes, 88);
    lv_obj_set_pos(app->hours, 80, 132);
    lv_obj_set_pos(app->minutes, 192, 132);
    button(app, app->copy->cancel, SW_ICON_NONE, ACT_CANCEL, 62, 252, 112, 48, false);
    button(app, app->copy->save, SW_ICON_CHECK, ACT_SAVE, 186, 252, 112, 48, true);
}

static void render(time_app_t *app)
{
    lv_obj_clean(app->content);
    fprintf(stderr, "[time-lab] render=%u editing=%d locale=%s\n", app->page, app->editing,
            app->locale == TIME_LOCALE_EN ? "en" : "zh");
    app->ring = NULL;
    if (!app->editing && (app->page == PAGE_FOCUS || app->page == PAGE_COUNTDOWN)) {
        time_countdown_t *timer = app->page == PAGE_FOCUS ? &app->model.focus : &app->model.countdown;
        app->ring = sw_ring(app->content, 344, (uint8_t)(timer->remaining_ms * 100 / timer->total_ms), tones[app->page]);
        lv_obj_set_style_arc_width(app->ring, 6, LV_PART_MAIN);
        lv_obj_set_style_arc_width(app->ring, 6, LV_PART_INDICATOR);
        lv_obj_center(app->ring);
        lv_obj_clear_flag(app->ring, LV_OBJ_FLAG_CLICKABLE);
    }
    if (app->gallery_navigation) {
        button(app, NULL, SW_ICON_LEFT, ACT_LEFT, 64, 64, 40, 40, false);
        button(app, NULL, SW_ICON_RIGHT, ACT_RIGHT, 256, 64, 40, 40, false);
    }
    const time_copy_t *copy = app->copy;
    const char *title = app->editing ? (app->page == PAGE_CLOCK ? copy->set_time : copy->set_duration) : copy->titles[app->page];
    lv_obj_t *heading = text(app->content, title, SW_FONT_HEADLINE, tones[app->page], 73);
    lv_obj_set_width(heading, 148);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(heading, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_ext_click_area(heading, 2);
    sw_set_action(heading, ACT_LANGUAGE);
    if (app->editing) { render_editor(app); return; }
    app->detail = text(app->content, "", SW_FONT_CAPTION, SW_TONE_MUTED, 104);
    app->value = text(app->content, "", app->page == PAGE_STOPWATCH ? SW_FONT_NUM_M : SW_FONT_NUM_L, SW_TONE_TEXT, 127);
    app->status = text(app->content, "", SW_FONT_BODY, tones[app->page], 211);
    if (app->page == PAGE_CLOCK) {
        button(app, copy->set_time, SW_ICON_CLOCK, ACT_EDIT, 96, 252, 168, 48, true);
    } else {
        bool running = app->page == PAGE_STOPWATCH ? app->model.stopwatch_running :
                       app->page == PAGE_FOCUS ? app->model.focus.running : app->model.countdown.running;
        bool done = app->page == PAGE_FOCUS && app->model.focus.done;
        const char *primary = done ? (app->model.resting ? copy->focus : copy->rest) : running ? copy->pause : copy->start;
        button(app, running && app->page == PAGE_STOPWATCH ? copy->lap : copy->reset, SW_ICON_NONE,
               ACT_SECONDARY, 62, 236, 112, 40, false);
        button(app, primary, running ? SW_ICON_PAUSE : SW_ICON_PLAY, ACT_PRIMARY, 186, 236, 112, 40, true);
        if (app->page != PAGE_STOPWATCH) {
            for (int i = 0; i < 3; ++i) {
                lv_obj_t *preset = button(app, app->page == PAGE_FOCUS ? copy->focus_presets[i] : copy->countdown_presets[i], SW_ICON_NONE,
                                         app->page == PAGE_COUNTDOWN && i == 2 ? ACT_EDIT : ACT_PRESET_0 + i,
                                         96 + 58 * i, 284, 52, 28, false);
                lv_obj_set_style_text_font(lv_obj_get_child(preset, 0), sw_font(SW_FONT_CAPTION), 0);
                if (running) lv_obj_add_state(preset, LV_STATE_DISABLED);
            }
        }
    }
    if (app->gallery_navigation && (app->page == PAGE_CLOCK || app->page == PAGE_STOPWATCH))
        sw_page_dots(app->content, PAGE_COUNT, (uint8_t)app->page);
    update_values(app);
    fprintf(stderr, "[time-lab] page=%u focus=%d countdown=%d stopwatch=%d\n", app->page,
            app->model.focus.running, app->model.countdown.running, app->model.stopwatch_running);
}

static void advance_app(time_app_t *app)
{
    unsigned completed = time_model_update(&app->model, app->now_ms());
    if (!completed) return;
    if (completed & 1) sw_toast(app->notices, SW_ICON_CHECK, SW_TONE_ORANGE,
                               app->model.resting ? app->copy->rest_complete : app->copy->focus_complete, 6000);
    if (completed & 2) sw_toast(app->notices, SW_ICON_BELL, SW_TONE_TEAL, app->copy->countdown_complete, 6000);
    app->dirty = true;
    fprintf(stderr, "[time-lab] completed=%u\n", completed);
}

static void clicked(lv_event_t *event)
{
    time_app_t *app = lv_event_get_user_data(event);
    int action = sw_event_action(event);
    if (action < ACT_LEFT || action > ACT_LANGUAGE) return;
    advance_app(app);
    time_model_t *m = &app->model;
    time_countdown_t *timer = app->page == PAGE_FOCUS ? &m->focus : &m->countdown;
    if (action == ACT_LANGUAGE) {
        app->locale = app->locale == TIME_LOCALE_EN ? TIME_LOCALE_ZH : TIME_LOCALE_EN;
        app->copy = app->locale == TIME_LOCALE_EN ? &time_copy_en : &time_copy_zh;
        lv_obj_clean(app->notices);
        /* Cancel only unsaved picker edits; active timers and the saved clock retain their values. */
        app->editing = false;
    } else if (action == ACT_LEFT || action == ACT_RIGHT) {
        app->page = (app->page + (action == ACT_RIGHT ? 1 : PAGE_COUNT - 1)) % PAGE_COUNT;
        app->editing = false;
    } else if (action == ACT_EDIT) app->editing = true;
    else if (action == ACT_CANCEL) app->editing = false;
    else if (action == ACT_SAVE) {
        uint64_t value = (uint64_t)lv_roller_get_selected(app->hours) * 60 + lv_roller_get_selected(app->minutes);
        if (app->page == PAGE_CLOCK) { m->clock_ms = value * 60000; m->clock_set = true; }
        else if (value) time_countdown_reset(&m->countdown, value * 1000);
        else {
            sw_toast(app->notices, SW_ICON_WARNING, SW_TONE_ORANGE, app->copy->invalid_duration, 2000);
            return;
        }
        app->editing = false;
    } else if (action == ACT_PRIMARY) {
        if (app->page == PAGE_STOPWATCH) m->stopwatch_running = !m->stopwatch_running;
        else if (app->page == PAGE_FOCUS && timer->done) {
            m->resting = !m->resting;
            time_countdown_reset(timer, m->resting ? 5 * 60000 : m->focus_duration_ms);
            time_countdown_toggle(timer);
        } else time_countdown_toggle(timer);
    } else if (action == ACT_SECONDARY) {
        if (app->page == PAGE_STOPWATCH) {
            if (m->stopwatch_running) { m->lap_ms = m->stopwatch_ms; ++m->laps; }
            else { m->stopwatch_ms = m->lap_ms = 0; m->laps = 0; }
        } else time_countdown_reset(timer, timer->total_ms);
    } else if (action >= ACT_PRESET_0 && action <= ACT_PRESET_2 && !timer->running) {
        static const unsigned focus_ms[] = {25 * 60000, 45 * 60000, 5 * 60000};
        static const unsigned countdown_ms[] = {10000, 5 * 60000, 15 * 60000};
        unsigned index = (unsigned)(action - ACT_PRESET_0);
        if (app->page == PAGE_FOCUS) {
            m->resting = index == 2;
            if (!m->resting) m->focus_duration_ms = focus_ms[index];
        }
        time_countdown_reset(timer, app->page == PAGE_FOCUS ? focus_ms[index] : countdown_ms[index]);
    }
    fprintf(stderr, "[time-lab] action=%d page=%u clock_set=%d\n", action, app->page, m->clock_set);
    app->dirty = true;
}

static void tick(lv_timer_t *timer)
{
    time_app_t *app = timer->user_data;
    advance_app(app);
    if (app->dirty) {
        app->dirty = false;
        render(app);
    } else update_values(app);
}

static void deleted(lv_event_t *event)
{
    time_app_t *app = lv_event_get_user_data(event);
    if (lv_event_get_target(event) != app->screen) return;
    lv_timer_del(app->ticker);
    lv_mem_free(app);
}

lv_obj_t *time_app_open(time_app_config_t config)
{
    time_app_t *app = lv_mem_alloc(sizeof(*app));
    LV_ASSERT_MALLOC(app);
    memset(app, 0, sizeof(*app));
    app->now_ms = config.now_ms;
    app->locale = config.locale;
    app->copy = config.locale == TIME_LOCALE_EN ? &time_copy_en : &time_copy_zh;
    app->page = (unsigned)config.tool;
    app->gallery_navigation = config.gallery_navigation;
    time_model_init(&app->model, config.now_ms());
    sw_init();
    app->screen = sw_screen();
    app->content = sw_column(app->screen);
    lv_obj_set_layout(app->content, 0);
    lv_obj_set_size(app->content, 360, 360);
    lv_obj_add_flag(app->content, LV_OBJ_FLAG_EVENT_BUBBLE);
    app->notices = sw_column(app->screen);
    lv_obj_set_layout(app->notices, 0);
    lv_obj_set_size(app->notices, 360, 360);
    lv_obj_add_event_cb(app->screen, clicked, LV_EVENT_CLICKED, app);
    app->ticker = lv_timer_create(tick, 50, app);
    lv_obj_add_event_cb(app->screen, deleted, LV_EVENT_DELETE, app);
    render(app);
    fprintf(stderr, "[time-lab] Sugarat Time ready; RAM-only; manual clock\n");
    return app->screen;
}
