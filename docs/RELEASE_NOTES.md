## NeuralNote 社区版 / Community edition

中文与英文共用同一程序，默认跟随系统语言。模型不包含在安装包里，可在软件内下载，或下载完整的 GGUF 文件后在本地加载。

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

- **自动测速**：导入音频后自动估算 BPM，不必先转录。配器或力度变化使整段节拍不够突出时，会分段检测；多个片段指向相近速度时给出估算。
- **跟拍测速**：在波形上方点「测速」，可重新分析，或跟着主拍点至少 4 下；也可直接编辑 BPM。BPM 会写入导出的 MIDI，不改变原音频播放速度。已有会话会保留保存的 BPM。
- **亮暗主题**：「选项 → 主题」可切换亮色或暗色。默认暗色，选择会保存。

### License and verification

v2.1.4 estimates BPM when audio is imported and compares sections if the overall beat is unclear. Open **Measure** above the waveform to reanalyse or tap at least four beats, or edit BPM directly. The chosen BPM is written to exported MIDI and does not change audio playback speed. Choose a persistent light or dark theme under **Options → Theme**.

This community binary distribution uses JUCE under AGPLv3; complete corresponding source is attached, and original component notices are retained. MuScriptor model weights are downloaded separately under CC BY-NC 4.0.

Builds run compiler and localization checks. Cloud runners do not verify transcription on every physical GPU, speaker output, or every DAW host. Report problems with OS, GPU, model and reproduction steps at https://github.com/cleafw/NeuralNote/issues .

English: Windows includes Vulkan and standalone/VST3. Mac packages target Apple Silicon or Intel and are **ad-hoc signed, not notarized**; `.pkg` includes standalone/AU/VST3 and `.dmg` contains the app. Linux packages target Ubuntu 24.04 x64 with system dependencies. Models download separately. See [English README](https://github.com/cleafw/NeuralNote/blob/main/README_EN.md).
