## NeuralNote 社区版 / Community edition

中文与英文共用同一程序，默认跟随系统语言。模型需在软件内单独下载，不包含在安装包里。

### 选择下载文件

| 系统 | 文件 | 内容 |
| --- | --- | --- |
| Windows x64 | `NeuralNote_Installer_Windows_x64.exe` | 独立程序 + 可选 VST3，保留 Vulkan GPU，无需安装开发用 Vulkan SDK；未做代码签名 |
| Mac Apple 芯片 | `NeuralNote_Mac_arm64_unnotarized.pkg` / `.dmg` | pkg 安装独立程序、AU、VST3；dmg 提供拖入 Applications 的独立程序 |
| Mac Intel | `NeuralNote_Mac_x64_unnotarized.pkg` / `.dmg` | Intel 架构，包内容同上 |
| Linux x64 | `NeuralNote_Linux_x64.deb` | Ubuntu 24.04 构建的独立程序 + VST3，含 Vulkan 后端；兼容性以依赖版本为准 |
| Linux x64 | `NeuralNote_Linux_x64.tar.gz` | 同一构建的安装目录与用户级安装脚本，需要系统依赖，不是 AppImage |

Mac 两种架构均为免费证书方案：使用临时签名（ad-hoc），**没有 Apple Developer ID 签名或公证**。系统可能阻止首次打开；确认来源后参考 [苹果说明](https://support.apple.com/zh-cn/102445)，在系统设置 → 隐私与安全性中选择“仍要打开”。受管理电脑的策略可能不允许。

Linux 推荐通过 `sudo apt install ./NeuralNote_Linux_x64.deb` 安装，使包管理器处理依赖。不是面向所有发行版的通用二进制。GPU 需要兼容的 Vulkan 显卡驱动；没有合适设备时可选择 CPU。

`*_PDBs.zip` 为 Windows 调试符号，普通用户无需下载。`source-with-submodules.tar.gz` 包含完整源码、固定版本子模块及 JUCE 补丁；`SHA256SUMS.txt` 可核对下载文件。

### 本版内容

- 修复模型下载目录不够明确的问题。点“下载到…”可直接输入完整路径（例如 `G:\NeuralNote\models`）或浏览选盘。确认前只显示建议路径，确认时检查剩余空间。重试或继续下载也可改选目录。
- 简体中文/英文界面、统一菜单和独立界面缩放。
- 原声与乐器独立播放、音量、声相、静音、独奏和选定音色试听。
- 乐器多选、分轨/批量 MIDI 导出、重新生成 MIDI。
- 波形拖动定位、钢琴窗中键平移及 F11 全屏。
- 应用更新只检查本仓库 Releases。

### License and verification

v2.1.2 model downloads display an editable destination before starting. Type an absolute path or browse to another drive, then confirm. The download checks the space needed in the chosen folder, including partial downloads available there.

This community binary distribution uses JUCE under AGPLv3; complete corresponding source is attached, and original component notices are retained. MuScriptor model weights are downloaded separately under CC BY-NC 4.0.

Builds run compiler and localization checks. Cloud runners do not verify transcription on every physical GPU, speaker output, or every DAW host. Report problems with OS, GPU, model and reproduction steps at https://github.com/cleafw/NeuralNote/issues .

English: Windows includes Vulkan and standalone/VST3. Mac packages target Apple Silicon or Intel and are **ad-hoc signed, not notarized**; `.pkg` includes standalone/AU/VST3 and `.dmg` contains the app. Linux packages target Ubuntu 24.04 x64 with system dependencies. Models download separately. See [English README](https://github.com/cleafw/NeuralNote/blob/main/README_EN.md).
