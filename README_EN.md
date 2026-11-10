# NeuralNote — community edition

[简体中文](README.md) · **English**

Turn audio into instrument-separated MIDI, with local transcription, playback and export.

This is a community fork of [DamRsn/NeuralNote](https://github.com/DamRsn/NeuralNote), maintained by [cleafw](https://github.com/cleafw). It builds on NeuralNote v2 with Simplified Chinese localization, multitrack selection and export, independent source/instrument playback, MIDI regeneration, and desktop menu and window improvements. It uses [MuScriptor](https://github.com/muscriptor/muscriptor) through [muscriptor.cpp](https://github.com/DamRsn/muscriptor.cpp).

[Downloads and release notes](https://github.com/cleafw/NeuralNote/releases) · [Report an issue](https://github.com/cleafw/NeuralNote/issues) · [Detailed workflow (Chinese)](docs/USER_WORKFLOW.md) · [Windows GPU build notes (Chinese)](docs/WINDOWS_CUSTOM_BUILD.md)

## Direct downloads

**Chinese and English use the same installer.** Choose your computer below. These links always download this fork's latest stable release, currently [v2.1.2](https://github.com/cleafw/NeuralNote/releases/tag/v2.1.2).

| Your computer | Recommended download | Alternative / contents |
| --- | --- | --- |
| Windows 64-bit (x64) | [Windows GPU installer `.exe`](https://github.com/cleafw/NeuralNote/releases/latest/download/NeuralNote_Installer_Windows_x64.exe) | Standalone app and optional VST3, with Vulkan GPU support |
| Mac with Apple M-series chip | [Installer `.pkg`](https://github.com/cleafw/NeuralNote/releases/latest/download/NeuralNote_Mac_arm64_unnotarized.pkg) | [Standalone app only: `.dmg`](https://github.com/cleafw/NeuralNote/releases/latest/download/NeuralNote_Mac_arm64_unnotarized.dmg) |
| Mac with Intel processor | [Installer `.pkg`](https://github.com/cleafw/NeuralNote/releases/latest/download/NeuralNote_Mac_x64_unnotarized.pkg) | [Standalone app only: `.dmg`](https://github.com/cleafw/NeuralNote/releases/latest/download/NeuralNote_Mac_x64_unnotarized.dmg) |
| Linux: Ubuntu 24.04 x64 or a system meeting the dependencies | [Debian/Ubuntu installer `.deb`](https://github.com/cleafw/NeuralNote/releases/latest/download/NeuralNote_Linux_x64.deb) | [User-local install archive: `.tar.gz`](https://github.com/cleafw/NeuralNote/releases/latest/download/NeuralNote_Linux_x64.tar.gz) |

On a Mac, open **Apple menu → About This Mac**: an Apple M-series chip means Apple Silicon; an Intel processor means Intel. Choose either `.pkg` or `.dmg`, not both.

[Installation instructions](#installation) · [All files and release history](https://github.com/cleafw/NeuralNote/releases) · [SHA256 checksums](https://github.com/cleafw/NeuralNote/releases/latest/download/SHA256SUMS.txt)

Most users do not need the `PDBs` debug symbols or `Source code` archives. Models download separately inside the app; there is no separate Chinese-language installer.

![Chinese interface showing source audio, instrument tracks and the multitrack piano roll](docs/images/neuralnote-zh-transcription.png)

The screenshots show Chinese; select **Options → Language → English** to switch the same application to English. The default follows the system language, falling back to English for unsupported languages.

## Features

- Local, multi-instrument audio-to-MIDI transcription; automatic or explicit instrument selection.
- Vulkan GPU support in the custom Windows build, with CPU also available; tested locally on an RTX 3060 12 GB.
- Independent source and instrument playback, gain, pan, mute, solo and meters.
- Piano-key audition using the last clicked instrument; drum audition follows percussion pitches.
- Single, Ctrl, Shift and Ctrl+A track selection, independent of transcription configuration.
- Export all or selected instruments into a multitrack MIDI, or one MIDI per instrument; optional volume/pan CCs.
- Regenerate MIDI from the same audio with the selected model, after confirming replacement of existing results.
- Left-drag seeking on the waveform, middle-drag piano-roll panning, separate window sizing and UI scaling, and F11 full screen in the standalone application.

## Download and platform status

Use **[this fork's Releases](https://github.com/cleafw/NeuralNote/releases)** for this edition. The upstream website and upstream releases may not include these changes.

| Platform | Status of this fork |
| --- | --- |
| Windows x64 | Published Vulkan installer with standalone and optional VST3; transcription tested locally on RTX 3060 |
| macOS Apple Silicon / Intel | Separate unnotarized `.pkg` / `.dmg` packages published; cloud build, signature verification and packaging passed; physical Mac runtime tests pending |
| Linux x64 | Ubuntu 24.04 `.deb` / `.tar.gz` packages published; cloud build and dependency checks passed; desktop runtime and other distributions remain unverified |

The first multiplatform community release is [v2.1.1](https://github.com/cleafw/NeuralNote/releases/tag/v2.1.1). Mac packages are ad-hoc signed, **not Developer ID signed or Apple-notarized**; see the release notes for first-launch instructions. Build from source for platforms without a package. Models download separately; Chinese and English use the same application.

## Installation

### Windows

1. Download and run the Windows `.exe` above, then choose the installation folder.
2. Select the standalone application. Install VST3 as well if you use a DAW such as FL Studio; a desktop shortcut is optional.
3. Launch NeuralNote, select the actual headphone/speaker output, then choose a model and download folder in the model panel.

The installer is not code-signed, so Windows may show an unknown-publisher prompt; check the download source. GPU use requires a compatible Vulkan graphics driver. **End users do not need the development Vulkan SDK.** CPU can be selected when no suitable GPU is available.

### macOS

- **Standalone app plus plugins:** download the `.pkg` matching your chip, double-click it and follow the installer. The app goes into Applications; VST3 and AU go into the system audio plugin directories. Administrator approval may be required.
- **Standalone app only:** download the matching `.dmg`, open it, drag `NeuralNote.app` into `Applications`, then launch it from Applications.

Mac builds are ad-hoc signed, **not Apple Developer ID signed or notarized**. If macOS blocks first launch because it cannot verify the developer, verify the source, then check **System Settings → Privacy & Security → Open Anyway**. Managed computers may disallow this. See [Apple's instructions](https://support.apple.com/102445). Users do not need a developer account or paid membership.

### Linux

For Ubuntu 24.04 x64 or a compatible system meeting the package dependencies, the `.deb` is recommended. Open a terminal in the download directory:

```sh
sudo apt install ./NeuralNote_Linux_x64.deb
```

The package manager handles dependencies. Launch NeuralNote from the application menu or run `NeuralNote`. The package includes VST3. A graphical desktop and compatible graphics driver must be provided by your system.

For a user-local installation, download `.tar.gz` and run these commands in the download directory:

```sh
tar -xzf NeuralNote_Linux_x64.tar.gz
cd NeuralNote-Linux-x64
./install-user.sh
```

The script installs the executable to `~/.local/bin/NeuralNote` and VST3 to `~/.vst3/`. Launch with `~/.local/bin/NeuralNote`. Do not run the user-install script with `sudo`. The archive is **not a self-contained AppImage**: install the system dependencies listed in its `README.txt` first. Compatibility with every Linux distribution is not guaranteed.

### First launch

Use the top model panel to download small, medium or large into a folder you choose; no extraction is needed. Load audio and click Transcribe. The UI follows your system language by default; change it under Options → Language. See Quick start below for playback and export.

## Quick start

1. Choose the physical headphone/speaker output in **Options → Audio/MIDI settings**.
2. Open the model panel and click “Download to…”. Enter a full folder path or use Browse to select a drive. Review the path and click Download to start. The model loads from the selected folder without extraction.
3. Load or drop WAV, AIFF, FLAC, MP3 or Ogg Vorbis audio, or record audio.
4. Select automatic detection or the instruments to transcribe, then click **Transcribe**. Results appear progressively; pause and resume are supported.
5. Play back the result. Use the source/instrument enable switches, M (mute), S (solo), volume and pan to compare tracks.
6. Export through the File menu or an instrument's context menu, or drag **Drag MIDI out** onto a MIDI track in your DAW.

### Regenerate and navigate

Select a model, then choose **Regenerate MIDI** above the waveform, in the File menu, or in the waveform context menu. Confirm replacement of existing finished/paused results; export them first if you need to keep them. The source audio stays loaded. Regeneration is disabled while recording or transcribing.

Left-drag on the waveform to seek continuously; release to stop seeking. Middle-drag in the piano roll to pan time and pitch. Hold a piano key to audition the current instrument and release to stop.

### Select and export tracks

Click for a single selection, Ctrl-click to toggle, Shift-click for a range, and Ctrl+A to select all. The context menu exports one instrument, the selected instruments in separate MIDI tracks, or one file per selected instrument. Mute and solo affect playback, not export scope. See the [detailed workflow](docs/USER_WORKFLOW.md) for session and MIDI options.

## Screenshots

<details>
<summary>Loaded audio, ready to transcribe</summary>

![Loaded audio with the Chinese transcription button](docs/images/neuralnote-zh-audio-loaded.png)

</details>

<details>
<summary>Multitrack playback and meters</summary>

![Playing transcribed instruments with the Chinese interface](docs/images/neuralnote-zh-playback.png)

</details>

## Models and storage

| Model | Approximate download | Trade-off |
| --- | --- | --- |
| small | 209 MB | Lower resource use; prioritize speed |
| medium | 618 MB | Balance speed and transcription quality |
| large | 2.7 GB | Higher resource use; a dedicated GPU is recommended |

All three can be installed and switched through the model menu. Performance and quality depend on the source, model and hardware; compare the same passage rather than assuming the largest is always best.

Choose a folder before downloading. On Windows, a fresh download prefers a suitable non-system local drive with a `NeuralNote/models` folder, falling back to user Documents or another writable location. Existing custom locations are retained. The download directory is the loading directory.

Windows settings live under `<user home>/.neuralnote`, with logs in `logs/NeuralNote.log`; settings and model directories are independent.

Models come from [DamRsn/muscriptor-gguf](https://huggingface.co/DamRsn/muscriptor-gguf). For manual installation use the supported `v1` files unchanged, keeping names such as `muscriptor-medium-f16.gguf`. Application update checks use **cleafw/NeuralNote** Releases, not upstream. Pushing commits alone does not publish a new version.

## Automated releases

GitHub Actions builds Windows Vulkan GPU, macOS Apple Silicon/Intel (ad-hoc signed, not notarized), and Linux x64 packages. Releases become public only after all required builds succeed. See the [release workflow notes (Chinese)](docs/RELEASE_WORKFLOW.md). Available downloads are listed in Releases.

## Build from source

Requirements: Git, CMake 3.24+, a C++23 compiler and Python 3. First configuration downloads ggml and the soundfont sources, so internet access is needed.

```sh
git clone --recurse-submodules https://github.com/cleafw/NeuralNote.git
cd NeuralNote
```

If submodules are missing, run `git submodule update --init --recursive`.

### Windows GPU build

Install Visual Studio C++ Build Tools, the Windows SDK and the Vulkan SDK, then use:

```powershell
./Tools/build-windows-gpu.ps1 -InteractionTests
# For another Vulkan SDK location:
./Tools/build-windows-gpu.ps1 -VulkanSdk 'E:\Environment\VulkanSDK\1.4.350.0' -InteractionTests
```

**The script currently contains maintainer-local Python, Vulkan SDK and optional ggml cache paths. Adapt them to your machine first.** GPU support is required for this custom Windows delivery; missing dependencies must not silently produce a CPU-only build. See the [build notes](docs/WINDOWS_CUSTOM_BUILD.md).

The standalone output is `build/NeuralNote_artefacts/Release/Standalone/NeuralNote.exe`. Build products and model files are not committed; downloadable installers belong in Releases.

### macOS and Linux

Install the platform dependencies before using this development build entry point:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

macOS needs Xcode and its Metal toolchain. Each build targets one architecture; use `-DCMAKE_OSX_ARCHITECTURES=x86_64` for Intel. Linux needs [JUCE's dependencies](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md); GPU builds also need Vulkan development tools and `-DMUSCRIPTOR_VULKAN=ON`. This fork still needs platform-specific runtime verification.

## Issues and contributions

Report issues in [this fork](https://github.com/cleafw/NeuralNote/issues), including OS, GPU, model, reproduction steps and relevant logs. Pull requests are welcome. Engine API documentation is in [muscriptor.cpp](https://github.com/DamRsn/muscriptor.cpp).

## License

NeuralNote code is under [Apache-2.0](LICENSE). Dependencies and assets retain their own licenses; see [Installers/license.txt](Installers/license.txt) and each dependency directory. JUCE offers AGPLv3 and commercial JUCE licensing paths; binary distributions must comply with the selected path. The root license does not cover every dependency.

**MuScriptor model weights use [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/), with attribution and noncommercial-use restrictions.** Code and model licenses are separate. The local Windows custom build disables ASIO; enabling and distributing ASIO requires compliance with the applicable Steinberg license.

## Credits

#### NeuralNote v2

Developed by [Damien Ronssin](https://github.com/DamRsn), with AI assistance.

#### NeuralNote v1

Developed by [Damien Ronssin](https://github.com/DamRsn) and [Tibor Vass](https://github.com/tiborvass). The plugin
user interface was designed by Perrine Morel.

Many thanks to the v1 contributors!

- [jatinchowdhury18](https://github.com/jatinchowdhury18): File browser.
- [trirpi](https://github.com/trirpi)
  - More scale options in `SCALE QUANTIZE`.
  - Horizontal zoom for the audio waveform and the piano roll.
- [polygon](https://github.com/polygon) and [SamuMazzi](https://github.com/SamuMazzi): Linux support.
