#include "recorder.h"
#include "ui_helloapp.h"
#include "sdgoods_board.h"
#include "sdgoods_lvgl.h"
#include "sdgoods_tap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_color_t frame[360 * 360], draw[360 * 40];
static lv_indev_data_t touch;
static int volume = 64;
static recorder_view_t view = { .state = REC_IDLE, .count = 12, .storage_ready = true };

void sdgoods_app_shell_bind(lv_obj_t *screen) { sdgoods_tap_lock(screen); }
void sdgoods_app_shell_set_pause_cb(void (*cb)(void)) { (void)cb; }
int sdgoods_audio_get_volume(void) { return volume; }
void sdgoods_audio_set_volume(int value) { volume = value; }
void sdgoods_touch_synth_report(lv_indev_state_t state, lv_coord_t x, lv_coord_t y)
{ (void)state; (void)x; (void)y; }
int64_t esp_timer_get_time(void) { return (int64_t)lv_tick_get() * 1000; }
bool sdgoods_lvgl_post(sdgoods_lvgl_cb_t cb, void *arg) { return lv_async_call(cb, arg) == LV_RES_OK; }
void recorder_poll(void) {}
recorder_view_t recorder_view(void) { return view; }
voice_clip_t recorder_clip(size_t index)
{ return index < view.count ? (voice_clip_t){(uint32_t)(view.count - index), 384000} : (voice_clip_t){0}; }
void recorder_toggle_capture(void)
{
    if (view.state == REC_IDLE) {
        view.state = REC_RECORDING;
        view.elapsed_ms = 24000;
        view.level = 78;
    } else if (view.state == REC_RECORDING || view.state == REC_UNSAVED) {
        view.state = REC_SAVING;
        view.message = NULL;
    }
}
void recorder_toggle_playback(voice_clip_t clip)
{
    if (view.state == REC_PLAYING && view.clip_id == clip.id) {
        view.state = REC_PAUSED;
    } else {
        view.clip_id = clip.id;
        view.state = REC_PLAYING;
    }
}
void recorder_stop_playback(void) { view.state = REC_IDLE; view.clip_id = 0; }
void recorder_delete(uint32_t id) { (void)id; --view.count; }
void recorder_discard(void) { view.state = REC_IDLE; }
void recorder_pause(void) {}

static void flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *pixel)
{
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x) frame[y * 360 + x] = *pixel++;
    lv_disp_flush_ready(drv);
}
static void input(lv_indev_drv_t *drv, lv_indev_data_t *data)
{ (void)drv; *data = touch; }
static void tick(int ms)
{
    for (int left = ms; left > 0; left -= 10) {
        lv_tick_inc(10);
        lv_timer_handler();
        ui_helloapp_poll();
    }
}
static void point(int x, int y, lv_indev_state_t state)
{
    touch = (lv_indev_data_t){.point = {x, y}, .state = state};
    tick(30);
}
static void tap(int x, int y)
{
    point(x, y, LV_INDEV_STATE_PRESSED);
    point(x, y, LV_INDEV_STATE_RELEASED);
    tick(100);
}
static void swipe(int x, int y, int dx, int dy)
{
    for (int i = 0; i <= 5; ++i) point(x + dx * i / 5, y + dy * i / 5, LV_INDEV_STATE_PRESSED);
    point(x + dx, y + dy, LV_INDEV_STATE_RELEASED);
    tick(600);
}
static bool has_label(lv_obj_t *obj, const char *name)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return false;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), name)) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i)
        if (has_label(lv_obj_get_child(obj, i), name)) return true;
    return false;
}
static lv_obj_t *find_arc(lv_obj_t *obj)
{
    if (lv_obj_check_type(obj, &lv_arc_class)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) {
        lv_obj_t *arc = find_arc(lv_obj_get_child(obj, i));
        if (arc) return arc;
    }
    return NULL;
}

static void dump_labels(lv_obj_t *obj)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class)) fprintf(stderr, "  %s\n", lv_label_get_text(obj));
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) dump_labels(lv_obj_get_child(obj, i));
}
static void require_label(const char *label)
{
    if (!has_label(lv_scr_act(), label)) {
        fprintf(stderr, "Missing visible label: %s\n", label);
        dump_labels(lv_scr_act());
        exit(1);
    }
}
static void save(const char *folder, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", folder, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    lv_refr_now(NULL);
    fprintf(file, "P6\n360 360\n255\n");
    for (size_t i = 0; i < 360 * 360; i++) {
        lv_color32_t color = { .full = lv_color_to32(frame[i]) };
        fputc(color.ch.red, file);
        fputc(color.ch.green, file);
        fputc(color.ch.blue, file);
    }
    fclose(file);
}

int main(int count, char **argv)
{
    assert(count == 2);
    lv_init();
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, draw, NULL, 360 * 40);
    static lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = display.ver_res = 360;
    display.draw_buf = &buffer;
    display.flush_cb = flush;
    lv_disp_drv_register(&display);
    static lv_indev_drv_t pointer;
    lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER;
    pointer.read_cb = input;
    lv_indev_drv_register(&pointer);
    ui_helloapp_start();
    tick(120);
    require_label("Start Recording");
    save(argv[1], "home");
    tap(135, 180);
    require_label("Hold to Stop & Save");
    lv_obj_t *ring = find_arc(lv_scr_act());
    assert(ring && lv_arc_get_value(ring) == 0);
    tap(180, 220);
    assert(view.state == REC_RECORDING && lv_arc_get_value(ring) == 0);
    point(180, 220, LV_INDEV_STATE_PRESSED);
    tick(530);
    assert(view.state == REC_RECORDING && lv_arc_get_value(ring) > 0 &&
           lv_arc_get_value(ring) < lv_arc_get_max_value(ring));
    save(argv[1], "recording");
    point(180, 220, LV_INDEV_STATE_RELEASED);
    assert(view.state == REC_RECORDING && lv_arc_get_value(ring) == 0);

    point(180, 210, LV_INDEV_STATE_PRESSED);
    tick(300);
    point(221, 210, LV_INDEV_STATE_PRESSED);
    tick(1200);
    assert(view.state == REC_RECORDING && lv_arc_get_value(ring) == 0);
    point(221, 210, LV_INDEV_STATE_RELEASED);

    point(180, 220, LV_INDEV_STATE_PRESSED);
    tick(1200);
    assert(view.state == REC_RECORDING && lv_arc_get_value(ring) == lv_arc_get_max_value(ring));
    tick(120);
    require_label("Saving recording");
    require_label("00:24");
    point(180, 220, LV_INDEV_STATE_RELEASED);
    lv_refr_now(NULL);
    const lv_color_t initial_center = frame[157 * 360 + 180];
    tick(590);
    save(argv[1], "saving");
    assert(frame[157 * 360 + 180].full != initial_center.full);
    tap(180, 157);
    swipe(180, 310, 0, -84);
    assert(view.state == REC_SAVING);
    require_label("Saving recording");
    assert(!has_label(lv_scr_act(), "Recordings"));

    view.state = REC_UNSAVED;
    view.message = "Save failed. Tap to retry";
    tick(100);
    require_label("Save failed. Tap to retry");
    assert(!has_label(lv_scr_act(), "Saving recording"));
    tap(180, 190);
    require_label("Saving recording");
    view.state = REC_IDLE;
    view.message = "Recording saved";
    view.elapsed_ms = 0;
    tick(100);
    require_label("Recording saved");
    require_label("Start Recording");
    assert(!has_label(lv_scr_act(), "Saving recording"));
    save(argv[1], "saved");
    view.message = NULL;
    tick(100);

    swipe(180, 338, 0, -94);
    require_label("Recordings");
    assert(!has_label(lv_scr_act(), "Recording 012"));
    save(argv[1], "menu");
    swipe(180, 227, 0, 68);
    require_label("Start Recording");
    assert(!has_label(lv_scr_act(), "Recordings"));
    swipe(180, 311, 0, -84);
    require_label("Recordings");
    tap(251, 273);
    require_label("Recording 012");
    save(argv[1], "library");
    tap(258, 128);
    require_label("Playing");
    require_label("Volume");
    save(argv[1], "playing");
    swipe(206, 181, 65, 0);
    assert(volume > 64);
    require_label("Volume");
    tap(240, 128);
    require_label("Paused");
    tap(230, 128);
    require_label("Playing");
    swipe(253, 260, 0, -130);
    assert(!has_label(lv_scr_act(), "Recording 012"));
    swipe(259, 140, -122, 0);
    require_label("Swipe right to go back");
    swipe(102, 140, 122, 0);
    require_label("Start Recording");
    assert(!view.clip_id);

    tap(180, 308);
    tap(180, 273);
    require_label("Recording 012");
    point(190, 125, LV_INDEV_STATE_PRESSED);
    tick(500);
    point(190, 125, LV_INDEV_STATE_RELEASED);
    require_label("Delete recording?");
    tap(233, 205);
    assert(view.count == 11 && view.state == REC_IDLE);
    puts("Recorder UI verified: hold-to-stop, saving and retry, menu gestures, scrolling, playback, volume, and delete.");
}
