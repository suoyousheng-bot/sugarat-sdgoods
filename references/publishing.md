# 将资源包发布为 GitHub 仓库

本页面向发布者。公开资源仓库为 [suoyousheng-bot/sugarat-sdgoods](https://github.com/suoyousheng-bot/sugarat-sdgoods)，固定版本为 `v0.2.1`。发布分支或其他仓库时，把 `sugarat-sdgoods` 目录的完整内容作为仓库根目录；不要上传父工程、SDK 安装目录或个人设备备份。目录内的相对链接和脚本无需父仓库。

## 发布内容

保留 AGENTS.md、SKILL.md、README 双语文件、COPY_TO_AGENT.txt、agents、boards、references、scripts、assets、resources.lock.json、checksums.sha256、LICENSE、NOTICE.md、VALIDATION.md、licenses、plays、plays.json、media 和 `.gitignore`。模板压缩包内是源码，不能省略；它的配套 JSON 清单也必须保留。完整 SDK 不在包中，官方固定版本的安装入口见 [环境说明](environment.md)。

## 本地检查

```sh
python3 scripts/kit.py check
python3 -B -m unittest discover -s scripts -p 'test_*.py'
```

核对 [许可归属](../NOTICE.md) 及随附的原始声明。检查公开目录中没有凭据、用户备份、设备身份记录和本机路径。校验清单只能检测文件变化，不能建立来源可信度；发布者应使用其正式仓库身份和固定版本。

## 交接文案

为实际创建的仓库选择 tag 或提交，再运行：

```sh
python3 scripts/kit.py prompt --repo <repository-url> --ref <release-tag-or-commit> --task <application-request>
```

参数中的仓库 URL 必须是实际目标地址，后两个参数分别是固定版本和用户需求。输出是一行中文，可供产品复制按钮使用；它不联网、不发布也不验证仓库是否存在。默认文案 [COPY_TO_AGENT.txt](../COPY_TO_AGENT.txt) 已固定到公开仓库的 `v0.2.1` 入口，可直接复制；用户随后描述应用需求。发布到其他仓库或更新版本时，使用上面的命令重新生成文案。

若将具体公开文案写回资源包，或更新模板、组件、说明，运行 `python3 scripts/update_checksums.py` 重新生成 `checksums.sha256`，再运行校验和测试。清单格式为逐文件的 SHA-256、两个空格和相对路径，覆盖根目录分发文件及资源目录，但不包括清单本身、`.git`、Python 缓存和用户生成内容。模板更新还需同时更新原始模板清单及版本，避免混用不同来源的文件。

发布者负责上传并确认读取权限。通过发布后的真实 tag 或提交下载完整源码包，在独立目录运行校验及初始化，确认公开读取和资源完整性；本地验证不能代替真实 GitHub 下载验收。
