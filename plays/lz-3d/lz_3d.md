# lz 实时 3D 角色固件

基于 SDGOODS ESP32-S3 圆屏基础工程（资源包 sugarat-sdgoods 0.2.0）和 `sdgoods-frame-stream` 组件。设备上实时渲染 lz 角色：800 面模型、256×256 贴图、4 段骨骼动画。每次开机先播放一次带声音的开机动画。

## 开机动画

- 每次开机播放一次：5 秒、24 帧/秒、360×360，同时播放音效。从控制中心返回不会重播。
- 画面按声音实际出声的时刻对齐；500 ms 内声音没有开始时，画面按自己的时钟播放。
- 播完用 0.4 秒从最后一帧交叉淡化到 3D 角色。播放期间触摸不起作用，也不能跳过。
- 音效按片段自身电平播放（峰值 8000，与平台自带音效相近），不随音量档位缩放；音量为 0 时功放不打开。
- 播完释放解码用的内存：PSRAM 约 506 KB（当前帧和解压缓冲），内部 RAM 约 11 KB（解压器状态），以及播放声音的任务栈。串口日志 `boot anim: ... before` 和 `boot anim released` 两行给出前后剩余内存，`boot anim: done` 给出每帧解码耗时。
- 数据 `main/bootanim/bootanim.bin`（约 0.85 MB）随固件一起放在 Flash。

## 操作

- 左右拖动：转身 360°，松手后带惯性减速。
- 上下拖动：俯仰最多约 ±29°，松手后回正。
- 点一下：依次播放 hello、listen、talk，播完回到 idle 循环；动作切换有 250 ms 过渡。
- 顶部下滑：控制中心（模板原有）。期间 3D 画面暂停，关闭后恢复。
- 底部一行显示最近 2 秒的帧率和平均每帧耗时；串口每 2 秒输出 `lz3d: fps=...` 明细。

## 源码改动

| 文件 | 内容 |
| --- | --- |
| `main/apps/ui_helloapp.c` | 应用：触摸、惯性、动画状态、按帧直推 |
| `main/render/lz3d.c`、`lz3d.h` | 分条软件渲染器，纯 C，可在电脑上编译 |
| `main/model/lz_model.h`、`lz_model_data.c` | 导出的模型数据（`lz_model_data.c` 由脚本生成，不要手改） |
| `main/bootanim/bootanim.c`、`bootanim.h` | 开机动画解码：逐帧异或差分 + ROM 解压，IMA ADPCM 声音，纯 C，可在电脑上编译 |
| `main/bootanim/bootanim.bin` | 开机动画数据（`tools/encode_bootanim.py` 生成，不要手改） |
| `components/sdgoods_board/src/sdgoods_audio.c`、`include/sdgoods_audio.h` | 增加一次性声音片段播放 `sdgoods_audio_clip_start()`；片段播放时 BGM 和录音回放返回 `ESP_ERR_INVALID_STATE` |
| `main/main.c` | 只改注释：开机动画改由本应用播放 |
| `main/CMakeLists.txt` | 加入以上源码、嵌入开机动画数据、`sugarat_frame` 依赖和编译选项 |

## 性能优化

1. **动画预烘焙**：骨骼和形变动画在电脑上按 24 帧/秒烘焙成顶点坐标（int16 量化，约 1.1 MB，放在 Flash）。设备不计算骨骼，每帧只在相邻两帧之间线性插值。
2. **正交投影**：贴图坐标和深度在屏幕上呈线性，每个像素只做三次定点加法，不做除法。
3. **背面剔除**：两个大部件剔除背面，约少画 35% 的三角面；开口较多的小部件双面绘制。
4. **分条渲染**：按 12 行一条渲染。深度缓冲只需一条大小（8.6 KB），放在内部 RAM。三角形每帧先按条分箱，每条只处理落在里面的三角形。
5. **双核并行**：每条分成 4 段列，由两个核抢占式领取。渲染下一条的同时，推屏组件在传送上一条。
6. **只推变化区域**：每帧只推角色包围盒（并上一帧的包围盒），不推整屏。
7. **内存分层**：推屏条带最先申请内部 DMA RAM；128 KB 贴图和 64 KB 三角形参数放进 Octal PSRAM（缓存未命中时比 DIO Flash 补得快）；顶点、分箱等约 23 KB 的小数组优先放内部 RAM。启动日志 `before init` 和 `after init` 两行给出实际剩余内存。
8. **热路径**：扫描线内循环放进 IRAM，渲染器用 -O3 编译，内循环不调用 libm。
9. **固定节拍**：`sugarat_frame_pacer` 按 60 Hz 网格放行帧。帧耗时在 16.7–33.3 ms 之间时稳定在 30 fps，不忽快忽慢；运动按真实时间计算，与帧率无关。

## 重新导出模型

```sh
blender --background --factory-startup --disable-autoexec \
  --python ../tools/export_model.py -- <lz.blend> main/model 800 256
```

参数依次为源文件、输出目录、目标三角面数和贴图边长（2 的幂）。减面后的顶点绑定到原始高面数表面，所以原模型的骨骼和形变都会带到导出的每一帧。

`../tools/host_render.c` 在电脑上按设备同样的分条方式渲染多个角度和动画帧，并检查背面剔除前后的像素差异。编译命令见文件开头。

## 重新生成开机动画

```sh
python3 ../tools/encode_bootanim.py ../assets/bootanim.mp4 main/bootanim/bootanim.bin
```

视频需为正方形、整数帧率，有音轨时一并转换。处理步骤：缩放到 360×360、降噪、圆屏外的四角填背景色、转成 RGB565。第 0 帧存整帧，之后每帧只存与上一帧不同的矩形区域的异或差分，再用 deflate 压缩。设备用 ESP32-S3 ROM 里的解压函数解码。声音转成 16 kHz 单声道，滤掉 150 Hz 以下的低音，峰值归一到 8000，再压成 4 位 IMA ADPCM。`--threshold` 控制多小的颜色变化视为不变（默认 2），调大文件更小，但背景容易出现斑块。

`../tools/host_bootanim.c` 用设备同一份解码代码在电脑上解出全部帧和声音，与生成时记录的校验值（`bootanim.bin.crc`）逐一比对，并输出若干帧 PPM 和 `audio.wav`。编译命令见文件开头。

## 固件大小

这台设备上的出厂区只有 3 MB（设备分区表是启动器 2.0 的多应用布局），而工程的 `platform/partitions.csv` 允许 15.875 MB，所以编译不会提示超限。烧录前确认 `build/lz_3d.bin` 小于 3,145,728 字节。目前固件约 2.75 MB，剩约 255 KB。

