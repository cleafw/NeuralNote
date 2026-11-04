# 多平台社区发布

工作流：`.github/workflows/ci_build.yml`（Build and publish installers）。无需 Apple 付费账号或个人访问令牌；发布使用当前仓库的短期 `GITHUB_TOKEN`。

## 发布步骤

1. 修改 `CMakeLists.txt` 中版本号及 Windows `neuralnote.iss` 的 AppVersion，确认一致。每次公开发布使用新版本号。
2. 更新 `docs/RELEASE_NOTES.md`，提交并推送到自己的仓库。
3. 推送对应轻量标签，例如 `git tag v2.1.1` 后 `git push origin v2.1.1`；或在 Actions 中手动运行默认分支的工作流。手动运行会从 CMake 读取版本并创建标签。
4. 准备任务创建草稿和含固定子模块的完整源码包。四个构建任务分别制作 Windows x64、Mac arm64、Mac Intel、Linux x64 包。
5. 所有构建成功，且发布任务确认必需附件齐全后，生成 SHA256 校验清单，并自动公开 Release。任一任务失败则保持草稿，可查看失败任务、修复后使用新标签，或对同一源码重跑。

已经公开的同版本禁止覆盖；先升版本再发布。重跑同一草稿只接受其标签所指向的同一个提交，避免混合不同源码的包。

## 构建与包

- Windows：clang-cl + Vulkan SDK，必须开启 `MUSCRIPTOR_VULKAN` 和 `NEURALNOTE_REQUIRE_GPU`，关闭 ASIO。生成 Inno Setup 安装包（独立程序、VST3、可选桌面图标），并允许自选安装位置。
- Mac：Metal，两个架构分别编译。使用 `codesign --sign -` 临时签名并验证，不访问钥匙串里的付费证书，不执行苹果公证。生成未公证 `.pkg` 和 `.dmg`；pkg 包含独立程序/AU/VST3，dmg 仅提供独立程序。
- Linux：Ubuntu 24.04 + Clang 20 + Vulkan，生成独立程序/VST3 的 `.deb` 和带用户级安装脚本的 `.tar.gz`。依赖版本由 `dpkg-shlibdeps` 推导；不宣称兼容全部 Linux 发行版，不宣称是自包含 AppImage。
- 所有构建都执行翻译覆盖和本地化测试。普通云端 runner 无实际 RTX 3060，不能把编译成功解释为所有 GPU 的真实转录验证；本机交互回归仍用 `Tools/build-windows-gpu.ps1 -InteractionTests`。
- 二进制按 JUCE 的 AGPLv3 开源授权路径发布，保留原项目及依赖许可，并提供含子模块、JUCE 补丁的完整对应源码。模型不打包，保持原有 CC BY-NC 4.0 下载源和许可。

维护入口：`.github/scripts/configure_and_build.py`（各系统构建）、`package_windows.py`（Windows）、`package_release.py`（Mac/Linux）、`prepare_release.py`（版本与源码）、`publish_release.py`（附件检查与公开）。

本机脚本仍可使用维护者的环境目录；云端脚本不得依赖本机 B/E 盘路径。打包和发布不会关闭用户正在使用的软件或修改模型目录。

## 首次发布记录

2026-11-04：`v2.1.1` 已公开，Windows x64 Vulkan、Mac arm64、Mac x64、Linux x64 四个平台全部构建及打包成功，254 条翻译覆盖检查通过。Release 包含安装包、Windows 调试符号、完整源码及 SHA256 校验清单。运行记录：https://github.com/cleafw/NeuralNote/actions/runs/41823248999 。这不替代真实 Mac/Linux 桌面、显卡或 DAW 宿主的运行验证。
