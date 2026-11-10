# 组件目录

在资源包根目录运行 `python3 scripts/kit.py components` 查看目录，使用 `components --id <id>` 读取一个组件的完整集成说明和历史验证范围。安装使用 `init --components ...` 或 `add --components ...`；依赖自动加入，应用入口需要显式接入。

| 组件 ID | 用途 | 源码与接口说明 | 依赖 |
| --- | --- | --- | --- |
| `sdgoods-case-control` | 开关、滑块和遥控本地模拟 | [sugarat_control](../assets/components/sdgoods-case-control/components/sugarat_control/README.md) | `sdgoods-ui-watch` |
| `sdgoods-case-data` | 仪表、历史和阈值告警页面；数据源自备 | [sugarat_data](../assets/components/sdgoods-case-data/components/sugarat_data/README.md) | `sdgoods-ui-watch` |
| `sdgoods-case-media` | 本地播放器、封面和控制页面 | [sugarat_media](../assets/components/sdgoods-case-media/components/sugarat_media/README.md) | `sdgoods-ui-watch`, `sdgoods-audio` |
| `sdgoods-case-companion` | 宠物、心情和语音状态页面 | [sugarat_companion](../assets/components/sdgoods-case-companion/components/sugarat_companion/README.md) | `sdgoods-ui-watch` |
| `sdgoods-case-time` | 专注、倒计时、秒表和手动时钟 | [sugarat_time](../assets/components/sdgoods-case-time/components/sugarat_time/README.md) | `sdgoods-ui-watch` |
| `wifi-provisioning` | Wi-Fi 扫描、连接、保存和热点配网 | [sugarat_wifi](../assets/components/wifi-provisioning/components/sugarat_wifi/README.md) | 无 |
| `sdgoods-wifi-ui` | 圆屏中英文 Wi-Fi 配网页面 | [sugarat_wifi_ui](../assets/components/sdgoods-wifi-ui/components/sugarat_wifi_ui/README.md) | `wifi-provisioning` |
| `sdgoods-imu` | QMI8658 运动传感器 | [sugarat_imu](../assets/components/sdgoods-imu/components/sugarat_imu/README.md) | 无 |
| `sdgoods-battery` | 电池电压和估算电量 | [sugarat_battery](../assets/components/sdgoods-battery/components/sugarat_battery/README.md) | 无 |
| `sdgoods-audio` | 麦克风、扬声器、PCM 和语音流 | [sugarat_audio](../assets/components/sdgoods-audio/components/sugarat_audio/README.md) | 无 |
| `sdgoods-frame-stream` | 连续动画帧调度和面板直传 | [sugarat_frame](../assets/components/sdgoods-frame-stream/components/sugarat_frame/README.md) | 无 |
| `sdgoods-ui-watch` | Watch 风格 UI、中文字体和图标 | [sugarat_watch](../assets/components/sdgoods-ui-watch/components/sugarat_watch/README.md) | `sdgoods-pixel-characters` |
| `sdgoods-ui-game` | Game 像素风格 UI 和 RPG 控件 | [sugarat_game](../assets/components/sdgoods-ui-game/components/sugarat_game/README.md) | `sdgoods-pixel-characters` |
| `sdgoods-pixel-characters` | 20 个动画像素角色 | [sugarat_pixel](../assets/components/sdgoods-pixel-characters/components/sugarat_pixel/README.md) | 无 |
| `https-client` | 受限 HTTPS 请求与退避 | [sugarat_https](../assets/components/https-client/components/sugarat_https/README.md) | 无 |
| `deotaland-agent-link` | Deotaland 激活和实时语音；需用户产品配置 | [sugarat_agent_link](../assets/components/deotaland-agent-link/components/sugarat_agent_link/README.md) | `https-client`, `sdgoods-audio` |

## 集成约束

每个组件保留中英文 README、公开头文件、源码、示例和随附许可。界面组件需要 LVGL 线程；外设组件的任务回调不能直接修改 LVGL。音频组件替代模板原有音频初始化，不能让两者同时占有 I²S。所有者释放和回调时机以对应 README 为准。

Watch/Game 二选一；当前五类完整案例依赖 Watch，不能只改一个主题名字就当作 Game 案例。案例自带的测试画廊不应成为独立应用的默认导航。

Agent Link 还需要用户配置实际产品 ID、注册芯片类型和账号接入；资源包不含凭据或云端权限。语音状态演示不代表已接入真实语音。控制案例是本地模拟，数据案例没有默认真实传感器提供者。

组件元数据保留上游的历史构建、预览和实物验证范围。它们不代表本资源包已执行这些设备测试；本次验证见 [VALIDATION.md](../VALIDATION.md)。每个新应用仍需自行编译和真机验收。
