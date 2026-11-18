/* SPDX-License-Identifier: Apache-2.0 */
#include "ui_helloapp.h"
#include "esp_log.h"
#include "esp_random.h"
#include "sdgoods_board.h"
#include "sugarat_audio.h"
#include "sugarat_game.h"
#include "sugarat_imu.h"

enum { FOOD_SIZE = 160, FOOD_BYTES = FOOD_SIZE * FOOD_SIZE * 3 };
#define FOOD(asset) \
    extern const uint8_t asset##_pixels[] asm("_binary_" #asset "_bin_start"); \
    static const lv_img_dsc_t asset = { \
        .header = { .cf = LV_IMG_CF_TRUE_COLOR_ALPHA, .w = FOOD_SIZE, .h = FOOD_SIZE }, \
        .data_size = FOOD_BYTES, .data = asset##_pixels, \
    }
FOOD(burger);
FOOD(grilled_chicken_breast);
FOOD(noodle_bowl);
FOOD(pasta_plate);
FOOD(pizza_slice);
FOOD(rice_bowl);
FOOD(salad_bowl);
FOOD(sandwich);
FOOD(soup_bowl);
FOOD(steak);
#undef FOOD

static const struct {
    const char *name;
    const lv_img_dsc_t *image;
} dishes[] = {
    { "Burger", &burger }, { "Grilled Chicken", &grilled_chicken_breast },
    { "Noodle Soup", &noodle_bowl }, { "Pasta", &pasta_plate },
    { "Pizza", &pizza_slice }, { "Rice Bowl", &rice_bowl },
    { "Garden Salad", &salad_bowl }, { "Sandwich", &sandwich },
    { "Vegetable Soup", &soup_bowl }, { "Steak", &steak },
};

enum { DISH_COUNT = sizeof(dishes) / sizeof(dishes[0]) };
#include "draw_timing.h"
extern const uint8_t draw_wav_start[] asm("_binary_draw_wav_start");
extern const uint8_t draw_wav_end[] asm("_binary_draw_wav_end");
enum { FRAME_COUNT = sizeof(frame_ms) / sizeof(frame_ms[0]) };
static const char *const TAG = "lunch";
static lv_obj_t *screen;
static lv_obj_t *picture;
static lv_obj_t *caption;
static lv_obj_t *hint;
static unsigned frame;
static unsigned shown = 5;
static unsigned previous = DISH_COUNT;
static unsigned winner;
static uint32_t started_at;
static bool rolling;
static bool paused;
static bool shake_ready;

/* Only the IMU task reads or updates the pickup timestamp. */
static int64_t last_pickup_us;
enum { PICKUP_GUARD_US = 250000 };

static void start_draw(const char *input)
{
    if (paused || rolling || !screen || lv_scr_act() != screen) return;

    if (previous == DISH_COUNT) {
        winner = esp_random() % DISH_COUNT;
    } else {
        winner = esp_random() % (DISH_COUNT - 1);
        if (winner >= previous) ++winner;
    }
    frame = 0;
    rolling = true;
    started_at = lv_tick_get();
    lv_label_set_text_static(caption, "Drawing...");
    lv_obj_add_flag(hint, LV_OBJ_FLAG_HIDDEN);
    if (sdgoods_audio_get_volume() > 0) {
        size_t length = (uintptr_t)draw_wav_end - (uintptr_t)draw_wav_start;
        esp_err_t result = sugarat_audio_play_wav(draw_wav_start, length);
        if (result != ESP_OK) ESP_LOGW(TAG, "draw sound unavailable: %s", esp_err_to_name(result));
    }
    ESP_LOGI(TAG, "draw started (%s)", input);
}

static void screen_tapped(lv_event_t *event)
{
    (void)event;
    if (previous != DISH_COUNT || !shake_ready) start_draw("touch");
}

static void shake_on_ui(void *unused)
{
    (void)unused;
    start_draw("shake");
}

static void imu_event(const sugarat_imu_event_t *event, void *unused)
{
    (void)unused;
    if (event->type == SUGARAT_IMU_EVENT_PICKUP) {
        last_pickup_us = event->time_us;
        return;
    }
    if (event->type != SUGARAT_IMU_EVENT_SHAKE) return;
    if (last_pickup_us != 0 && event->time_us - last_pickup_us < PICKUP_GUARD_US) return;

    if (!sdgoods_lvgl_post(shake_on_ui, NULL)) {
        ESP_LOGW(TAG, "shake event dropped: UI queue full");
    }
}

static void pause_screen(void)
{
    paused = true;
    sugarat_audio_stop();
}
static void resume_screen(void) { paused = false; }

static lv_obj_t *center_label(const char *text, sg_font_t font, sg_tone_t tone, int y)
{
    lv_obj_t *label = sg_label(screen, text, font, tone);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

void ui_helloapp_start(void)
{
    if (screen) {
        lv_scr_load(screen);
        return;
    }

    sg_init();
    screen = sg_screen();
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, screen_tapped, LV_EVENT_SHORT_CLICKED, NULL);
    center_label("LUNCH SHUFFLE", SG_FONT_CAPTION, SG_TONE_ACCENT, 58);

    picture = lv_img_create(screen);
    lv_obj_clear_flag(picture, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(picture, dishes[shown].image);
    lv_obj_align(picture, LV_ALIGN_TOP_MID, 0, 84);
    caption = center_label("What's for lunch?", SG_FONT_TITLE, SG_TONE_TEXT, 246);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_24, 0);
    hint = center_label("Shake to draw", SG_FONT_CAPTION, SG_TONE_MUTED, 286);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);

    lv_scr_load(screen);
    sdgoods_app_shell_bind(screen);
    sdgoods_app_shell_set_pause_cb(pause_screen);
    sdgoods_app_shell_set_resume_cb(resume_screen);
    sugarat_imu_config_t imu = sugarat_imu_default_config();
    imu.shake_threshold_g = 0.55f;
    imu.shake_peaks = 4;
    imu.shake_window_ms = 700;
    imu.shake_min_peak_gap_ms = 80;
    imu.shake_max_peak_gap_ms = 250;
    imu.step_detection = false;
    imu.on_event = imu_event;
    esp_err_t result = sugarat_imu_start(&imu);
    shake_ready = result == ESP_OK;
    if (!shake_ready) {
        lv_label_set_text_static(hint, "Tap to draw");
        ESP_LOGW(TAG, "IMU unavailable: %s", esp_err_to_name(result));
    } else {
        ESP_LOGI(TAG, "shake threshold %.2f g, %u peaks in %u ms, gap %u-%u ms",
                 (double)imu.shake_threshold_g, (unsigned)imu.shake_peaks,
                 (unsigned)imu.shake_window_ms, (unsigned)imu.shake_min_peak_gap_ms,
                 (unsigned)imu.shake_max_peak_gap_ms);
    }
    ESP_LOGI(TAG, "ready with %d dishes", DISH_COUNT);
}

void ui_helloapp_poll(void)
{
    if (paused || !rolling || lv_scr_act() != screen) return;
    if (lv_tick_elaps(started_at) < frame_ms[frame]) return;

    ++frame;
    if (frame == FRAME_COUNT) {
        shown = winner;
        previous = winner;
        rolling = false;
        lv_label_set_text_static(caption, dishes[winner].name);
        lv_label_set_text_static(hint, "Tap to draw again");
        lv_obj_clear_flag(hint, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "draw result: %s", dishes[winner].name);
    } else {
        shown = (shown + 1) % DISH_COUNT;
    }
    lv_img_set_src(picture, dishes[shown].image);
}
