# Sugarat Wi-Fi 配网

[English](README.md) | 中文

面向 ESP-IDF 5.5.4 的扫描选网组件。安装时选择 `wifi-provisioning`、`target=esp-idf`，目录使用 ESP-IDF 工程根目录。组件不提供手动填写 SSID 或写死凭据。HTML 页面内置于设备，默认英文，通过语言按钮切换中文。

由应用或 BSP 初始化 NVS，初始化失败时不要擦除分区。在调用方组件的 `REQUIRES` 中添加 `sugarat_wifi`，通过 `sugarat_wifi_default_config()` 获取明确配置，再调用 `sugarat_wifi_create(&config, &wifi)`。组件管理默认 STA/AP 网络接口和 Wi-Fi 驱动，已有网络管理器时拒绝初始化。已有默认事件循环可以复用。应用必须先删除依赖的界面，再调用 `sugarat_wifi_destroy(wifi)`。

## 事件与应用接入

创建前设置 `config.on_event` 和 `config.user_data`。通知在网络工作线程执行，不持有状态互斥锁，并可能在 `sugarat_wifi_create` 返回前开始，因此须先准备好用户数据。需要保留事件时应复制内容；事件包含序号、连接回执、SSID、IP、保存及热点状态、错误及断开原因码，不包含密码。回调应快速返回，不得调用 LVGL 或销毁网络实例，并须处理非阻塞命令接口的繁忙结果。销毁返回后不再调用回调。圆屏 UI 另提供在 LVGL 线程执行的 `on_connected`，用于页面跳转。

| 事件 | 含义 |
|---|---|
| `SUGARAT_WIFI_EVENT_CONNECTING` | 开始连接或有界重试。 |
| `SUGARAT_WIFI_EVENT_CONNECTED` | 已获得 DHCP 地址、保存凭据并关闭热点、DNS 和 HTTP，可进入应用。 |
| `SUGARAT_WIFI_EVENT_DISCONNECTED` | 工作任务观察到连接断开，后续可能重试。 |
| `SUGARAT_WIFI_EVENT_FAILED` | 连接失败，或重试次数、连接期限已耗尽。 |
| `SUGARAT_WIFI_EVENT_STORAGE_FAILED` | 已联网但凭据未提交，保留配网入口。 |
| `SUGARAT_WIFI_EVENT_PORTAL_OPENED` | HTTP 和强制门户 DNS 已就绪。 |
| `SUGARAT_WIFI_EVENT_PORTAL_CLOSED` | 配网服务及热点已关闭。 |
| `SUGARAT_WIFI_EVENT_PORTAL_FAILED` | 热点或服务关闭失败，不发布应用就绪成功事件。 |

快照保留 `last_event` 和 `last_connected`，供较晚创建的消费者读取。开始新连接或断线时会使保留的连接通知失效。`last_connected` 在清理完成后发布，具有独立序号；重复读取同一版本不代表产生新连接。打开 Wi-Fi 设置的消费者应忽略已处理的连接。保存和热点关闭失败须与密码错误分别处理，互联网连通检查由应用负责。

在调用方工程的 `sdkconfig.defaults` 中，将 `CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM` 设置为已在其硬件和供电条件下验证的值。默认值为 20 dBm，范围为 2–20 dBm，驱动会映射到支持的功率档位。组件在首次扫描或已保存网络重连之前应用该限制，并以四分之一 dBm 为单位记录驱动回读值。设置或读取限制失败会导致创建失败，并释放已取得的资源。

创建后自动扫描，或连接此组件已保存的网络。`sugarat_wifi_snapshot` 复制同步状态；`sugarat_wifi_scan`、`sugarat_wifi_connect`、`sugarat_wifi_portal` 和 `sugarat_wifi_forget` 将操作交给工作任务，不阻塞界面。连接参数为扫描结果编号、扫描代数和密码。过期选项、不支持的加密及无效密码长度会被拒绝；繁忙时应等待新状态后重试。最多展示 16 个不同的可见 SSID，同名网络使用扫描到的最强接入点。不支持隐藏网络、WEP、企业网络和 64 位原始十六进制 PSK，个人网络密码要求 8–63 字节。ESP32-S3 支持 2.4 GHz。选网标识的是 SSID，驱动会扫描全部信道并可选择同名网络的其他接入点，不固定 BSSID。WPA3 SAE 支持两种 PWE 方法，默认日志包含断开原因码，不包含密码。

热点仅在请求后开启。设备屏幕显示开放热点名称，手机无需输入热点密码，连接后打开 `http://192.168.4.1/`。页面自动扫描，用户选择网络并只填写密码。页面不自动聚焦输入框，使用 17px 输入字号，并请求固定视口缩放比例。HTTP 访问限制在热点子网内，也支持双栈服务端以 IPv4 映射 IPv6 地址表示的 IPv4 客户端，修改请求需要本次会话令牌和同源 JSON。服务不开放 CORS，不使用外部资源、统计、密码回读或凭据日志。手机提示无法上网时仍需保持热点连接。热点开启期间，DHCP 会发布热点 DNS 地址，IPv4 DNS 查询返回热点地址，HTTP 联网检测请求则通过带正文的响应重定向至统一配网地址，以支持 iOS 等客户端的强制门户检测。是否显示登录窗口由操作系统决定。组件不拦截 HTTPS。DNS 绑定至热点网络接口，热点关闭时在 TCP/IP 线程同步移除。

状态轮询、语言切换及重新扫描会保留所选 SSID 和密码草稿，前提是该网络仍可用且加密方式一致。改选其他网络或原选项消失时清空草稿。连接响应包含本次请求的回执，状态接口返回最近完成 DHCP 的回执。页面仅在该回执已连接且配置保存成功后清空提交的密码，已有连接或失败的尝试不会删除新输入。草稿仅保留在页面内存中。

空闲热点默认在 15 分钟后关闭。连接成功后先保存凭据，立即切换为仅 STA 模式，再移除 DNS 和 HTTP，最后发布 CONNECTED，不设置成功等待计时器。关闭的是配网热点，路由器连接保持有效。保存失败时保留配网入口并发布 STORAGE_FAILED；关闭失败时发布 PORTAL_FAILED，不发布 CONNECTED。iOS 配网窗口的退出由系统控制，普通 Safari 标签页无法被设备强制关闭。热点可能在网页最后一次轮询之前消失，因此提交连接后若未收到最终结果就失去通信，网页会引导用户查看设备屏幕。默认连接超时为 20 秒，最多重连三次。获得 DHCP 地址后才将凭据提交至 NVS 的 `sugarat_wifi` 命名空间；替换失败时保留上次有效配置。已连接但保存失败会明确显示。清除操作只删除此命名空间的网络键。组件不会自动启用 NVS 凭据加密，需要加密的应用应明确配置 NVS 安全方案，不应盲目修改 eFuse 或分区。

销毁会释放网络、HTTP 和事件循环资源，初始化失败会释放已取得的资源。连接成功表示取得局域网地址，不代表互联网服务可达。编译不能证明实物行为，必须在硬件上验证两种配网方式、错误密码恢复、重启保留、热点过期及无线性能。

实现参考：[ESP-IDF Wi-Fi 驱动](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-guides/wifi.html)、[Espressif 配网概述](https://developer.espressif.com/blog/2026/05/simple-provisioning/)。

Sugarat Radio 在 sdkconfig.defaults 中设置 8 dBm 发射功率上限，并新增默认开启的 `CONFIG_SUGARAT_WIFI_US_CHANNELS`。它在首次扫描和重连前以手动国家策略固定美国信道 1–11。
