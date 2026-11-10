# 谷仓音频

[English](README.md) | 中文

安装时选择 `sdgoods-audio`、`target=esp-idf`，目录使用谷仓固件工程根目录，并在 `main/REQUIRES` 中添加 `sugarat_audio`。在 LVGL 和触摸初始化之后，用 `sugarat_audio_init(&config)` 替换模板中的 `sdgoods_audio_init()` 调用，并检查返回的错误码。组件独占扬声器使用的 I2S0 TX、麦克风使用的 I2S1 RX，以及 GPIO 3 上的功放使能脚。ESP32-S3 只有这两个 I2S 端口，如果固件同时调用 `sdgoods_audio_init()`，会收到 I2S 驱动错误。BSP 的 `sdgoods_audio_get_volume()`、`sdgoods_audio_set_volume()` 和 `sdgoods_audio_bgm_stop()` 仍可安全调用，控制中心的音量页面照常工作；组件占用端口期间，`sdgoods_audio_bgm_start()` 以及 BSP 的录音和播放会失败。

```c
sugarat_audio_config_t audio = sugarat_audio_default_config();
audio.on_event = on_audio_event; /* runs on the audio task */
ESP_ERROR_CHECK(sugarat_audio_init(&audio)); /* instead of sdgoods_audio_init() */
ESP_ERROR_CHECK(sugarat_audio_tone(1000, 300));
```

## 操作

一个音频任务同一时间只执行一项操作：单音、扫频、PCM 播放、WAV 播放、录音、播放录音或麦克风电平监测。发起新请求或调用 `sugarat_audio_stop()` 时，当前操作会在两个数据块之间结束，并发布 STOPPED 事件。音频格式为 16 kHz、16 位 PCM，立体声播放时混合为单声道。`sugarat_audio_play_wav()` 接受 16 位、16 kHz、单声道或双声道的 RIFF PCM 文件，其他编码或采样率返回 `ESP_ERR_NOT_SUPPORTED`。在操作完成、被新请求替换或被停止之前，调用方必须保持 PCM 和 WAV 缓冲区有效。录音存放在 PSRAM 中，最长 `record_max_ms`（默认 5000，每秒 32 KB）；开启 `process_recording` 后会去除直流和低频噪声、统一响度，并在首尾做淡入淡出。`sugarat_audio_write_wav_header()` 写入 44 字节文件头，用于导出录音。

`follow_system_volume` 为 true 时，播放音量跟随控制中心，否则使用 `volume_percent`。每项操作先输出 80 ms 静音再打开功放，结束时保留 60 ms 尾音后关闭，以避免爆音；音量变化在一个 16 ms 数据块内平滑过渡，音量为 0 时功放保持打开并输出静音。基础工程 BSP 的 `sdgoods_audio_set_volume()` 在 BSP 背景音乐没有播放时会关闭功放。组件会在下一个数据块重新打开功放，声音能继续，但播放中拖动音量滑块会有爆音。要让调音量没有爆音，把 `components/sdgoods_board/src/sdgoods_audio.c` 中 `sdgoods_audio_set_volume()` 里的 `pa_apply();` 改为 `if (s_bgm_run) { pa_apply(); }`，只在 BSP 背景音乐运行时控制功放。`ip-agent` 应用已包含这项修改。`sugarat_audio_get_status()` 报告状态、麦克风电平、峰值和 dBFS、进度、录音长度、实际音量和最近的错误。`config.on_event` 在音频任务中执行：请复制事件或用 `sdgoods_lvgl_post()` 投递到 LVGL 线程，不要在回调中调用 LVGL。

## 流式音频

`sugarat_audio_stream_start(&config)` 用于语音对话：麦克风持续工作，`config.on_mic` 在麦克风任务中每 16 ms 收到一块 16 kHz 单声道数据，扬声器播放 `sugarat_audio_stream_write()` 写入队列的内容。请在工作任务中调用它，不要在音频任务中调用；它会先停止当前操作。PSRAM 播放队列可容纳 `playback_buffer_ms`（200–10000）毫秒，在第一次启动时分配，之后再启动保持该大小。只能由一个任务写入；写入不阻塞，返回实际接收的完整采样数。`xStreamBufferCreateWithCaps()` 创建的队列比申请的少一个字节，因此写入不会拆开一个采样：否则队列写满时，之后的每个采样都会错位一个字节，播放出来是满幅噪声。清空时由音频任务把队列读空，而不是在写入方运行时重置队列。队列积累到 200 ms 才开始播放，较短的一段音频到达 250 ms 后也会开始播放；队列播空时用 4 ms 淡出，再积累到 120 ms 后继续播放，因此迟到的网络数据包不会变成听得见的断音。`sugarat_audio_stream_flush()` 清空待播放的音频，例如用户打断回复时。状态中新增 `out_level_percent`（扬声器 RMS，−50 到 0 dBFS）和 `out_active`（120 ms 内有音频送到扬声器），可用于嘴型或音量动画。有音频排队时打开功放，连续 800 ms 无音频时关闭；每段回复结束时会记录播放时长和播空次数。调用 `sugarat_audio_stop()` 结束流式音频。

## 验证范围

已在一块谷仓圆屏徽章上（ESP-IDF 5.5.4）完成实物验收，覆盖单音、扫频、播放中调节音量、麦克风电平、3 秒录音、录音回放和 WAV 播放。流式音频已在 IP Agent 应用的语音对话中验证：长达 17 s、会写满 10 s 队列的回复播放时没有播空断音或噪声。这块板子的功放和扬声器按约四分之一满幅设计，BSP 自带声音的峰值约为 8000，因此流式语音的峰值应限制在 9000 左右，否则会破音。BSP 按上述方式修改后，控制中心调节音量时可以继续播放；否则控制中心打开或应用暂停时，请停止音频。音质取决于外壳和扬声器，请在设备上确认。组件不解码压缩格式，不做重采样，也不修改 BSP 引脚、分区表或应用入口。
