# Windows 自定义版：GPU 与中文

配置位置更新（2026-10-31）：Windows 当前统一使用 `<用户主目录>/.neuralnote`，日志在其 `logs/NeuralNote.log`。旧版的 Roaming AppData 会被 Codex MSIX 启动环境重定向，导致从桌面启动看不到模型位置。旧配置和模型路径自动迁移到新目录，模型文件保留原位。下文历史验证记录中的 `%APPDATA%/NeuralNote` 是旧位置。

此版本必须启用 **Vulkan GPU**，不是 CUDA。目标设备为 RTX 3060 12 GB，系统内存 64 GB。
CPU 后端仍保留用于兼容，但不能作为默认交付的唯一后端。

## 环境与编译

- Visual Studio 2022 C++ Build Tools（含 Windows SDK、CMake）。
- Vulkan SDK 1.4.350.0：`B:\Environment\VulkanSDK\1.4.350.0`，必须包含 `Bin/glslc.exe`、头文件、库与 SPIRV-Headers。
- Python：`E:\Environment\Python311\python.exe`。
- ggml 使用 muscriptor.cpp 固定版本；本机已缓存于 `B:\Environment\NeuralNote\ggml`。

从项目目录运行：

```powershell
./Tools/build-windows-gpu.ps1
# SDK 位置不同：
./Tools/build-windows-gpu.ps1 -VulkanSdk 'E:\Environment\VulkanSDK\1.4.350.0'
```

脚本导入 VS x64 环境，并显式设置 `MUSCRIPTOR_VULKAN=ON`、`NEURALNOTE_REQUIRE_GPU=ON`。
依赖缺失必须修复，不得改成 OFF 交付。SDK 只用于开发，显卡驱动提供运行时。
安装 SDK 可使用官方的 `copy_only=1` 模式，把文件放在指定环境目录，避免改系统路径和注册图层。

输出：`build/NeuralNote_artefacts/Release/Standalone/NeuralNote.exe`。
桌面快捷方式通过 `Tools/launch-desktop.ps1` 启动此文件。

## 交付检查

1. 运行本地化测试（编译脚本自动执行）。
2. 退出空闲的旧程序，再启动新程序；切勿强制结束转录中的任务。
3. 检查 `%APPDATA%/NeuralNote/logs/NeuralNote.log` 的 ComputeDevices 行包含 RTX 3060 / Vulkan。
4. 设置 → 计算设备中选择自动或 RTX 3060。自动选择应优先选择 GPU。
5. 已有模型时，用短音频做一次 GPU 转录；若没有模型，应明确记录仅验证到设备识别，不能声称转录已验证。

## 多语言

设置 → 语言提供跟随系统（默认）、English、简体中文。未支持的系统语言回退英文。
词典在 `Lib/Utils/NnChinese*.inc`；主界面使用 `nn::tr`，框架使用 JUCE `TRANS`。
JUCE 中写死的按钮由 `Tools/patch_juce_localisation.py` 自动修复，重复配置不会重复修改。
独立程序已打开的框架窗口会同步刷新语言；插件模式不修改宿主窗口。
新增翻译需检查英文→中文→英文切换、音频/MIDI 设置标题、选项菜单、高级设置与输入静音提示。
设备驱动返回的错误文本、设备名称、品牌及文件名不强行翻译。

## 通用模型下载目录

程序不得包含个人电脑专用的模型路径。点击下载时才准备默认目录并打开系统文件夹选择窗口：

1. 优先使用当前模型已经确认且仍可写的目录，保留已有模型及断点续传位置；其他型号确认过的目录也可复用。
2. Windows 首次使用时枚举磁盘，按盘符顺序选择非 C 盘、非 Windows 系统盘的本地固定硬盘。跳过不可访问、移动、网络、光盘及剩余空间不足的磁盘。
3. 在第一个可用磁盘创建 `NeuralNote/models`，如 `D:/NeuralNote/models`。盘符由实际电脑决定，本机也可能选择 B 盘。
4. 无合适磁盘时回退到用户“文档”目录下的 `NeuralNote/models`；该目录不可用时再尝试用户应用数据目录。
5. 文件夹选择窗口定位到上述位置，用户可自行改选。仅确认后才保存路径并开始下载；取消时可能保留刚创建的空文件夹，但不下载、不修改模型配置。

下载位置即使用位置，无需解压或再次安装。首次选址规则不影响已保存的自定义目录，也不自动迁移已有模型。

## 钢琴卷帘交互（2026-10-31）

- 在卷帘内按住鼠标中键，上下移动音高视图、左右移动时间视图；边界限制在可用时间/琴键范围内。手动平移会关闭自动跟随播放头，可用工具栏重新打开。
- 左键点击卷帘仍然定位播放头；左键按住左侧琴键试听钢琴音色，拖过其他琴键切换音高，松开、切到其他应用或关闭界面时释放声音。
- 试听使用独立合成器，经过总音量控制，不受原音/合成音混合比例和乐器静音控制影响，不启动播放、不改变转录音符、不输出 MIDI。
- 运行 `./Tools/build-windows-gpu.ps1 -InteractionTests` 验证中键平移、松开停止、边界、琴键命中和离线音频发声/衰减。

## 早期验证记录（2026-10-30）

- Release 独立程序编译成功；221 条翻译词典的覆盖检查、本地化与中英切换测试通过。
- 从桌面快捷方式启动成功，进程正常响应。
- 应用日志实际输出：`ComputeDevices: NVIDIA GeForce RTX 3060 (Vulkan), 11.8 GB`，并保留 CPU 后端。
- 当前没有已安装的转录模型，因此尚未进行完整音频转录验证；不得将设备枚举成功当作模型推理测试通过。
- 窗口自动化工具目前不可用，未完成逐屏视觉验收；此次已通过代码检查补齐独立窗口和音频设置的翻译入口。
