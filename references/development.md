# 从基础工程开发应用

以下命令在完整资源包根目录运行。资源包本身是参考资料和源码集合；固件工程放在另一个目录。先查看用户已有项目，避免把基础工程覆盖到用户代码上。

## 新工程

```sh
python3 scripts/kit.py check
python3 scripts/kit.py init --directory ../my-badge --name my_badge --components sdgoods-ui-watch
```

目标目录必须不存在。项目名以 ASCII 字母开头，最多 31 个字母、数字或下划线。脚本校验模板及组件，安装依赖后创建工程；已存在的目标不会被替换。文件系统写入失败会报告保留的工程位置，检查后再处理，不自动删除工程。中文界面通过主题字体和应用文本实现，工程名无需使用中文。

Game 风格将上面的组件改为 `sdgoods-ui-game`。两套主题都需要 Pixel 角色组件，脚本会自动加入。只有基础工程时可省略 `--components`。按用户需求选择 [组件或完整案例](components.md)；现有案例依赖 Watch，安装器会拒绝同时选择 Watch 和 Game。

## 已有工程

```sh
python3 scripts/kit.py add --directory ../my-badge --components sdgoods-case-time
```

目标必须是含 `components/sdgoods_board` 的 ESP-IDF 工程。先核对其板卡、SDK、LVGL、BSP 版本和本包兼容；安装器不替代这项判断。已存在且相同的文件复用，不同内容或符号链接会阻止安装，且在已知冲突检查完成前不写任何组件。意外 IO 失败会报告已写文件，保留它们供检查或同版本重试。

脚本只安装源码，不修改 `main/CMakeLists.txt`、应用入口、用户配置和凭据。为所用组件加入 README 指定的 `REQUIRES`，显式接入初始化、回调和释放。已有工程需要合并组件修改时由 Agent 阅读差异并保留用户定制，不用整目录覆盖。

## 应用入口与主题

修改 `main/apps/ui_helloapp.c`，保留 `ui_helloapp_start()` 和 `ui_helloapp_poll()` 接口；新增源码时同步 `main/CMakeLists.txt` 和应用注册。BSP 由 `main/main.c` 初始化。先创建屏幕、`lv_scr_load()`，再绑定 `sdgoods_app_shell_bind()`；继续保留退出、暂停和恢复行为。不要从应用调用 launcher 的宿主清理逻辑。

Watch 对应 `components/sugarat_watch/include/sugarat_watch.h` 和 `examples/sw_gallery.c`；Game 对应 `sugarat_game.h` 和 `examples/sg_gallery.c`。两者均在初始化 LVGL 后调用各自的 `sw_init()` 或 `sg_init()`，并在 LVGL 线程组合界面。例子包含画廊创建函数；启用画廊时还需将对应示例 `.c` 和头文件目录加入应用 CMake。独立应用只取需要的页面，案例设置 `gallery_navigation=false`。

两套最小应用接入示例放在 [Watch](../assets/starters/watch/ui_helloapp.c) 和 [Game](../assets/starters/game/ui_helloapp.c)。它们只演示主题页面和本地按钮反馈。确认目标是新建工程后，可将选定示例复制到其 `main/apps/ui_helloapp.c`，并在 `main/CMakeLists.txt` 的 `REQUIRES` 加入 `sugarat_watch` 或 `sugarat_game`。初始化器不会替用户执行这次替换。

原始模板的注释和说明保留上游工具及平台名称；这些平台工具没有随本包分发。本包操作入口是 `scripts/kit.py`，不调用 Sugarat `hardware_*` 工具或上游市场脚本。完整基础工程仍保留 LICENSE、NOTICE、LICENSING.md 和 TRADEMARK.md。

## 单一应用镜像

一个应用只交付一个 ESP-IDF 应用镜像 `build/<project-name>.bin`。社区发布、启动器在线安装和电脑一键烧录都按这个约定处理。

- 引导程序、分区表和 otadata 是平台通用文件，由一键烧录工具和启动器提供。不要修改 `platform/partitions.csv` 和引导程序配置，也不要让应用依赖新增分区。
- 不要增加需要单独烧写的 bin，例如 SPIFFS、FAT 或 LittleFS 镜像，以及字体、模型、音频和资源包 bin。启动器按镜像大小给每个应用分配一段连续空间，只写入这一个镜像，其他 bin 没有安装位置。
- 资源编进应用镜像（C 数组、`EMBED_FILES` 或 `EMBED_TXTFILES`），或在首次运行时下载到 `sdgoods_appdata_begin(<app_id>, ...)` 返回的应用数据目录。该目录位于共享的 16 MiB appdata 分区，按 app_id 隔离，安装和替换应用都会保留。函数返回 `ESP_ERR_NOT_FOUND` 表示设备没有 appdata 分区，应用改用 NVS，或在不保存数据时也能运行。
- 小配置先调用 `sdgoods_nvs_ensure()`，再打开应用自己的 NVS 命名空间。NVS 由所有应用共用，不调用 `nvs_flash_init()` 或 `nvs_flash_erase()`，不读写其他应用的命名空间。
- 单应用烧录时镜像最大 0xFE0000 字节。启动器安装占用 `向上取整(镜像大小 + 56 KiB, 64 KiB)`，所有已安装应用共用约 10 MiB；镜像越大，同时能安装的应用越少，大资源优先放应用数据目录。
- `esp_app_desc` 的项目名是应用身份，发布后保持不变。版本号与 `play.json` 一致，每次发布使用新的版本号。
- 启动器通过重启切换应用，运行时全部内存归当前应用。保留应用外壳的退出路径，由它调用 `sdgoods_return_to_launcher()`；应用不写 otadata、分区表或其他应用的空间。
- 每个应用都要能被启动器安装运行。启动器把镜像写进自己选定位置的 ota_N 槽并重启进入，应用不假定运行在出厂区、0x10000 地址或某个分区标签，不调用 `esp_ota_set_boot_partition` 或 OTA 更新接口。保留平台控制中心：`sdgoods_device_is_managed_app()` 为真时它显示返回启动器的 Exit，否则显示 Power。

## 编译

激活 [匹配环境](environment.md)，在新工程目录运行：

```sh
idf.py build
```

新工程通过 `sdkconfig.defaults` 声明 `esp32s3`。已有工程若目标不同，先解决目标和配置冲突，不对其盲目运行会重置配置的命令。构建目录默认是 `build`；应用产物是 `<project-name>.bin`，构建目录中的引导程序、分区表和烧录参数供本机烧录使用，发布时只交付应用镜像。编译失败时保留诊断，停止烧录。

固件构建不等于预览或真机运行。新增应用完成后按用户要求选择后续操作，设备操作见 [部署说明](deployment.md)。
