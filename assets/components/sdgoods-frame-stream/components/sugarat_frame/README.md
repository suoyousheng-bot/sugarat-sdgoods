# SDGOODS direct frame stream

English | [中文](README.zh.md)

Install `sdgoods-frame-stream` with `target=esp-idf` under the SDGOODS firmware project root and add `sugarat_frame` to `main/REQUIRES`. The component sends a continuously changing screen region straight to the panel from internal RAM, rendered on both cores, at an even frame rate. It requires the SDGOODS template BSP with the panel completion yield and `sdgoods_lvgl_fast_loop()`; without them every strip waits about 1.5 ms extra. On the badge a full 360x360 bilinear rotation runs at an even 30 fps (24 ms per frame, longest 30 ms); a smaller disc costs proportionally less.

```c
static sugarat_frame_stream_t *stream;
static sugarat_frame_pacer_t pacer;
static sugarat_rotate_job_t job;

static void fill(uint16_t *pixels, int x1, int y, int x2, int rows, void *arg)
{
    job.out = pixels; job.out_x = x1; job.out_y = y; job.stride = x2 - x1;
    sugarat_frame_rotate(&job, y, y + rows, x1, x2 - 1);
}

/* once, after sdgoods_lvgl_init() */
sugarat_frame_workers_config_t workers = sugarat_frame_workers_default_config();
ESP_ERROR_CHECK(sugarat_frame_workers_start(&workers));
sugarat_frame_stream_config_t config = sugarat_frame_stream_default_config();
ESP_ERROR_CHECK(sugarat_frame_stream_create(&config, &stream));
sugarat_frame_pacer_init(&pacer, SUGARAT_FRAME_SLOT_60HZ_US);

/* in an lv_timer with a 1 ms period, while the animated screen is visible */
int64_t now = esp_timer_get_time();
if (sugarat_frame_pacer_due(&pacer, now)) {
    job = sugarat_rotate_job(&params, angle_at(now), NULL, 32);
    int x0, y0, x1, y1; sugarat_rotate_bounds(&params, 360, &x0, &y0, &x1, &y1);
    esp_err_t err = can_stream ? sugarat_frame_stream_send(stream, x0, y0, x1 + 1, y1 + 1, fill, NULL) : ESP_FAIL;
    if (err != ESP_OK) { /* render the frame into the LVGL image instead and invalidate it */ }
    sugarat_frame_pacer_finish(&pacer, esp_timer_get_time(), true);
    sdgoods_lvgl_fast_loop(true);
}
```

## When the panel may be written directly

A direct strip overwrites whatever LVGL showed in that region, and LVGL does not redraw it. Send only when all of these hold, and otherwise draw the frame through LVGL: `sugarat_frame_lvgl_layers_clear()` is true; no finger is pressed (touch feedback and swipes are LVGL drawing); the app shell has not paused the application (track `sdgoods_app_shell_set_pause_cb()` and `set_resume_cb()`); and the last LVGL frame of this screen drew the static backdrop around the region. After a failed send, draw the frame through LVGL. `ESP_ERR_TIMEOUT` disables the stream for good, because the sender may still own a strip.

## Pacing

Derive motion from `esp_timer_get_time()`, never from frame counts. `sugarat_frame_pacer_due()` admits frames on a fixed 16.667 ms grid; a frame longer than one slot starts at the next free slot, so frames between 16.7 and 33.3 ms run at an even 30 fps instead of alternating gaps. Outside touch the BSP loop sleeps a 10 ms tick per pass, which quantizes the timer: call `sdgoods_lvgl_fast_loop(true)` while animating and `false` when the animation stops or the screen leaves. Also hold `_lv_disp_get_refr_timer()` at a short period (for example 4 ms) while the screen is visible, so LVGL-path frames reach the panel in the same pass; restore `CONFIG_LV_DISP_DEF_REFR_PERIOD` on exit. `sugarat_frame_stats_record()` and `sugarat_frame_stats_window()` produce fps, average and maximum frame time and the largest gap between animated frames; log them every 10 s and compare before and after a change on the device.

## Rendering

`sugarat_frame_parallel()` runs indexed work on the calling task and a helper on core 1; both claim the next index from a shared counter, so preemption on one core does not leave the other idle. Call it from one task at a time, on core 0 (the LVGL task). `sugarat_frame_rotate()` renders a `sugarat_rotate.h` job this way. The rotation texture is a square of native RGB565; the backdrop and output are byte-swapped as with `LV_COLOR_16_SWAP`. Interior pixels use an unclamped bilinear sampler, and row geometry is computed once per strip. The rendering is CPU-bound: keeping the texture in PSRAM costs under 10 % compared with a cache-resident texture. `sugarat_rotate.h` and `sugarat_frame_pacing.h` have no ESP-IDF dependency; `test/host_test.c` checks them with `cc -std=c11 -fsanitize=address,undefined -Iinclude test/host_test.c -lm`.

## Memory and limits

The default stream holds two 8640-byte strips (360 pixels x 12 rows) in internal DMA RAM and a 3 KB sender stack; with Wi-Fi active about 29 KB of internal RAM stayed free. With memory for one strip the stream works without overlap. Wider strips cut per-strip overhead but take internal RAM that Wi-Fi and the panel's PSRAM path need. The measured figures come from the Spotify player that this component was extracted from (ESP-IDF 5.5.4, LVGL 8.3.11); verify the frame rate of each new application on the device.
