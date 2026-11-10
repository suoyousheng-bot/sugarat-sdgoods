---
description: "Sugarat 电台的操作方式、美国服务选择和二开参数。"
---

# Sugarat 电台玩法参考

[English](PLAY.md) | 中文

## 概述

面向单点触摸 ESP32-S3 圆屏徽章的网络电台。首次进入 Sugarat 配网页面，取得 IP、保存凭据并关闭配网热点后自动进入电台。已保存的 Sugarat 网络重启后重新连接，不导入上游配网凭据。默认英文，保留语言切换。屏幕、触摸、电源和音频接线沿用已核对的上游 BSP。

## 目录

- [操作](#controls)
- [美国电台与时间](#us-stations-and-time)
- [可改参数](#customization)
- [发布验收](#release-acceptance)

<a id="controls"></a>
## 操作

主屏左右滑切换 13 套皮肤并保存选择。左右按钮切台，中间按钮暂停、恢复或重试失败的流。上滑或点击列表图标浏览电台；从列表打开设置，选择 Wi-Fi、美国地区、外观、休眠和刷新。顶部下拉打开 BSP 控制中心。换皮肤保持播放；切地区刷新目录并保留当前音频流。Wi-Fi 设置在新连接成功保存后，或用户主动返回时，回到原有电台页面。

<a id="us-stations-and-time"></a>
## 美国电台与时间

目录先读取已配置的 HTTPS 精选 JSON 数组，其次查询限定 `countrycode=US` 的 [Radio Browser](https://docs.radio-browser.info/)，最后使用[内置开发电台](main/stations-us.json)。选择城市和州后先按该州内的城市名称搜索，结果不足时扩展到该州和全美；不保证每个结果都在所选城市广播。不发送 IP 定位请求。托管精选清单有效时使用其排序，并优先于地区查询。两个远端来源仅接受 HTTP(S) 的 MP3/AAC/AAC+，拒绝已知播放列表、HLS 和其他编码，按 URL 去重，上限 64 台。响应损坏、不完整或过大时回退。内置台仍然需要联网。

开发预设包括 [KEXP 官方 160K AAC-LC 流](https://www.kexp.org/mobile/kexp-livestreams/)和两个 [Radio Paradise 流](https://radioparadise.com/listen/stream-links)。电台名称保留各自品牌。已知美国呼号使用配置的 FM 标识；纯网络台显示频道编号，不编造频率。KEXP 的西雅图标识为 [90.3 FM](https://www.kexp.org/about/)，KCRW 的洛杉矶标识为 [89.9 FM](https://www.kcrw.com/pages/about-kcrw)，[WNYC FM](https://wnyc.org/about) 与 WNYC AM 区分。显示频率仅标识广播台；设备通过互联网收听，不接收 FM 射频。

地区选择包括全美和 16 个城市与州组合。各地区 POSIX 时区包含夏令时规则，凤凰城和檀香山使用固定偏移。全美模式默认美国东部时间。SNTP 使用 `time.cloudflare.com`，未获得时间时显示 `--:--`。地区和时区由用户手动选择，不从网络推断位置。

<a id="customization"></a>
## 可改参数

| 参数或文件 | 默认值 | 作用 |
| --- | --- | --- |
| `CONFIG_SUGARAT_RADIO_CURATED_URL` | 空 | 精选 JSON 数组的 HTTPS 地址，托管审阅后的清单再配置。 |
| `CONFIG_SUGARAT_RADIO_BROWSER_URL` | `https://de1.api.radio-browser.info` | 电台目录服务器，可配置可用的 Radio Browser 镜像。 |
| `CONFIG_SUGARAT_RADIO_DEFAULT_TIMEZONE` | 美国东部 POSIX 时区 | 全美模式下的时钟时区。 |
| `CONFIG_SUGARAT_WIFI_US_CHANNELS` | 开启 | 扫描和重连前设置美国信道 1–11。 |
| `CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM` | 8 | 开发版发射功率上限，收信与供电仍需真机验证。 |
| `main/stations-us.json` | KEXP 和两个 Radio Paradise 流 | 内置开发兜底清单，也是托管接口格式示例。 |
| `main/radio_region.c` | 17 项 | 地区名称、查询词和时区，选择器最多支持 20 项。 |
| `main/apps/ui_radio_skin.c` | 13 套 | 皮肤名称、颜色与注册表，保持已保存偏好使用的 ID 稳定。 |

偏好存入 NVS；源码、清单和分发产物不得含凭据。固件不通过擦除 NVS 恢复初始化错误，启动时不挂载或格式化 appdata。Sugarat 网络组件替代上游 Wi-Fi 驱动初始化；不要再启动 BSP Wi-Fi 管理器或第二个 I2S 所有者。继续二开时保留 `NOTICE`、源码版权和修改声明。

<a id="release-acceptance"></a>
## 发布验收

桌面渲染和编译结果在 [VALIDATION.md](VALIDATION.zh.md) 中分别记录。真机验收需要覆盖两种配网方式、密码错误恢复、重启重连、配网热点关闭、13 套皮肤、切台、MP3/AAC 声音、断网恢复、音量和静音、休眠及物理电源行为。长时间播放和实际美国网络可达性仍需要设备与网络验证。商业发布还需要真实精选托管地址和已审阅的电台清单；源码许可不授予音频流使用权。
