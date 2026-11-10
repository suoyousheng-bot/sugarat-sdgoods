# 谷仓直推出帧组件

[English](README.md) | 中文

在谷仓固件工程根目录以 `target=esp-idf` 安装 `sdgoods-frame-stream`，并把 `sugarat_frame` 加入 `main/REQUIRES`。组件把持续变化的屏幕区域从内部 RAM 直接推给屏幕，由两个核渲染，帧率均匀。它依赖谷仓模板 BSP 中的推屏完成切换和 `sdgoods_lvgl_fast_loop()`；缺少这两项时每条带额外等待约 1.5 ms。在徽章上，整屏 360x360 双线性旋转稳定为 30 fps（每帧 24 ms，最长 30 ms）；圆盘越小，耗时按比例越少。

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

## 何时可以直接写屏

直推的条带会覆盖 LVGL 在该区域显示的内容，LVGL 不会重画。以下条件全部成立时才发送，否则通过 LVGL 绘制这一帧：`sugarat_frame_lvgl_layers_clear()` 为真；没有手指按下（触摸反馈和滑动由 LVGL 绘制）；应用外壳没有暂停本应用（跟踪 `sdgoods_app_shell_set_pause_cb()` 与 `set_resume_cb()`）；本页面最近一次 LVGL 帧已画好该区域周围的静态背景。发送失败后通过 LVGL 绘制这一帧。`ESP_ERR_TIMEOUT` 会永久停用该推屏流，因为发送任务可能仍占用某条带。

## 出帧节奏

运动量按 `esp_timer_get_time()` 计算，不按帧数计算。`sugarat_frame_pacer_due()` 在固定的 16.667 ms 网格上放行帧；超过一格的帧从下一个空闲格开始，因此 16.7～33.3 ms 的帧以均匀的 30 fps 运行，而不是间隔忽长忽短。非触摸状态下 BSP 主循环每轮睡一个 10 ms tick，定时器会被量化：动画期间调用 `sdgoods_lvgl_fast_loop(true)`，动画停止或离开页面时调用 `false`。页面可见时还要把 `_lv_disp_get_refr_timer()` 保持在较短周期（例如 4 ms），使走 LVGL 路径的帧在同一轮上屏；退出时恢复 `CONFIG_LV_DISP_DEF_REFR_PERIOD`。`sugarat_frame_stats_record()` 与 `sugarat_frame_stats_window()` 给出帧率、平均与最长帧时间以及动画帧之间的最大间隔；每 10 秒记录一次，在设备上对比改动前后。

## 渲染

`sugarat_frame_parallel()` 在调用任务和 1 号核上的辅助任务中执行带编号的工作；两者从共享计数器领取下一个编号，一个核被抢占时另一个核不会空等。同一时间只从一个任务调用，并在 0 号核（LVGL 任务）上调用。`sugarat_frame_rotate()` 以这种方式渲染 `sugarat_rotate.h` 的任务。旋转纹理是原始字节序 RGB565 的正方形；背景与输出与 `LV_COLOR_16_SWAP` 一样是字节交换格式。内部像素使用不钳位的双线性采样，每条带的行几何只计算一次。渲染受 CPU 算力限制：纹理放在 PSRAM 与放在缓存内相比，耗时增加不到 10%。`sugarat_rotate.h` 与 `sugarat_frame_pacing.h` 不依赖 ESP-IDF；`test/host_test.c` 用 `cc -std=c11 -fsanitize=address,undefined -Iinclude test/host_test.c -lm` 检查它们。

## 内存与限制

默认推屏流在内部 DMA RAM 中占用两条 8640 字节的条带（360 像素 x 12 行）和 3 KB 发送任务栈；Wi-Fi 开启时内部 RAM 仍剩约 29 KB。只能分配一条带时推屏流仍可工作，但渲染与传输不重叠。更宽的条带能减少每条带的开销，但会占用 Wi-Fi 和屏幕 PSRAM 路径需要的内部 RAM。以上测量数据来自本组件所提取自的 Spotify 播放器（ESP-IDF 5.5.4、LVGL 8.3.11）；每个新应用的帧率都要在设备上验证。
