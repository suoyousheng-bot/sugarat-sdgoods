/** Real LVGL pointer events and application screens with a scripted speaker clock. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "beat_audio.h"
#include "beat_level.h"
#include "sdgoods_board.h"
#include "sdgoods_gesture.h"
#include "sdgoods_input.h"
#include "ui_helloapp.h"

static void (*pause_callback)(void);
static void (*resume_callback)(void);
static uint32_t clock_ms;
static uint32_t audio_id;
static uint32_t offset;
static unsigned chirps;
static uint32_t chirp_at;
static bool failed;
static bool playing;
static bool fast_loop;
static bool pressed;
enum { SCREEN_SIDE = 360 };
static lv_color_t framebuffer[SCREEN_SIDE * SCREEN_SIDE];

void sdgoods_app_shell_bind(lv_obj_t *screen) { assert(screen == lv_scr_act()); }
void sdgoods_app_shell_set_pause_cb(void (*cb)(void)) { pause_callback = cb; }
void sdgoods_app_shell_set_resume_cb(void (*cb)(void)) { resume_callback = cb; }
void sdgoods_app_shell_set_exit_cb(void (*cb)(void)) { assert(cb); }
void sdgoods_lvgl_fast_loop(bool value) { fast_loop = value; }
void sdgoods_app_on_tap(lv_obj_t *screen, sdgoods_tap_ctx_t *ctx, sdgoods_tap_cb_t cb, void *data)
{
    sdgoods_tap_bind(screen, ctx, cb, data);
}
bool sdgoods_lvgl_post(sdgoods_lvgl_cb_t cb, void *arg)
{
    (void)cb;
    (void)arg;
    assert(false);
    return false;
}
void sdgoods_touch_synth_report(lv_indev_state_t state, lv_coord_t x, lv_coord_t y)
{
    (void)state;
    (void)x;
    (void)y;
}
int64_t esp_timer_get_time(void) { return (int64_t)lv_tick_get() * 1000; }

uint32_t beat_audio_start(uint32_t ms)
{
    offset = clock_ms = ms;
    playing = true;
    return ++audio_id;
}
void beat_audio_queue_chirp(uint32_t id, uint32_t ms)
{
    assert(playing && id == audio_id);
    chirp_at = ms;
    ++chirps;
}
void beat_audio_stop(void) { playing = false; }
beat_audio_state_t beat_audio_position(uint32_t id, uint32_t *ms)
{
    assert(playing && id == audio_id);
    *ms = clock_ms;
    return failed ? BEAT_AUDIO_ERROR : BEAT_AUDIO_PLAYING;
}

static void pointer(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void)driver;
    data->point = (lv_point_t){180, 181};
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    const int width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y) {
        memcpy(&framebuffer[y * SCREEN_SIDE + area->x1], pixels, (size_t)width * sizeof(*pixels));
        pixels += width;
    }
    lv_disp_flush_ready(driver);
}
static void pump(void)
{
    lv_tick_inc(32);
    lv_timer_handler();
    ui_helloapp_poll();
}
static void tap(void)
{
    pressed = true;
    pump();
    pressed = false;
    pump();
}

static lv_obj_t *find_label(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) && strcmp(lv_label_get_text(obj), text) == 0) {
        return obj;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) {
        lv_obj_t *match = find_label(lv_obj_get_child(obj, i), text);
        if (match) {
            return match;
        }
    }
    return NULL;
}

static void expect_label(const char *text)
{
    lv_obj_update_layout(lv_scr_act());
    lv_obj_t *label = find_label(lv_scr_act(), text);
    assert(label);
    lv_area_t area;
    lv_obj_get_coords(label, &area);
    assert(area.x1 >= 60 && area.x2 < 300 && area.y1 >= 60 && area.y2 < 300);
}

static void capture(const char *directory, const char *name)
{
    if (!directory) {
        return;
    }
    lv_refr_now(NULL);
    char path[1024];
    const int length = snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    assert(length > 0 && (size_t)length < sizeof(path));
    FILE *out = fopen(path, "wb");
    assert(out);
    fprintf(out, "P6\n%d %d\n255\n", SCREEN_SIDE, SCREEN_SIDE);
    for (int y = 0; y < SCREEN_SIDE; ++y) {
        for (int x = 0; x < SCREEN_SIDE; ++x) {
            const int dx = 2 * x + 1 - SCREEN_SIDE;
            const int dy = 2 * y + 1 - SCREEN_SIDE;
            const lv_color_t pixel = framebuffer[y * SCREEN_SIDE + x];
            const uint32_t rgb = lv_color_to32(pixel);
            const bool on_panel = dx * dx + dy * dy <= SCREEN_SIDE * SCREEN_SIDE;
            fputc(on_panel ? (rgb >> 16) & 255 : 16, out);
            fputc(on_panel ? (rgb >> 8) & 255 : 16, out);
            fputc(on_panel ? rgb & 255 : 16, out);
        }
    }
    fclose(out);
}

int main(int argc, char **argv)
{
    assert(argc <= 2);
    const char *preview_dir = argc == 2 ? argv[1] : NULL;
    lv_init();
    static lv_color_t pixels[360 * 12];
    lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, 360 * 12);
    lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = display.ver_res = 360;
    display.draw_buf = &buffer;
    display.flush_cb = flush;
    assert(lv_disp_drv_register(&display));
    lv_indev_drv_t input;
    lv_indev_drv_init(&input);
    input.type = LV_INDEV_TYPE_POINTER;
    input.read_cb = pointer;
    assert(lv_indev_drv_register(&input));

    ui_helloapp_start();
    pump();
    expect_label("Tap to play");
    capture(preview_dir, "ready");
    tap();
    assert(fast_loop && playing && audio_id == 1);
    expect_label("Get ready");
    clock_ms = 500;
    pump();
    expect_label("1");

    clock_ms = 2896;
    pump();
    expect_label("TAP NOW");
    capture(preview_dir, "cue");
    tap();
    expect_label("Jump queued");
    assert(chirps == 1 && chirp_at == 3000);
    tap();
    assert(chirps == 1);
    clock_ms = 3128;
    pump();
    capture(preview_dir, "jump");
    clock_ms = 3504;
    pump();
    assert(playing && !find_label(lv_scr_act(), "Missed the beat"));

    pause_callback();
    assert(!fast_loop && !playing);
    pump();
    resume_callback();
    assert(fast_loop && playing && offset == 3504 && audio_id == 2);
    clock_ms = 5520;
    pump();
    expect_label("Missed the beat");
    capture(preview_dir, "missed");
    assert(!playing && !fast_loop);
    tap();
    assert(playing && audio_id == 3 && offset == 0);
    expect_label("Get ready");

    failed = true;
    pump();
    expect_label("Audio stopped");
    expect_label("Tap to retry");
    assert(!playing && !fast_loop);
    failed = false;
    tap();
    assert(playing && offset == 0);
    for (unsigned tile = 0; tile < FINISH_TILE; ++tile) {
        if (beat_level_hazard(tile + 1)) {
            const uint32_t landing = COUNT_IN_MS + tile * BEAT_MS;
            const bool late = tile % 2 == 0;
            clock_ms = (late ? landing + 64 : landing - 104) / 8 * 8;
            pump();
            expect_label("TAP NOW");
            tap();
            expect_label(late ? "Jump!" : "Jump queued");
            assert(chirp_at == (late ? clock_ms : landing));
        }
    }
    clock_ms = 29504;
    pump();
    expect_label("Circuit clear!");
    expect_label("Tap to retry");
    assert(chirps == 19);
    puts("Early and late touch, timed chirps, all 18 skips, clear, pause/resume, miss, retry and audio error verified through LVGL.");
}
