# 音乐与媒体案例

[English](README.md) | 中文

在谷仓 360 像素圆屏徽章上复用用户已验收的本地播放器、专辑封面和播放控制。通过 本资源包入口说明中的独立安装脚本 安装 `sdgoods-case-media`，指定 `target=esp-idf` 和项目语言。依赖包含 Watch 控件、Pixel 及音频驱动。在 main 的 `REQUIRES` 中添加 `sugarat_media`；安装保留自定义源码和现有应用入口，不会访问设备。

## 场景与语言

选择 `MEDIA_SCENE_PLAYER`、`MEDIA_SCENE_COVER` 或 `MEDIA_SCENE_CONTROLS`。根据用户明确指定的语言选择 `MEDIA_LOCALE_ZH` 或 `MEDIA_LOCALE_EN`，否则采用参考内容语言，再采用对话语言。所有场景共用播放状态，标题显示曲名。独立案例保持 `gallery_navigation=false`；仅为明确的测试总览开启箭头。初始暂停，隐藏屏幕后不会自动恢复播放。控件包括进度跳转、上一首/下一首、连续音量、恢复最近音量的静音和单曲循环。

## 集成与音频

阅读 `include/sugarat_media.h` 和 `include/media_transport.h`。在 LVGL 线程使用显式 `media_config_t` 调用 `media_open()`：指定后端回调、三首曲目的时长、曲目数、取消静音音量、语言、场景、总览选择和静音预览标记。加载返回的屏幕，将 `media_set_paused()` 绑定到外壳暂停/恢复，并在加载其他屏幕后删除原屏幕。服务方持有的状态和 PCM 缓冲区须比屏幕存活更久。播放调度按请求标识串行执行，等待对应的结束确认后才替换音频，并忽略旧完成事件。设备错误允许明确重试；无法确认开始/停止时会阻止播放直到重启。

`examples/media_clock.c` 提供明确静音的原生后端。`examples/media_device.c` 提供固件生命周期的工作任务，使用 `sugarat_audio`、静态 PCM 数据和有界命令/结束事件队列。将选定示例复制到应用，包含其头文件并与 main 一起编译。实物播放时，在 main 的 `REQUIRES` 添加 `sugarat_audio`，调用 `media_device_init()` 替代基础工程的 `sdgoods_audio_init()`，并使用 `media_device_backend()`。禁止同时初始化两个驱动。按 `components/sugarat_audio/README.md` 将音量设置保护应用到现有 BSP。音频回调不持有屏幕指针，也不调用 LVGL。示例所有者存活到固件结束，没有动态关闭 API。

使用 Python 3 在应用持有的目录生成 `media_tracks.h` 和 `media_tracks.c`，并将生成的 C 与 main 一起编译。生成器提供晨光、海风和夜行：三首原创八秒短曲，16 kHz、16 位单声道，峰值低于 7000。不需要外部媒体或 Python 依赖，写入前会拒绝所有输出冲突。将输出目录加入头文件路径。原生构建编译时钟示例而非设备示例，并设置 `silent_preview=true`。原生预览不会声称音频已正常工作。可选 WAV 输出使用相同曲目。

```sh
python3 components/sugarat_media/tools/generate-audio.py --output main/media-assets
python3 components/sugarat_media/tools/generate-audio.py --format wav --output media-preview
```

## 验证

在仅写入应用分区、独立摘要校验和音频所有者启动观察通过后，用户已在 ESP-IDF 5.5.4、LVGL 8.3.11 的谷仓圆屏徽章上验收此案例。主机测试覆盖中英文、三个视图、32 KiB LVGL 堆、圆屏控件边界、进度跳转、替换、静音、外部音量变化、单曲循环、旧完成事件、超时和屏幕释放。仓库的 `examples/music-media` 测试工程构建这些规范源码。每次新集成仍需独立构建、实物输入和音质验证。不提供网络、账号、录音、压缩音频解码器或文件导入。
