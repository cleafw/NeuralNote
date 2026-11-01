# 本项目的维护要求

- 用户的 Windows 自定义版必须支持 Vulkan GPU 加速，目标显卡是 NVIDIA GeForce RTX 3060 12 GB；禁止因 SDK 缺失而悄悄交付 CPU-only 版本。
- 使用 `Tools/build-windows-gpu.ps1` 编译，详见 `docs/WINDOWS_CUSTOM_BUILD.md`。完成后检查应用日志实际列出 RTX 3060，而不仅检查编译成功。
- 默认跟随系统语言，支持英文和简体中文；新界面、设置、窗口标题、提示均需接入词典。品牌、文件名、设备型号和技术缩写保留原样。
- JUCE 翻译补丁由 `Tools/patch_juce_localisation.py` 在配置时应用；必须把补丁规则保存在父仓库，不可仅依赖未提交的子模块改动。
- 项目在 `D:\Leaf\AI_Work\Project\MyProject\NeuralNote`；开发环境在 `E:\Environment` 或 `B:\Environment`。模型下载目录必须通用：首次下载检测磁盘，优先在非 C 盘、非系统盘创建 `NeuralNote/models`，再由用户确认或改选；不得写死任何个人目录。只有系统盘时回退用户文档目录。不要重新放回旧项目目录。
- 保留桌面 `NeuralNote 自定义版` 快捷方式及 `Tools/launch-desktop.ps1` 的启动路径。更新运行中的程序前检查是否正在转录；不要强制终止用户任务。
- 菜单与混音操作见 `docs/USER_WORKFLOW.md`。窗口拖动必须扩充内容区域，不能改变界面缩放；试听乐器与导出选择必须独立于转录乐器配置。验证本轮功能使用 `Tools/build-windows-gpu.ps1 -InteractionTests`，不要把测试中的合成音符写回用户会话。
- Windows 用户配置必须使用 `NNFileUtils::getNeuralNoteDirectory()` 返回的用户主目录 `.neuralnote`。Codex MSIX 会把子进程的 Roaming AppData 重定向到自身 LocalCache；不可再将模型位置或独立程序会话单独存回 AppData，否则桌面启动会读到不同配置。JUCE 独立程序配置路径补丁同样通过 `Tools/patch_juce_localisation.py` 保存。
