/** Sugarat Beat uses a fixed viewport and draws positions from the speaker clock. */
#include "ui_helloapp.h"

#include <string.h>

#include "beat_audio.h"
#include "beat_level.h"
#include "beat_view.h"
#include "esp_log.h"
#include "sdgoods_board.h"
#include "sdgoods_gesture.h"
#include "sugarat_game.h"

enum { FRAME_MS = 24, PLAY_TOUCH_MS = 10 };
typedef enum { READY, RUNNING, LOST, WON, AUDIO_FAULT } screen_mode_t;

static lv_obj_t *screen;
static lv_obj_t *prompt;
static lv_obj_t *hint;
static lv_obj_t *pulse;
static beat_view_t view;
static sdgoods_tap_ctx_t taps;
static beat_level_t level;
static uint32_t request;
static uint32_t position_ms;
static uint32_t last_draw;
static screen_mode_t mode;
static bool paused;
static bool gameplay_press;

static lv_obj_t *hud_label(const char *text, const lv_font_t *font, int y, int width)
{
    lv_obj_t *label = sg_label(screen, text, SG_FONT_CAPTION, SG_TONE_TEXT);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xf2f2f2), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    if (width > 0) {
        lv_obj_set_width(label, width);
        lv_obj_set_style_radius(label, 7, 0);
        lv_obj_set_style_bg_color(label, lv_color_hex(0x191919), 0);
        lv_obj_set_style_bg_opa(label, LV_OPA_90, 0);
        lv_obj_set_style_pad_ver(label, 4, 0);
    } else {
        lv_obj_set_style_text_color(label, lv_color_hex(0x202020), 0);
        lv_obj_set_style_text_letter_space(label, 2, 0);
    }
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static void hint_text(const char *text, bool active)
{
    if (strcmp(lv_label_get_text(hint), text) != 0) {
        lv_label_set_text(hint, text);
        lv_obj_set_style_text_color(hint, lv_color_hex(active ? 0xa1ff55 : 0xf2f2f2), 0);
    }
}

static void touch_period(uint32_t ms)
{
    lv_indev_t *pointer = lv_indev_get_next(NULL);
    if (pointer && pointer->driver->type == LV_INDEV_TYPE_POINTER && pointer->driver->read_timer
        && pointer->driver->read_timer->period != ms) {
        lv_timer_set_period(pointer->driver->read_timer, ms);
    }
}

static void pacing(bool playing)
{
    lv_timer_set_period(_lv_disp_get_refr_timer(lv_disp_get_default()),
                        playing ? 20 : LV_DISP_DEF_REFR_PERIOD);
    touch_period(playing ? PLAY_TOUCH_MS : LV_INDEV_DEF_READ_PERIOD);
    sdgoods_lvgl_fast_loop(playing);
}

static void show_result(screen_mode_t next, const char *text)
{
    mode = next;
    if (next != WON) {
        beat_audio_stop();
    }
    pacing(false);
    lv_label_set_text(prompt, text);
    hint_text("Tap to retry", false);
    lv_label_set_text(pulse, "");
}

static void update_game(void)
{
    if (mode != RUNNING || paused) {
        return;
    }
    uint32_t ms = position_ms;
    const beat_audio_state_t audio = beat_audio_position(request, &ms);
    if (audio == BEAT_AUDIO_ERROR) {
        show_result(AUDIO_FAULT, "Audio stopped");
        return;
    }
    if (audio == BEAT_AUDIO_WAITING || ms < position_ms) {
        return;
    }
    position_ms = ms;
    beat_level_advance(&level, ms);
    if (level.result == BEAT_MISSED) {
        beat_view_draw(&view, &level, level.launch_ms, false, BEAT_NO_CUE);
        show_result(LOST, "Missed the beat");
    } else if (level.result == BEAT_CLEARED) {
        beat_view_draw(&view, &level, level.launch_ms, false, BEAT_NO_CUE);
        show_result(WON, "Circuit clear!");
    }
}

static void start_run(void)
{
    beat_level_reset(&level);
    mode = RUNNING;
    position_ms = 0;
    last_draw = UINT32_MAX;
    lv_label_set_text(prompt, "Get ready");
    lv_label_set_text(pulse, "2");
    hint_text("Tap on cue", false);
    pacing(true);
    beat_view_draw(&view, &level, 0, false, BEAT_NO_CUE);
    request = beat_audio_start(0);
}

static void tap_menu(void *data)
{
    (void)data;
    if (!paused && mode != RUNNING && !gameplay_press) {
        start_run();
    }
}

static void press_game(lv_event_t *event)
{
    (void)event;
    gameplay_press = mode == RUNNING;
    if (!gameplay_press || paused) {
        return;
    }
    update_game();
    if (mode != RUNNING) {
        return;
    }
    const beat_tap_t tap = beat_level_tap(&level, position_ms);
    if (tap == BEAT_TAP_IGNORED) {
        return;
    }
    const bool queued = tap == BEAT_TAP_QUEUED;
    beat_audio_queue_chirp(request, queued ? beat_level_next_launch(&level) : position_ms);
    beat_view_draw(&view, &level, position_ms, false, BEAT_NO_CUE);
    hint_text(queued ? "Jump queued" : "Jump!", true);
}

static void pause_game(void)
{
    update_game();
    paused = true;
    beat_audio_stop();
    pacing(false);
}

static void resume_game(void)
{
    paused = false;
    if (mode == RUNNING) {
        pacing(true);
        request = beat_audio_start(position_ms);
        if (level.tap_queued) {
            beat_audio_queue_chirp(request, beat_level_next_launch(&level));
        }
    }
}

static void exit_game(void)
{
    pause_game();
    screen = NULL;
}

void ui_helloapp_start(void)
{
    if (screen) {
        lv_scr_load(screen);
        return;
    }
    mode = READY;
    paused = false;
    gameplay_press = false;
    beat_level_reset(&level);
    sg_init();
    screen = sg_screen();
    beat_view_create(&view, screen);
    prompt = hud_label("Sugarat Beat", &lv_font_montserrat_24, 64, 220);
    pulse = hud_label("KINETIC CIRCUIT", &lv_font_montserrat_14, 110, 0);
    hint = hud_label("Tap to play", &lv_font_montserrat_14, 272, 142);
    beat_view_draw(&view, &level, 0, true, BEAT_NO_CUE);
    lv_scr_load(screen);
    sdgoods_app_shell_bind(screen);
    sdgoods_app_shell_set_pause_cb(pause_game);
    sdgoods_app_shell_set_resume_cb(resume_game);
    sdgoods_app_shell_set_exit_cb(exit_game);
    sdgoods_app_on_tap(screen, &taps, tap_menu, NULL);
    lv_obj_add_event_cb(screen, press_game, LV_EVENT_PRESSED, NULL);
    ESP_LOGI("sugarat_beat", "Ready");
}

void ui_helloapp_poll(void)
{
    if (!screen || paused || mode != RUNNING) {
        return;
    }
    /* The board returns to its idle sampling period when the previous touch boost expires. */
    touch_period(PLAY_TOUCH_MS);
    update_game();
    if (mode != RUNNING || (last_draw != UINT32_MAX && position_ms - last_draw < FRAME_MS)) {
        return;
    }
    const uint32_t ms = position_ms;
    if (last_draw == UINT32_MAX || ms / BEAT_MS != last_draw / BEAT_MS) {
        if (ms < COUNT_IN_MS) {
            lv_label_set_text(pulse, ms < BEAT_MS ? "2" : "1");
        } else {
            lv_label_set_text(prompt, "Sugarat Beat");
            lv_label_set_text(pulse, ((ms - COUNT_IN_MS) / BEAT_MS) % 2 ? ". O . O" : "O . O .");
        }
    }
    const int cue = beat_level_cue_pad(&level, ms);
    const bool cue_visible = cue != BEAT_NO_CUE;
    const bool jumping = level.to - level.from == 2 && ms - level.launch_ms < 170;
    const char *instruction = level.tap_queued ? "Jump queued" : cue_visible ? "TAP NOW"
        : jumping ? "Jump!" : "Tap on cue";
    hint_text(instruction, cue_visible || level.tap_queued || jumping);
    beat_view_draw(&view, &level, ms, false, cue);
    last_draw = ms;
}
