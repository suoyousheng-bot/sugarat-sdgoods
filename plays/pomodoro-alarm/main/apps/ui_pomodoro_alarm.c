/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Pomodoro alarm — application layer for the SDGOODS 360x360 round badge.
 *
 * A focus timer that actually wakes you up: a 25 / 45 minute work session, a
 * 5 minute break, and a real bell on the speaker when a phase ends.
 *
 * The screen is drawn entirely with the Sugarat Watch component library
 * (sw_ prefix) so the layout stays inside the 240x240 circular safe rectangle.
 * The timing accounting is the Sugarat Time case model (time_model), which
 * reports phase completion so the app can ring the alarm and switch phase.
 *
 * The bell reuses the board audio service: the shared background player is
 * started for a few seconds at a raised volume and stopped again afterwards.
 * A tap anywhere silences it immediately.
 */

#include "ui_pomodoro_alarm.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdgoods_board.h" /* platform: screen / touch / app shell / audio / loop helpers */
#include "sugarat_watch.h"  /* watchOS style components (sw_) */
#include "time_copy.h"      /* Sugarat Time case copy (English) */
#include "time_model.h"     /* Sugarat Time case timer accounting */

static const char *TAG = "pomodoro_alarm";

enum {
    ACT_PRIMARY = 1,
    ACT_RESET,
    ACT_PRESET_WORK,
    ACT_PRESET_LONG,
    ACT_PRESET_BREAK,
};

#define WORK_DEFAULT_MS (25u * 60000u)
#define WORK_LONG_MS    (45u * 60000u)
#define BREAK_MS        (5u * 60000u)
#define ALARM_VOLUME    70
#define ALARM_MS        6000u

typedef struct {
    time_model_t model;
    lv_obj_t *screen, *content, *notices, *value, *detail, *status, *ring;
    lv_timer_t *ticker;
    bool fast_loop;
} pomodoro_alarm_t;

static pomodoro_alarm_t s_app;
static lv_obj_t *s_scr;

/* ------------------------------------------------------------------ alarm */

/* The bell runs in its own task: the board audio service blocks briefly while
 * it starts and stops the shared player, and the LVGL thread must not stall. */
static TaskHandle_t s_alarm_task;
static volatile bool s_alarm_request;
static volatile bool s_alarm_stop;
static volatile bool s_alarm_playing;

static void alarm_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!s_alarm_request) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        s_alarm_request = false;
        s_alarm_stop = false;

        const int saved_volume = sdgoods_audio_get_volume();
        sdgoods_audio_set_volume(ALARM_VOLUME);
        s_alarm_playing = true;
        sdgoods_audio_bgm_start(AUDIO_BGM_THEME_FLAPPY);

        uint32_t waited = 0;
        while (waited < ALARM_MS && !s_alarm_stop) {
            vTaskDelay(pdMS_TO_TICKS(100));
            waited += 100;
        }
        s_alarm_stop = false;

        sdgoods_audio_bgm_stop();
        sdgoods_audio_set_volume(saved_volume);
        s_alarm_playing = false;
        ESP_LOGI(TAG, "alarm finished");
    }
}

static void alarm_ring(void)
{
    if (!s_alarm_task) {
        if (xTaskCreate(alarm_task, "alarm", 3072, NULL, 4, &s_alarm_task) != pdPASS) {
            s_alarm_task = NULL;
            ESP_LOGE(TAG, "alarm task create failed");
            return;
        }
    }
    s_alarm_request = true;
}

static void alarm_silence(void)
{
    if (s_alarm_playing) {
        s_alarm_stop = true;
    }
}

/* ------------------------------------------------------------------ model */

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000;
}

static const time_copy_t *copy(void)
{
    return &time_copy_en;
}

static sw_tone_t phase_tone(const pomodoro_alarm_t *app)
{
    return app->model.resting ? SW_TONE_TEAL : SW_TONE_ORANGE;
}

/* ------------------------------------------------------------------- view */

static lv_obj_t *text(lv_obj_t *parent, const char *value, sw_font_t font, sw_tone_t tone, int y)
{
    lv_obj_t *label = sw_label(parent, value, font, tone);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static lv_obj_t *button(pomodoro_alarm_t *app, const char *label, sw_icon_t icon, int action,
                        int x, int y, int width, int height, bool filled)
{
    lv_obj_t *obj = sw_button(app->content, icon, label,
                              filled ? SW_BUTTON_FILLED : SW_BUTTON_PLAIN, phase_tone(app));
    sw_set_action(obj, action);
    lv_obj_set_style_min_width(obj, 0, 0);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_pad_hor(obj, 0, 0);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static void update_values(pomodoro_alarm_t *app)
{
    const time_model_t *model = &app->model;
    const time_countdown_t *timer = &model->focus;
    const time_copy_t *c = copy();

    if (!app->value) {
        return;
    }

    unsigned seconds = (unsigned)((timer->remaining_ms + 999) / 1000);
    lv_label_set_text_fmt(app->value, "%02u:%02u", seconds / 60, seconds % 60);
    sw_ring_set(app->ring, (uint8_t)(timer->total_ms ? timer->remaining_ms * 100 / timer->total_ms : 0));

    const char *status = timer->running ? c->running
                      : timer->remaining_ms == timer->total_ms ? c->ready
                      : c->paused;
    lv_label_set_text(app->status, s_alarm_playing ? c->done : status);
    lv_label_set_text_fmt(app->detail, c->focus_detail,
                          model->resting ? c->rest : c->focus, model->completed_focus);
}

static void render(pomodoro_alarm_t *app)
{
    const time_copy_t *c = copy();
    const sw_tone_t tone = phase_tone(app);
    const bool running = app->model.focus.running;

    lv_obj_clean(app->content);

    /* Keep two pixels of clearance from the circular panel edge. */
    const lv_coord_t ring_diameter = LV_MIN(LCD_WIDTH, LCD_HEIGHT) - 4;
    app->ring = sw_ring(app->content, ring_diameter, 100, tone);
    lv_obj_set_style_arc_width(app->ring, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(app->ring, 10, LV_PART_INDICATOR);
    lv_obj_center(app->ring);
    lv_obj_clear_flag(app->ring, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *heading = text(app->content, c->titles[0], SW_FONT_HEADLINE, tone, 66);
    lv_obj_set_width(heading, 148);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);

    app->detail = text(app->content, "", SW_FONT_CAPTION, SW_TONE_MUTED, 100);
    app->value = text(app->content, "", SW_FONT_NUM_L, SW_TONE_TEXT, 124);
    app->status = text(app->content, "", SW_FONT_BODY, tone, 212);

    button(app, c->reset, SW_ICON_NONE, ACT_RESET, 62, 238, 112, 40, false);
    button(app, running ? c->pause : c->start, running ? SW_ICON_PAUSE : SW_ICON_PLAY,
           ACT_PRIMARY, 186, 238, 112, 40, true);

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *preset = button(app, c->focus_presets[i], SW_ICON_NONE,
                                  ACT_PRESET_WORK + i, 96 + 58 * i, 284, 52, 28, false);
        lv_obj_set_style_text_font(lv_obj_get_child(preset, 0), sw_font(SW_FONT_CAPTION), 0);
        if (running) {
            lv_obj_add_state(preset, LV_STATE_DISABLED);
        }
    }

    update_values(app);
}

/* ------------------------------------------------------------------ timer */

static void advance(pomodoro_alarm_t *app)
{
    const unsigned completed = time_model_update(&app->model, now_ms());
    if (!(completed & 1)) {
        update_values(app);
        return;
    }

    /* A phase just ended: ring the bell and roll into the next one. */
    const bool was_work = !app->model.resting;
    alarm_ring();
    sw_toast(app->notices, SW_ICON_BELL, was_work ? SW_TONE_ORANGE : SW_TONE_TEAL,
             was_work ? copy()->focus_complete : copy()->rest_complete, 6000);

    app->model.resting = was_work;
    const uint64_t next_ms = app->model.resting ? BREAK_MS : app->model.focus_duration_ms;
    time_countdown_reset(&app->model.focus, next_ms);
    app->model.focus.running = true;

    ESP_LOGI(TAG, "phase complete, next=%s", app->model.resting ? "break" : "focus");
    render(app);
}

static void clicked(lv_event_t *event)
{
    pomodoro_alarm_t *app = lv_event_get_user_data(event);
    const int action = sw_event_action(event);

    if (action == SW_ACTION_NONE) {
        /* A plain tap: silence a ringing alarm. */
        alarm_silence();
        return;
    }

    alarm_silence();
    time_model_t *model = &app->model;

    switch (action) {
    case ACT_PRIMARY:
        time_countdown_toggle(&model->focus);
        break;
    case ACT_RESET:
        time_countdown_reset(&model->focus, model->resting ? BREAK_MS : model->focus_duration_ms);
        break;
    case ACT_PRESET_WORK:
        model->resting = false;
        model->focus_duration_ms = WORK_DEFAULT_MS;
        time_countdown_reset(&model->focus, WORK_DEFAULT_MS);
        break;
    case ACT_PRESET_LONG:
        model->resting = false;
        model->focus_duration_ms = WORK_LONG_MS;
        time_countdown_reset(&model->focus, WORK_LONG_MS);
        break;
    case ACT_PRESET_BREAK:
        model->resting = true;
        time_countdown_reset(&model->focus, BREAK_MS);
        break;
    default:
        return;
    }

    render(app);
}

static void tick(lv_timer_t *timer)
{
    pomodoro_alarm_t *app = timer->user_data;
    advance(app);
}

static void deleted(lv_event_t *event)
{
    pomodoro_alarm_t *app = lv_event_get_user_data(event);
    if (lv_event_get_target(event) != app->screen) {
        return;
    }
    if (app->ticker) {
        lv_timer_del(app->ticker);
        app->ticker = NULL;
    }
    if (s_scr == app->screen) {
        s_scr = NULL;
    }
    ESP_LOGI(TAG, "exit");
}

/* ------------------------------------------------------------------- app */

static void on_menu_exit(void)
{
    if (s_scr) {
        lv_obj_del(s_scr);
        s_scr = NULL;
    }
}

void ui_pomodoro_alarm_start(void)
{
    if (s_scr) {
        /* Already built: switch back instead of rebuilding (a rebuild repaints
         * the whole screen and throws away the running session). */
        lv_scr_load(s_scr);
        return;
    }

    memset(&s_app, 0, sizeof(s_app));
    time_model_init(&s_app.model, now_ms());
    s_app.model.focus_duration_ms = WORK_DEFAULT_MS;

    sw_init();
    s_app.screen = sw_screen();
    s_app.content = sw_column(s_app.screen);
    lv_obj_set_layout(s_app.content, 0);
    lv_obj_set_size(s_app.content, 360, 360);
    lv_obj_add_flag(s_app.content, LV_OBJ_FLAG_EVENT_BUBBLE);

    s_app.notices = sw_column(s_app.screen);
    lv_obj_set_layout(s_app.notices, 0);
    lv_obj_set_size(s_app.notices, 360, 360);

    lv_obj_add_event_cb(s_app.screen, clicked, LV_EVENT_CLICKED, &s_app);
    lv_obj_add_event_cb(s_app.screen, deleted, LV_EVENT_DELETE, &s_app);

    s_scr = s_app.screen;
    render(&s_app);

    s_app.ticker = lv_timer_create(tick, 50, &s_app);

    lv_scr_load(s_scr);

    /* Standard device shell. bind() must run after lv_scr_load(): it attaches the
     * gesture capture layer to the current screen. */
    sdgoods_app_shell_bind(s_scr);
    sdgoods_app_shell_set_exit_cb(on_menu_exit);

    ESP_LOGI(TAG, "start");
}

void ui_pomodoro_alarm_poll(void)
{
    if (!s_scr) {
        return; /* not in the foreground */
    }

    /* The ring keeps animating while the timer runs, so keep the main loop on
     * the interactive 2 ms heartbeat for smooth frames. It may only be toggled
     * from the LVGL task, which is where apps_poll() runs. */
    if (!s_app.fast_loop) {
        sdgoods_lvgl_fast_loop(true);
        s_app.fast_loop = true;
    }
}
