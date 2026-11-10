# 本地验证范围

验证日期：2026-11-09。运行平台：macOS arm64；固件使用已有的 ESP-IDF 5.5.4、Python 3.12 环境。完整资源包复制到父仓库之外的独立临时目录，再创建两个全新固件工程；编译没有读取父仓库的模板、组件或应用源码。SDK 和工具链仍作为外部环境使用。

## 本资源包

| 检查 | 结果 |
| --- | --- |
| `python3 scripts/kit.py check` | 校验分发文件 SHA-256、模板压缩包及其全部 486 个源码成员 |
| `python3 -B -m unittest discover -s scripts -p 'test_*.py'` | 8 项通过；包含依赖安装、已有文件保留、冲突时不写入、重复安装、主题兼容、符号链接拒绝、资源篡改拒绝和路径越界拒绝 |
| 两个独立 Python 测试进程 | 各使用私有临时目录，均通过，无共享工程或设备访问 |
| Skill 格式 | skill-creator 的 `quick_validate.py` 通过；校验器使用已有环境中的 PyYAML，资源包辅助脚本不依赖它 |
| 相对 Markdown 链接 | 全部 46 个 Markdown 文件的本地链接通过检查 |
| Watch 最小接入示例 | 独立新工程 `idf.py build` 通过；BIN 大小 1,450,704 字节 |
| Game 最小接入示例 | 独立新工程 `idf.py build` 通过；BIN 大小 896,000 字节 |

两次构建分别安装相应主题及 Pixel 依赖，将 `assets/starters` 中的对应入口复制到新工程，并在应用 CMake 加入对应 `REQUIRES`。其余功能组件保留源码和历史验证说明，本次没有逐个重新编译或执行全部组件测试。固件 BIN 没有放入公开资源包。

## 父仓库检查

`pnpm run build:artifacts`、其后的 `pnpm run build:check`、`pnpm run test:docs` 和 `pnpm run doc-sync` 通过。`pnpm run lint` 未通过，报错位于本次资源包目录之外，未修改那些文件。本目录没有新增 TypeScript 运行时代码。

## 未执行的验证

未执行串口发现、芯片检测、备份、烧录、字节验证或真机功能验收。未执行 Windows/Linux 的全新环境安装，未下载完整 SDK，也未在真实 GitHub 发布地址进行下载验收。公开地址由发布者确定后，按 [发布说明](references/publishing.md) 完成首次下载与交接验证。
