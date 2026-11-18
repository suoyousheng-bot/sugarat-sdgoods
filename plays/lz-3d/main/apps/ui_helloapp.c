/* Modified by Sugarat: see lz_3d.md for source changes. */
/*
 * 谷仓共创计划 · 谷仓 SDGOODS 开放平台基础工程
 * 应用层示例
 * https://github.com/SDGOODS/SDGOODS-ESP32S3
 *
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件属于应用层，以 Apache-2.0 发布：可自由商用、可闭源分发，
 * 只需保留本声明并携带 NOTICE 文件。详见 LICENSING.md。
 */

/*
 * lz 实时 3D 角色：手指左右拖动转身、上下拖动俯仰（松手回正），松手后带惯性；
 * 点一下依次播放 hello / listen / talk，播完回到 idle 循环，动作之间 250 ms 过渡。
 *
 * 渲染走 sugarat_frame 的直推流：每帧只把角色包围盒（并上一帧）按 12 行一条推给屏幕，
 * 每条由两个核分列渲染（lz3d_render），渲染与上一条的传输重叠。底部 FPS 行由 LVGL 绘制，
 * 位于 LZ_CLIP_Y1 以下，直推区域不会覆盖它。
 *
 * 每次开机先播一次开机动画（bootanim.bin，tools/encode_bootanim.py 生成）：画面按声音的起播时刻
 * 对齐，只推每帧变化的区域；播完用 BOOT_XFADE_US 从最后一帧交叉淡化到 3D 画面，然后释放动画
 * 占用的内存。动画期间触摸不起作用。
 */

#include "ui_helloapp.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "sdgoods_board.h"
#include "sugarat_frame.h"
#include "lz3d.h"
#include "bootanim.h"

LV_FONT_DECLARE(si_yuan_black_icon_14);

static const char *TAG = "lz3d";

#define LZ_SCREEN        360
#define LZ_ROWS          12          /* = sugarat_frame default strip rows */
#define LZ_CHUNKS        4           /* column chunks per strip, claimed by both cores */
#define LZ_CLIP_Y1       304         /* rows below stay LVGL-owned (FPS label) */
#define LZ_BG            0x1083      /* native RGB565 backdrop, matches the screen colour */
#define LZ_PIXELS        150.0f      /* screen radius of the model's bounding sphere */
#define LZ_DRAG_RAD_PX   0.0120f     /* rotation per dragged pixel */
#define LZ_PITCH_MAX     0.50f
#define LZ_INERTIA_TAU_S 0.45f       /* yaw velocity decays by e every tau after release */
#define LZ_PITCH_TAU_S   0.18f       /* pitch returns to level with this time constant */
#define LZ_VEL_MAX       14.0f
#define LZ_TAP_PX        10
#define LZ_TAP_US        400000
#define LZ_BLEND_US      250000
#define LZ_STATS_US      2000000
#define BOOT_XFADE_US    400000      /* last boot frame -> 3D view */
#define BOOT_AUDIO_WAIT_US 500000    /* video starts on its own clock if audio has not started by then */

extern const uint8_t bootanim_bin_start[] asm("_binary_bootanim_bin_start");
extern const uint8_t bootanim_bin_end[] asm("_binary_bootanim_bin_end");

enum { ANIM_IDLE = 0, ANIM_HELLO, ANIM_LISTEN, ANIM_TALK };

static lv_obj_t *s_scr, *s_fps;
static lv_timer_t *s_timer;
static lz3d_t s_r;
static bool s_ready, s_paused, s_stream_ok;
static sugarat_frame_stream_t *s_stream;
static sugarat_frame_pacer_t s_pacer;
static sugarat_frame_stats_t s_stats;
static uint16_t s_zbuf[LZ_SCREEN * LZ_ROWS];   /* .bss: internal RAM */
static float *s_from;                          /* pose snapshot blended out after a switch */
static int s_px0, s_py0, s_px1, s_py1;         /* previous frame bounds; empty when x1<=x0 */
static int64_t s_last_us, s_prep_us_sum, s_render_frames;

typedef enum { BOOT_NONE, BOOT_VIDEO, BOOT_XFADE } boot_phase_t;
static bool s_boot_played;                     /* once per power-on */
static bootanim_audio_t s_boot_audio;          /* read by the audio task until the clip ends */
static struct {
    boot_phase_t phase;
    bootanim_t anim;
    bool audio_started, audio_on, full, release_pending;
    int64_t open_us, t0, xfade_us, released_us;
    int pushes;
    int64_t decode_sum, decode_max, send_sum;
    unsigned internal_before, psram_before;
} s_boot;

static struct {
    int anim, next_tap;
    int64_t start_us, blend_us;
    float yaw, pitch, vel;
    bool drag, moved;
    lv_point_t press;
    float yaw0, pitch0;
    int64_t press_us, move_us;
    lv_coord_t last_x;
} s;

/* Small hot arrays prefer internal RAM; PSRAM keeps the renderer working when it is short. */
static void *alloc_internal(unsigned bytes)
{
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void *alloc_psram(unsigned bytes)
{
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void *alloc_internal_sz(size_t bytes) { return alloc_internal((unsigned)bytes); }
static void *alloc_psram_sz(size_t bytes) { return alloc_psram((unsigned)bytes); }

static void play(int anim, int64_t now)
{
    memcpy(s_from, s_r.pose, sizeof(float) * 3 * lz_model.pos_count);
    s.anim = anim;
    s.start_us = now;
    s.blend_us = now;
}

/* ---- touch ---- */
static void on_touch(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    int64_t now = esp_timer_get_time();
    if (s_boot.phase != BOOT_NONE) return;
    if (code == LV_EVENT_PRESSED) {
        s.drag = true; s.moved = false;
        s.press = p; s.press_us = now; s.move_us = now; s.last_x = p.x;
        s.yaw0 = s.yaw; s.pitch0 = s.pitch; s.vel = 0;
    } else if (code == LV_EVENT_PRESSING && s.drag) {
        int dx = p.x - s.press.x, dy = p.y - s.press.y;
        if (dx * dx + dy * dy > LZ_TAP_PX * LZ_TAP_PX) s.moved = true;
        s.yaw = s.yaw0 + dx * LZ_DRAG_RAD_PX;
        float pitch = s.pitch0 + dy * LZ_DRAG_RAD_PX;
        s.pitch = pitch > LZ_PITCH_MAX ? LZ_PITCH_MAX : pitch < -LZ_PITCH_MAX ? -LZ_PITCH_MAX : pitch;
        float dt = (now - s.move_us) * 1e-6f;
        if (dt > 0.004f) {
            float inst = (p.x - s.last_x) * LZ_DRAG_RAD_PX / dt;
            s.vel = 0.6f * inst + 0.4f * s.vel;
            s.last_x = p.x; s.move_us = now;
        }
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && s.drag) {
        s.drag = false;
        /* A finger that rested before lifting throws nothing. */
        if (now - s.move_us > 80000) s.vel = 0;
        if (s.vel > LZ_VEL_MAX) s.vel = LZ_VEL_MAX;
        if (s.vel < -LZ_VEL_MAX) s.vel = -LZ_VEL_MAX;
        if (code == LV_EVENT_RELEASED && !s.moved && now - s.press_us < LZ_TAP_US) {
            static const int order[] = {ANIM_HELLO, ANIM_LISTEN, ANIM_TALK};
            s.vel = 0;
            play(order[s.next_tap], now);
            s.next_tap = (s.next_tap + 1) % 3;
        }
    }
}

/* ---- strips ---- */
/* fade < 0: plain 3D. 0..32: 3D weight blended over the boot animation's last frame. */
typedef struct { uint16_t *px; int x1, x2, y, rows, fade; } strip_job_t;
static strip_job_t s_job;

/* Blends two byte-swapped RGB565 pixels; k = 0..32 is the weight of b. */
static inline uint16_t mix565(uint16_t a_sw, uint16_t b_sw, int k)
{
    uint32_t a = (uint16_t)((a_sw << 8) | (a_sw >> 8)), b = (uint16_t)((b_sw << 8) | (b_sw >> 8));
    a = (a | a << 16) & 0x07E0F81Fu;
    b = (b | b << 16) & 0x07E0F81Fu;
    uint32_t m = ((a * (uint32_t)(32 - k) + b * (uint32_t)k) >> 5) & 0x07E0F81Fu;
    uint16_t c = (uint16_t)(m | m >> 16);
    return (uint16_t)((c << 8) | (c >> 8));
}

static void chunk(int i, void *arg)
{
    (void)arg;
    int w = s_job.x2 - s_job.x1;
    int a = s_job.x1 + w * i / LZ_CHUNKS, b = s_job.x1 + w * (i + 1) / LZ_CHUNKS;
    lz3d_render(&s_r, s_job.px, s_zbuf, s_job.x1, w, s_job.y, s_job.rows, a, b);
    if (s_job.fade < 0) return;
    const uint16_t *video = s_boot.anim.frame;
    const uint16_t bg = s_r.cfg.bg;
    for (int r = 0; r < s_job.rows; r++) {
        int y = s_job.y + r;
        uint16_t *o = s_job.px + r * w - s_job.x1;
        const uint16_t *v = video + y * LZ_SCREEN;
        for (int x = a; x < b; x++) {
            uint16_t p = y < LZ_CLIP_Y1 ? o[x] : bg;   /* rows below stay LVGL background after the fade */
            o[x] = mix565(v[x], p, s_job.fade);
        }
    }
}

static void fill(uint16_t *pixels, int x1, int y, int x2, int rows, void *arg)
{
    int fade = arg ? *(const int *)arg : -1;
    s_job = (strip_job_t){pixels, x1, x2, y, rows, fade};
    sugarat_frame_parallel(LZ_CHUNKS, chunk, NULL);
}

/* ---- boot animation ---- */
static void boot_fill(uint16_t *pixels, int x1, int y, int x2, int rows, void *arg)
{
    (void)arg;
    int w = x2 - x1;
    for (int r = 0; r < rows; r++)
        memcpy(pixels + r * w, s_boot.anim.frame + (y + r) * LZ_SCREEN + x1, (size_t)w * 2);
}

static size_t boot_audio_read(int16_t *pcm, size_t max, void *arg)
{
    return bootanim_audio_read(arg, pcm, max);
}

static void heap_free(void *p) { heap_caps_free(p); }

static void boot_open(void)
{
    s_boot_played = true;
    memset(&s_boot, 0, sizeof(s_boot));
    s_boot.internal_before = (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s_boot.psram_before = (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (!bootanim_open(&s_boot.anim, bootanim_bin_start, (size_t)(bootanim_bin_end - bootanim_bin_start),
                       alloc_internal_sz, alloc_psram_sz, heap_free)) {
        ESP_LOGE(TAG, "boot anim: invalid data or out of memory; skipped");
        return;
    }
    if (s_boot.anim.width != LZ_SCREEN || s_boot.anim.height != LZ_SCREEN) {
        ESP_LOGE(TAG, "boot anim: %dx%d does not match the screen; skipped", s_boot.anim.width, s_boot.anim.height);
        bootanim_close(&s_boot.anim);
        return;
    }
    s_boot.phase = BOOT_VIDEO;
    s_boot.full = true;
    ESP_LOGI(TAG, "boot anim: %d frames at %d fps, audio %.2f s, %u bytes; before: internal free=%u psram free=%u; playing: internal free=%u psram free=%u",
             s_boot.anim.frame_count, s_boot.anim.fps, s_boot.anim.audio_samples / (float)(s_boot.anim.audio_rate ? s_boot.anim.audio_rate : 1),
             (unsigned)(bootanim_bin_end - bootanim_bin_start), s_boot.internal_before, s_boot.psram_before,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* Hands the panel to the 3D view. After a failure the screen is repainted by LVGL first. */
static void boot_finish(int64_t now, bool failed)
{
    bootanim_close(&s_boot.anim);
    s_boot.phase = BOOT_NONE;
    s_boot.release_pending = true;
    if (s_fps) lv_obj_clear_flag(s_fps, LV_OBJ_FLAG_HIDDEN);
    if (failed) {
        if (s_scr) lv_obj_invalidate(s_scr);
        s_px0 = s_px1 = 0;
    } else {
        s_px0 = s_r.x0; s_py0 = s_r.y0; s_px1 = s_r.x1; s_py1 = s_r.y1;
    }
    s_last_us = now;
    int decoded = s_boot.anim.next;
    ESP_LOGI(TAG, "boot anim: %s, frames=%d pushes=%d decode avg=%lldus max=%lldus send avg=%lldus",
             failed ? "stopped" : "done", decoded, s_boot.pushes,
             decoded ? s_boot.decode_sum / decoded : 0, s_boot.decode_max,
             s_boot.pushes ? s_boot.send_sum / s_boot.pushes : 0);
}

/* Logs free memory once the frame buffers are freed and the audio task has exited. */
static void boot_release_check(int64_t now)
{
    if (!s_boot.release_pending || sdgoods_audio_clip_busy()) return;
    if (!s_boot.released_us) { s_boot.released_us = now; return; }
    if (now - s_boot.released_us < 200000) return;   /* the idle task frees the audio task's stack */
    s_boot.release_pending = false;
    ESP_LOGI(TAG, "boot anim released: internal free=%u (before %u) psram free=%u (before %u)",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), s_boot.internal_before,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), s_boot.psram_before);
}

static bool boot_send(int x0, int y0, int x1, int y1, sugarat_frame_strip_fn fn, void *arg, int64_t now)
{
    esp_err_t err = sugarat_frame_stream_send(s_stream, x0, y0, x1, y1, fn, arg);
    if (err != ESP_OK) {
        s_stream_ok = false;
        ESP_LOGE(TAG, "panel stream failed: %s; 3D view stopped", esp_err_to_name(err));
        boot_finish(now, true);
        lv_label_set_text(s_fps, "display stream error");
        return false;
    }
    s_boot.pushes++;
    s_boot.send_sum += esp_timer_get_time() - now;
    return true;
}

static void boot_tick(int64_t now)
{
    bootanim_t *b = &s_boot.anim;
    if (!s_boot.audio_started) {
        /* Started here, after main.c restored the volume, so the amplifier follows it. */
        s_boot.audio_started = true;
        s_boot.open_us = now;
        if (b->audio_samples) {
            bootanim_audio_init(b, &s_boot_audio);
            esp_err_t err = sdgoods_audio_clip_start(boot_audio_read, &s_boot_audio);
            s_boot.audio_on = err == ESP_OK;
            if (err != ESP_OK) ESP_LOGW(TAG, "boot anim: audio not started: %s", esp_err_to_name(err));
        }
    }
    if (s_paused || !sugarat_frame_lvgl_layers_clear()) {
        s_boot.full = true;   /* LVGL drew over the panel; repaint the whole frame afterwards */
        return;
    }

    if (s_boot.phase == BOOT_VIDEO) {
        if (!s_boot.t0) {
            int64_t a = s_boot.audio_on ? sdgoods_audio_clip_start_us() : 0;
            if (a) s_boot.t0 = a;
            else if (!s_boot.audio_on || now - s_boot.open_us > BOOT_AUDIO_WAIT_US) s_boot.t0 = now;
        }
        int target = 0;
        if (s_boot.t0 && now > s_boot.t0) target = (int)((now - s_boot.t0) * b->fps / 1000000);
        if (target >= b->frame_count) target = b->frame_count - 1;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        while (b->next <= target) {
            int64_t t = esp_timer_get_time();
            if (!bootanim_decode_next(b, &x0, &y0, &x1, &y1)) {
                ESP_LOGE(TAG, "boot anim: frame %d is corrupt", b->next);
                boot_finish(now, true);
                return;
            }
            t = esp_timer_get_time() - t;
            s_boot.decode_sum += t;
            if (t > s_boot.decode_max) s_boot.decode_max = t;
        }
        if (s_boot.full) { x0 = 0; y0 = 0; x1 = LZ_SCREEN; y1 = LZ_SCREEN; s_boot.full = false; }
        if (x1 > x0 && !boot_send(x0, y0, x1, y1, boot_fill, NULL, esp_timer_get_time())) return;
        if (s_boot.t0 && now >= s_boot.t0 + (int64_t)b->frame_count * 1000000 / b->fps) {
            s_boot.phase = BOOT_XFADE;
            s_boot.xfade_us = now;
            s.start_us = s.blend_us = now - LZ_BLEND_US;   /* idle starts from its first frame */
            sugarat_frame_pacer_reset(&s_pacer);
        }
        return;
    }

    /* BOOT_XFADE */
    if (!sugarat_frame_pacer_due(&s_pacer, now)) return;
    float p = (float)(now - s_boot.xfade_us) / BOOT_XFADE_US;
    if (p > 1.0f) p = 1.0f;
    int fade = (int)(32.0f * p * p * (3.0f - 2.0f * p) + 0.5f);
    lz3d_pose(&s_r, ANIM_IDLE, (now - s.start_us) * 1e-6f);
    lz3d_prepare(&s_r, 0, 0);
    if (!lz3d_bin(&s_r, 0, LZ_SCREEN)) {
        ESP_LOGE(TAG, "triangle bins overflowed; fade skipped");
        fade = 32;
        lz3d_bin(&s_r, 0, 0);   /* no strips: the frame shows the background only */
    }
    if (!boot_send(0, 0, LZ_SCREEN, LZ_SCREEN, fill, &fade, now)) return;
    sugarat_frame_pacer_finish(&s_pacer, esp_timer_get_time(), true);
    if (fade >= 32) boot_finish(now, false);
}

/* ---- frame ---- */
static void frame_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_scr || !s_ready) return;
    int64_t now = esp_timer_get_time();
    if (s_boot.phase != BOOT_NONE) {
        if (s_stream_ok) boot_tick(now);
        return;
    }
    boot_release_check(now);
    if (s_paused || !s_stream_ok || !sugarat_frame_lvgl_layers_clear()) {
        /* LVGL owns the panel; repaint the whole character once it is back. */
        s_px1 = s_px0;
        sugarat_frame_pacer_reset(&s_pacer);
        s_last_us = now;
        return;
    }
    if (!sugarat_frame_pacer_due(&s_pacer, now)) return;

    float dt = s_last_us ? (now - s_last_us) * 1e-6f : 0;
    s_last_us = now;
    if (dt > 0.1f) dt = 0.1f;
    if (!s.drag) {
        s.yaw += s.vel * dt;
        s.vel *= expf(-dt / LZ_INERTIA_TAU_S);
        if (fabsf(s.vel) < 0.02f) s.vel = 0;
        s.pitch *= expf(-dt / LZ_PITCH_TAU_S);
    }
    s.yaw = fmodf(s.yaw, 6.2831853f);

    const lz_anim_t *a = &lz_model.anims[s.anim];
    float at = (now - s.start_us) * 1e-6f;
    if (!a->loop && at * a->fps >= a->frames) {
        play(ANIM_IDLE, now);
        at = 0;
    }
    lz3d_pose(&s_r, s.anim, at);
    if (now - s.blend_us < LZ_BLEND_US) {
        float k = 1.0f - (float)(now - s.blend_us) / LZ_BLEND_US;
        lz3d_pose_blend(&s_r, s_from, k * k * (3 - 2 * k));
    }
    lz3d_prepare(&s_r, s.yaw, s.pitch);

    int x0 = s_r.x0, y0 = s_r.y0, x1 = s_r.x1, y1 = s_r.y1;
    if (s_px1 > s_px0) {
        if (s_px0 < x0) x0 = s_px0;
        if (s_py0 < y0) y0 = s_py0;
        if (s_px1 > x1) x1 = s_px1;
        if (s_py1 > y1) y1 = s_py1;
    }
    int64_t prepared = esp_timer_get_time();
    esp_err_t err = ESP_OK;
    if (x1 > x0 && y1 > y0) {
        if (!lz3d_bin(&s_r, y0, y1)) {
            ESP_LOGE(TAG, "triangle bins overflowed; frame skipped");
        } else {
            err = sugarat_frame_stream_send(s_stream, x0, y0, x1, y1, fill, NULL);
        }
    }
    if (err != ESP_OK) {
        s_stream_ok = false;
        ESP_LOGE(TAG, "panel stream failed: %s; 3D view stopped", esp_err_to_name(err));
        lv_label_set_text(s_fps, "display stream error");
        return;
    }
    s_px0 = s_r.x0; s_py0 = s_r.y0; s_px1 = s_r.x1; s_py1 = s_r.y1;

    int64_t done = esp_timer_get_time();
    s_prep_us_sum += prepared - now;
    s_render_frames++;
    sugarat_frame_stats_record(&s_stats, now, done, true, true);
    sugarat_frame_pacer_finish(&s_pacer, done, true);

    sugarat_frame_summary_t sum;
    if (sugarat_frame_stats_window(&s_stats, done, LZ_STATS_US, &sum)) {
        int64_t send_us = sugarat_frame_stream_take_send_us(s_stream);
        ESP_LOGI(TAG, "fps=%.1f avg=%lldus max=%lldus gap_max=%lldus prep_avg=%lldus panel=%lldus tris=%d region=%dx%d internal_free=%u",
                 sum.fps, sum.avg_us, sum.max_us, sum.gap_max_us,
                 s_render_frames ? s_prep_us_sum / s_render_frames : 0, send_us / (sum.frames ? sum.frames : 1),
                 s_r.tri_visible, x1 - x0, y1 - y0,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        s_prep_us_sum = 0; s_render_frames = 0;
        char b[48];
        snprintf(b, sizeof(b), "%.0f fps  %.1f ms", sum.fps, sum.avg_us / 1000.0f);
        lv_label_set_text(s_fps, b);
    }
}

static void on_pause(void) { s_paused = true; }
static void on_resume(void) { s_paused = false; }

static void on_menu_exit(void)
{
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    if (s_boot.phase != BOOT_NONE) {
        bootanim_close(&s_boot.anim);
        s_boot.phase = BOOT_NONE;
    }
    sdgoods_lvgl_fast_loop(false);
    s_scr = NULL;
    s_fps = NULL;
    ESP_LOGI(TAG, "exit");
}

static bool renderer_init(void)
{
    const lz_model_t *m = &lz_model;
    ESP_LOGI(TAG, "before init: internal free=%u largest=%u psram free=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    /* The panel strips need internal DMA RAM, so they are allocated before anything else. */
    sugarat_frame_workers_config_t workers = sugarat_frame_workers_default_config();
    esp_err_t err = sugarat_frame_workers_start(&workers);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "render helper failed: %s", esp_err_to_name(err));
        return false;
    }
    sugarat_frame_stream_config_t sc = sugarat_frame_stream_default_config();
    err = sugarat_frame_stream_create(&sc, &s_stream);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel stream failed: %s", esp_err_to_name(err));
        return false;
    }
    /* Texels are read at random per pixel. Octal PSRAM refills cache lines several times faster
     * than DIO flash, and internal RAM stays free for the panel, LVGL and Wi-Fi. */
    unsigned tex_bytes = (unsigned)m->tex_size * m->tex_size * 2;
    const uint16_t *tex = m->texture;
    uint16_t *copy = alloc_psram(tex_bytes);
    if (copy) { memcpy(copy, m->texture, tex_bytes); tex = copy; }
    ESP_LOGI(TAG, "texture %dx%d in %s", m->tex_size, m->tex_size, copy ? "PSRAM" : "flash");
    lz3d_config_t cfg = {
        .screen_w = LZ_SCREEN, .screen_h = LZ_SCREEN,
        .pivot_x = 180.0f, .pivot_y = 168.0f,
        .scale = LZ_PIXELS / m->radius,
        .clip_y1 = LZ_CLIP_Y1,
        .bg = (uint16_t)((LZ_BG << 8) | (LZ_BG >> 8)),
        .texture = tex,
        .cull = true,
    };
    if (!lz3d_init(&s_r, m, &cfg, LZ_ROWS, alloc_internal, alloc_psram)) {
        ESP_LOGE(TAG, "renderer buffers could not be allocated");
        return false;
    }
    s_from = alloc_internal(sizeof(float) * 3 * m->pos_count);
    if (!s_from) return false;
    lz3d_pose(&s_r, ANIM_IDLE, 0);
    memcpy(s_from, s_r.pose, sizeof(float) * 3 * m->pos_count);

    ESP_LOGI(TAG, "model tris=%d positions=%d anims=%d strips=%d; after init: internal free=%u psram free=%u",
             m->tri_count, m->pos_count, m->anim_count, sugarat_frame_stream_buffers(s_stream),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return true;
}

void ui_helloapp_start(void)
{
    if (s_scr) {
        lv_scr_load(s_scr);
        return;
    }
    if (!s_ready) {
        s_ready = renderer_init();
        s_stream_ok = s_ready;
    }

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_make((LZ_BG >> 11) << 3, ((LZ_BG >> 5) & 63) << 2, (LZ_BG & 31) << 3), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Full-screen touch surface without styles, so pressing never invalidates the 3D region. */
    lv_obj_t *pad = lv_obj_create(s_scr);
    lv_obj_remove_style_all(pad);
    lv_obj_set_size(pad, LZ_SCREEN, LZ_SCREEN);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pad, on_touch, LV_EVENT_ALL, NULL);

    s_fps = lv_label_create(s_scr);
    lv_label_set_text(s_fps, s_ready ? "" : "3D init failed");
    lv_obj_set_style_text_font(s_fps, &si_yuan_black_icon_14, 0);
    lv_obj_set_style_text_color(s_fps, lv_color_hex(0x8890a8), 0);
    lv_obj_set_width(s_fps, SDG_UI_SAFE_W);
    lv_obj_set_style_text_align(s_fps, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_fps, SDG_UI_SAFE_X, LZ_CLIP_Y1 + 4);
    if (s_ready && !s_boot_played) {
        boot_open();
        if (s_boot.phase != BOOT_NONE) lv_obj_add_flag(s_fps, LV_OBJ_FLAG_HIDDEN);
    }

    lv_scr_load(s_scr);
    sdgoods_app_shell_bind(s_scr);
    sdgoods_app_shell_set_exit_cb(on_menu_exit);
    sdgoods_app_shell_set_pause_cb(on_pause);
    sdgoods_app_shell_set_resume_cb(on_resume);

    memset(&s, 0, sizeof(s));
    s.start_us = s.blend_us = esp_timer_get_time() - LZ_BLEND_US;
    s_px0 = s_px1 = 0;
    s_last_us = 0;
    sugarat_frame_pacer_init(&s_pacer, SUGARAT_FRAME_SLOT_60HZ_US);
    if (s_ready) {
        s_timer = lv_timer_create(frame_cb, 1, NULL);
        sdgoods_lvgl_fast_loop(true);
    }
    ESP_LOGI(TAG, "start");
}

void ui_helloapp_poll(void)
{
}
