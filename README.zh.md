---
description: "使用独立的硬件资料、源码模板、UI 主题和 Agent Skill 开发 SDGOODS 圆形徽章固件。"
---

# Sugarat SDGOODS 开发资源包

[English](README.md) | 中文

## 摘要

把本资源包的 GitHub 入口链接交给常用 Agent，即可按说明为 SDGOODS ESP32-S3 360×360 圆形徽章开发固件。资源包包含基础源码模板、Watch/Game 主题、16 个组件包和硬件资料。使用时不依赖 Sugarat 或本资源包所在的上级仓库。ESP-IDF 和工具链需单独安装。

## 目录

- [交给其他 Agent](#give-this-kit-to-an-agent)
- [本地使用源码](#use-the-sources-locally)
- [资源和验证](#resources-and-verification)
- [发布](#publishing)

<a id="give-this-kit-to-an-agent"></a>
## 交给其他 Agent

复制 [COPY_TO_AGENT.txt](COPY_TO_AGENT.txt) 给常用 Agent，再描述想做的应用。文案指向固定的 [v0.1.0 Agent 入口](https://github.com/suoyousheng-bot/sugarat-sdgoods/blob/v0.1.0/AGENTS.md)；同版本完整资源包可从 [v0.1.0 源码 ZIP](https://github.com/suoyousheng-bot/sugarat-sdgoods/archive/refs/tags/v0.1.0.zip) 获取。接收方 Agent 先读 [AGENTS.md](AGENTS.md) 和 [SKILL.md](SKILL.md)，再下载该版本资源。支持 Skill 安装的工具必须保留整个目录，包括资源和脚本。

可用 `python3 scripts/kit.py prompt --repo <repository-url> --ref <release-tag-or-commit> --task <application-request>` 为实际发布地址生成一句话。此命令只生成文字，不发布仓库，也不检查访问权限；产品的复制按钮可以直接复制其输出。

<a id="use-the-sources-locally"></a>
## 本地使用源码

资源包辅助脚本需要 Python 3.9 或更新版本。以下命令在资源包根目录运行：

```sh
python3 scripts/kit.py check
python3 scripts/kit.py components --id sdgoods-ui-watch
python3 scripts/kit.py init --directory ../my-badge --name my_badge --components sdgoods-ui-watch
```

初始化会创建一个新的固件工程，并安装主题及其 Pixel 依赖。应用入口由 Agent 显式接入；目标目录已存在时拒绝初始化。已有兼容工程可用 `python3 scripts/kit.py add --directory ../my-badge --components sdgoods-case-time` 安装组件；脚本在写入组件前报告不同内容的文件并保留它们。

编译前阅读 [开发说明](references/development.md) 和 [环境说明](references/environment.md)。辅助脚本不会安装 SDK、打开串口、烧录或创建私有备份。设备操作遵循 [部署说明](references/deployment.md) 和用户授权。

<a id="resources-and-verification"></a>
## 资源和验证

[resources.lock.json](resources.lock.json) 记录版本、依赖、集成方法和上游验证范围。[checksums.sha256](checksums.sha256) 覆盖分发文件。原始模板压缩包及逐文件清单完整保留。[硬件资料](references/hardware.md) 说明文档参数；[组件目录](references/components.md) 指向包内源码和示例。[NOTICE.md](NOTICE.md) 说明许可归属。

编译、写入字节验证、启动和真机验收分别记录。历史验收只属于记录中的应用和设备，每次新集成都需要验证。本包不包含预编译固件、设备备份、凭据、完整 SDK 或 Sugarat 原生预览运行时。本地验证结果和其余平台检查范围见 [VALIDATION.md](VALIDATION.md)。

<a id="publishing"></a>
## 发布

公开仓库为 [suoyousheng-bot/sugarat-sdgoods](https://github.com/suoyousheng-bot/sugarat-sdgoods)，首个资源快照固定为 `v0.1.0`。发布到其他仓库时，将整个目录复制为仓库根目录，按 [发布说明](references/publishing.md) 检查文件并重新生成对应地址的交接文案。

## 开发备注

无。
