# Deotaland Agent Link

[English](README.md) | 中文

在谷仓圆屏徽章的 ESP-IDF 工程根目录下以 `target=esp-idf` 安装 `deotaland-agent-link`，安装器会一并安装依赖 `https-client` 和 `sdgoods-audio`。把 `sugarat_agent_link` 加入 `main/REQUIRES`。组件把设备连接到德塔（Deotaland）智能体：获取激活码、设备登录、创建实时语音会话，并通过 Opus 进行按住说话的语音对话。协议参照德塔公开的 Agent_link 设备接口，源码独立编写。

```c
/* app_main, after nvs_flash_init(), sugarat_audio_init() and before Wi-Fi starts */
ESP_ERROR_CHECK(sugarat_agent_cloud_start());
ESP_ERROR_CHECK(sugarat_agent_voice_init());
/* Wi-Fi event callback */
sugarat_agent_cloud_set_network(event == SUGARAT_WIFI_EVENT_CONNECTED);
/* LVGL timer: read status and drive the UI */
sugarat_agent_cloud_status_t cloud; sugarat_agent_cloud_status(&cloud);
if (cloud.phase == SUGARAT_AGENT_CLOUD_ONLINE && cloud.voice_supported) sugarat_agent_voice_open();
/* talk button */
sugarat_agent_voice_talk(pressed);
```

## 配置

在 `sdkconfig.defaults` 或 menuconfig 的 "Deotaland Agent Link" 中设置产品。`CONFIG_SUGARAT_AGENT_LINK_PRODUCT_ID` 是在德塔平台登记的产品 ID，为 0 时编译失败。`CONFIG_SUGARAT_AGENT_LINK_CHIP_TYPE`（默认 `esp32-s3`）必须与该产品登记的芯片一致：不一致时平台以 `chipTypeMismatch` 拒绝激活，状态为 `SUGARAT_AGENT_CLOUD_REJECTED`。`CONFIG_SUGARAT_AGENT_LINK_BASE_URL` 选择网关地址（默认 `https://bot.deotaland.cn`）。产品 ID 和登记芯片都要向用户询问，不要猜测。

Wi-Fi、两个 TLS 会话和显示共用约 156 KiB 内部 RAM。用 `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`、`CONFIG_MBEDTLS_DYNAMIC_BUFFER`、`CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` 和 `CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM` 把网络缓冲区和任务栈移到 PSRAM，并关闭 Wi-Fi 和 LWIP 的 IRAM 优化。完整配置见 `ip-agent` 应用的 `sdkconfig.defaults`。

## 激活和登录

`sugarat_agent_cloud_start()` 创建一个工作任务，负责平台会话。它等待 `sugarat_agent_cloud_set_network(true)`。首次启动时在 NVS 命名空间 `agent_link` 中保存一个随机 UUIDv4 序列号，并申请六位激活码（`SUGARAT_AGENT_CLOUD_CODE`，到期前 30 s 自动更换）。用户在德塔控制台输入激活码。工作任务每 3 s 查询一次绑定状态，并把返回的设备密钥存入 NVS。有密钥后进行认证，每 60 s 发送一次心跳。阶段依次为 `SIGNING_IN`、`NO_AGENT`（已认领但未绑定智能体，每 10 s 重试）、`ONLINE` 和 `FAULT`（退避 3–60 s）。`ALREADY_BOUND` 表示设备属于另一个账号。`sugarat_agent_cloud_forget()` 删除密钥并申请新激活码；密钥被吊销时也会自动这样处理。`revision` 每次变化都递增，`agent_epoch` 在绑定的智能体变化时递增。请求函数会阻塞在 HTTPS 上，只能在工作任务中调用。

## 语音对话

`sugarat_agent_voice_init()` 在 PSRAM 栈上创建上行、控制和接收任务。`sugarat_agent_voice_open()` 为绑定的智能体新建会话；平台拒绝新建时沿用最近的会话（`reused_conversation`）。它还会启动 `sugarat_audio` 流式音频，并用一次性授权连接实时 WebSocket（`tidb-link.realtime.v1`，上行 Opus 16 kHz，下行 Opus 或 PCM）。对话打开期间麦克风一直工作。`sugarat_agent_voice_talk(true)` 立即开始一轮，并包含按下前 300 ms 的声音；`talk(false)` 结束这一轮。250 ms 内松开的按压会被丢弃（`SUGARAT_AGENT_VOICE_ERR_TOO_SHORT`）。回复期间按下会打断并清空播放。连接就绪前或重连期间录下的语音会排队（最多 10 s），就绪后发送。连接断开后依次在 1、2、4、8 s 后用新授权重连，之后每 15 s 一次（`RECONNECTING`、`FAILED`）。

每轮使用客户端按住说话断句。20 s 没有回复的一轮回到空闲，并报告 `SUGARAT_AGENT_VOICE_ERR_NO_REPLY`。平台拒绝按住说话，或某个连接上的第一轮没有回复时，会话重新连接，并在本次对话剩余时间使用服务端断句，需要重说这一轮。`turn` 报告 `LISTENING`、`THINKING` 和 `SPEAKING`。播放已清空且回复结束，或回复音频停止到达 4 s 后，`SPEAKING` 结束。所有函数立即返回，可从任意任务调用，包括 LVGL；状态复制有锁保护。

## 音质

徽章功放按约四分之一满幅设计，因此回复音频经过峰值 9000 的限幅（增益约 0.4 s 恢复）和 [1 2 1] 低通。`sugarat_audio` 预缓冲 200 ms，播空后重新缓冲，网络抖动不会变成爆音。Wi-Fi 省电（`WIFI_PS_MIN_MODEM`）会让功放产生可闻的电流声：对话打开时设为 `WIFI_PS_NONE`，关闭时恢复 `WIFI_PS_MIN_MODEM`。用 `CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM=15` 限制发射功率。

## 密钥

设备密钥、连接令牌和授权 URL 只保存在 RAM 或 NVS 中，不会出现在日志里。日志只包含阶段、平台代码、会话 ID、帧计数和堆内存数据。除非部署启用 NVS 加密，NVS 不加密。不要把密钥编译进固件。

## 已知限制

已在谷仓圆屏徽章上用 ESP-IDF 5.5.4 验证，所用德塔产品登记的芯片是 ESP32-C3，因此需要上报 `esp32-c3`。观察到激活、绑定、按住说话、无欠载无杂音的播放，以及语音连接时约 48 KiB 的空闲内部堆。打包后的组件通过编译验证；在每台新设备上验证激活和语音。没有 OTA、文字对话、唤醒词或回声消除。不支持用户说话时同时播放回复的全双工模式。同一时间只运行一个对话。
