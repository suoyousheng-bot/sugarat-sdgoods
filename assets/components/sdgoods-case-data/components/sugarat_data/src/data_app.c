/* SPDX-License-Identifier: Apache-2.0 */
/** Data scenes reuse Watch controls; providers publish decoded readings on the LVGL thread. */
#include "sugarat_data.h"
#include "sugarat_watch.h"
#include "data_copy.h"
#include <stdio.h>
#include <stdlib.h>

enum { ACT_PREV = 1, ACT_NEXT, ACT_LANGUAGE, ACT_ACK };
typedef struct {
    sd_config_t config;
    sd_model_t model;
    const sd_copy_t *copy;
    lv_obj_t *screen, *content, *value, *status, *condition, *ring, *chart, *humidity, *battery, *ack;
    lv_timer_t *timer;
    bool dirty, paused;
} data_app_t;

static void temperature(char *buffer, size_t size, int16_t value)
{
    int number = value;
    snprintf(buffer, size, "%s%d.%d", number < 0 ? "-" : "", abs(number) / 10, abs(number) % 10);
}

static lv_obj_t *text(data_app_t *app, const char *value, sw_font_t font, sw_tone_t tone, int y)
{
    lv_obj_t *label = sw_label(app->content, value, font, tone);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static lv_obj_t *button(data_app_t *app, const char *value, int action, int x, int y, int w, int h)
{
    lv_obj_t *obj = sw_button(app->content, SW_ICON_NONE, NULL, SW_BUTTON_PLAIN, SW_TONE_TEAL);
    lv_obj_set_style_min_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    sw_set_action(obj, action);
    lv_obj_center(sw_label(obj, value, SW_FONT_BODY, SW_TONE_TEXT));
    return obj;
}

static void refresh(data_app_t *app)
{
    sd_model_t *m = &app->model;
    const sd_copy_t *c = app->copy;
    bool live = m->status == SD_LIVE;
    sw_tone_t tone = m->alarm_active ? SW_TONE_ORANGE : SW_TONE_TEAL;
    char buffer[48];
    if (live) temperature(buffer, sizeof(buffer), m->sample.temperature_tenths);
    else snprintf(buffer, sizeof(buffer), "—");
    lv_label_set_text(app->value, buffer);
    lv_obj_align(app->value, LV_ALIGN_TOP_MID, 0,
                 app->config.scene == SD_SCENE_OVERVIEW || app->config.scene == SD_SCENE_HISTORY ? 104 : 142);
    lv_label_set_text(app->status, c->statuses[m->status]);
    lv_obj_set_style_text_color(app->status, sw_color(live ? tone : SW_TONE_ORANGE), 0);
    lv_obj_align(app->status, LV_ALIGN_TOP_MID, 0, app->config.scene == SD_SCENE_ALARM ? 104 : 280);
    if (app->ring) {
        int32_t value = m->sample.temperature_tenths - m->limits.minimum_tenths;
        uint8_t percent = live ? (uint8_t)(value * 100 / (m->limits.maximum_tenths - m->limits.minimum_tenths)) : 0;
        sw_ring_set(app->ring, percent);
        lv_obj_set_style_arc_color(app->ring, sw_color(tone), LV_PART_INDICATOR);
    }
    if (app->humidity) {
        if (live) snprintf(buffer, sizeof(buffer), "%u%%", m->sample.humidity_percent);
        else snprintf(buffer, sizeof(buffer), "—");
        sw_tile_set(app->humidity, buffer);
        if (live) snprintf(buffer, sizeof(buffer), "%u%%", m->sample.battery_percent);
        else snprintf(buffer, sizeof(buffer), "—");
        sw_tile_set(app->battery, buffer);
    }
    if (app->chart) {
        lv_chart_series_t *series = lv_chart_get_series_next(app->chart, NULL);
        for (uint8_t i = 0; i < SD_HISTORY_CAPACITY; ++i) {
            sd_point_t point = {0};
            bool present = i >= SD_HISTORY_CAPACITY - m->count;
            if (present) point = sd_model_point(m, i - (SD_HISTORY_CAPACITY - m->count));
            lv_chart_set_value_by_id(app->chart, series, i, present && point.valid ? point.value : LV_CHART_POINT_NONE);
        }
        bool has_history = false;
        for (uint8_t i = 0; i < m->count; ++i) if (sd_model_point(m, i).valid) has_history = true;
        lv_label_set_text(app->condition, has_history ? c->history : c->empty);
        lv_obj_align(app->condition, LV_ALIGN_TOP_MID, 0, 251);
        lv_chart_refresh(app->chart);
    }
    if (app->ack) {
        const char *condition = m->alarm_active ? (m->acknowledged ? c->acknowledged : c->alarm) :
                                live ? c->normal : c->waiting;
        lv_label_set_text(app->condition, condition);
        lv_obj_align(app->condition, LV_ALIGN_TOP_MID, 0, 131);
        lv_obj_set_style_text_color(app->condition, sw_color(tone), 0);
        if (m->alarm_active && !m->acknowledged) lv_obj_clear_state(app->ack, LV_STATE_DISABLED);
        else lv_obj_add_state(app->ack, LV_STATE_DISABLED);
    }
}

static void render(data_app_t *app)
{
    lv_obj_clean(app->content);
    app->ring = app->chart = app->humidity = app->battery = app->ack = app->condition = NULL;
    const sd_copy_t *c = app->copy;
    sd_limits_t limits = app->model.limits;
    if (app->config.scene == SD_SCENE_GAUGE) {
        app->ring = sw_ring(app->content, 360, 0, SW_TONE_TEAL);
        lv_obj_set_pos(app->ring, 0, 0);
        lv_obj_set_style_arc_width(app->ring, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_width(app->ring, 10, LV_PART_INDICATOR);
    }
    if (app->config.gallery_navigation) {
        lv_obj_t *prev = button(app, "", ACT_PREV, 64, 64, 40, 40);
        lv_obj_t *next = button(app, "", ACT_NEXT, 256, 64, 40, 40);
        lv_obj_center(sw_icon(prev, SW_ICON_LEFT, SW_SIZE_M, SW_TONE_TEAL));
        lv_obj_center(sw_icon(next, SW_ICON_RIGHT, SW_SIZE_M, SW_TONE_TEAL));
    }
    button(app, c->language, ACT_LANGUAGE, 152, 34, 56, 28);
    text(app, c->scenes[app->config.scene], SW_FONT_BODY, SW_TONE_TEAL, 74);
    text(app, app->config.source == SD_SOURCE_DEMO ? c->demo : c->external, SW_FONT_CAPTION, SW_TONE_MUTED, 309);
    app->status = text(app, "", SW_FONT_CAPTION, SW_TONE_MUTED, 280);
    app->value = text(app, "—", app->config.scene == SD_SCENE_GAUGE ? SW_FONT_NUM_L : SW_FONT_NUM_M, SW_TONE_TEXT, 142);
    char minimum[16], maximum[16], buffer[80];
    temperature(minimum, sizeof(minimum), limits.minimum_tenths);
    temperature(maximum, sizeof(maximum), limits.maximum_tenths);
    switch (app->config.scene) {
    case SD_SCENE_GAUGE:
        text(app, "°C", SW_FONT_HEADLINE, SW_TONE_MUTED, 232);
        snprintf(buffer, sizeof(buffer), "%s %s / %s °C", c->range, minimum, maximum);
        text(app, buffer, SW_FONT_CAPTION, SW_TONE_MUTED, 259);
        break;
    case SD_SCENE_OVERVIEW:
        app->humidity = sw_tile(app->content, SW_ICON_NONE, SW_TONE_TEAL, "—", c->humidity);
        app->battery = sw_tile(app->content, SW_ICON_NONE, SW_TONE_BLUE, "—", c->battery);
        lv_obj_set_size(app->humidity, 124, 108); lv_obj_set_pos(app->humidity, 50, 164);
        lv_obj_set_size(app->battery, 124, 108); lv_obj_set_pos(app->battery, 186, 164);
        text(app, "°C", SW_FONT_CAPTION, SW_TONE_MUTED, 148);
        break;
    case SD_SCENE_HISTORY: {
        int16_t initial[] = {0, 0};
        app->chart = sw_sparkline(app->content, 244, 82, initial, 2, SW_TONE_TEAL);
        lv_obj_set_pos(app->chart, 58, 166);
        lv_chart_set_point_count(app->chart, SD_HISTORY_CAPACITY);
        lv_chart_set_range(app->chart, LV_CHART_AXIS_PRIMARY_Y, limits.minimum_tenths, limits.maximum_tenths);
        lv_chart_set_div_line_count(app->chart, 3, 0);
        lv_obj_set_style_line_color(app->chart, sw_color(SW_TONE_RAISED), LV_PART_MAIN);
        lv_obj_set_style_size(app->chart, 3, LV_PART_INDICATOR);
        app->condition = text(app, c->empty, SW_FONT_CAPTION, SW_TONE_MUTED, 251);
        text(app, "°C", SW_FONT_CAPTION, SW_TONE_MUTED, 148);
        break;
    }
    case SD_SCENE_ALARM:
        app->condition = text(app, "", SW_FONT_BODY, SW_TONE_ORANGE, 131);
        temperature(minimum, sizeof(minimum), limits.alarm_tenths);
        temperature(maximum, sizeof(maximum), limits.alarm_tenths - limits.hysteresis_tenths);
        snprintf(buffer, sizeof(buffer), "%s ≥ %s °C", c->threshold, minimum);
        text(app, buffer, SW_FONT_BODY, SW_TONE_MUTED, 213);
        snprintf(buffer, sizeof(buffer), "%s ≤ %s °C", c->recovery, maximum);
        text(app, buffer, SW_FONT_CAPTION, SW_TONE_MUTED, 240);
        app->ack = button(app, c->acknowledge, ACT_ACK, 100, 266, 160, 36);
        lv_obj_set_style_translate_y(app->value, 24, 0);
        break;
    }
    refresh(app);
    app->dirty = false;
}

static void clicked(lv_event_t *event)
{
    data_app_t *app = lv_event_get_user_data(event);
    if (app->paused) return;
    int action = sw_event_action(event);
    switch (action) {
    case ACT_PREV: case ACT_NEXT:
        app->config.scene = (sd_scene_t)((app->config.scene + (action == ACT_NEXT ? 1 : 3)) % 4);
        app->dirty = true;
        break;
    case ACT_LANGUAGE:
        app->config.locale = app->config.locale == SD_LOCALE_ZH ? SD_LOCALE_EN : SD_LOCALE_ZH;
        app->copy = app->config.locale == SD_LOCALE_ZH ? &sd_copy_zh : &sd_copy_en;
        app->dirty = true;
        break;
    case ACT_ACK: sd_model_acknowledge(&app->model); break;
    default: return;
    }
}

static void tick(lv_timer_t *timer)
{
    data_app_t *app = timer->user_data;
    sd_model_tick(&app->model, app->config.now_ms());
    if (app->dirty) render(app); else refresh(app);
}

static void deleted(lv_event_t *event)
{
    data_app_t *app = lv_event_get_user_data(event);
    if (lv_event_get_target(event) != app->screen) return;
    lv_timer_del(app->timer);
    lv_mem_free(app);
}

lv_obj_t *sd_open(sd_config_t config)
{
    sd_model_t model;
    if (!config.now_ms || !config.refresh_ms || config.locale > SD_LOCALE_EN || config.locale < SD_LOCALE_ZH ||
        config.scene > SD_SCENE_ALARM || config.scene < SD_SCENE_GAUGE || config.source > SD_SOURCE_EXTERNAL ||
        config.source < SD_SOURCE_DEMO || !sd_model_init(&model, config.limits)) return NULL;
    data_app_t *app = lv_mem_alloc(sizeof(*app));
    if (!app) return NULL;
    *app = (data_app_t){.config = config, .model = model,
        .copy = config.locale == SD_LOCALE_ZH ? &sd_copy_zh : &sd_copy_en};
    app->screen = sw_screen();
    lv_obj_set_user_data(app->screen, app);
    app->content = sw_column(app->screen);
    lv_obj_set_layout(app->content, 0);
    lv_obj_set_size(app->content, 360, 360);
    lv_obj_set_pos(app->content, 0, 0);
    lv_obj_add_flag(app->content, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(app->screen, clicked, LV_EVENT_CLICKED, app);
    app->timer = lv_timer_create(tick, config.refresh_ms, app);
    lv_obj_add_event_cb(app->screen, deleted, LV_EVENT_DELETE, app);
    render(app);
    return app->screen;
}

bool sd_publish(lv_obj_t *screen, sd_sample_t sample)
{
    data_app_t *app = lv_obj_get_user_data(screen);
    bool accepted = sd_model_push(&app->model, sample, app->config.now_ms());
    if (!app->paused && !app->dirty) refresh(app);
    return accepted;
}

void sd_disconnect(lv_obj_t *screen)
{
    data_app_t *app = lv_obj_get_user_data(screen);
    sd_model_disconnect(&app->model, app->config.now_ms());
    if (!app->paused && !app->dirty) refresh(app);
}

void sd_set_paused(lv_obj_t *screen, bool paused)
{
    data_app_t *app = lv_obj_get_user_data(screen);
    app->paused = paused;
    if (paused) lv_timer_pause(app->timer);
    else { lv_timer_resume(app->timer); lv_timer_ready(app->timer); }
}
