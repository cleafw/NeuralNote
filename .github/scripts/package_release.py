"""Package the compiled macOS/Linux bundles without paid signing services."""
import argparse
import os
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / 'build/NeuralNote_artefacts/Release'
DIST = ROOT / 'dist'

def run(*args, **kwargs):
    return subprocess.run([str(x) for x in args], check=True, **kwargs)

def copy_notices(destination):
    destination.mkdir(parents=True, exist_ok=True)
    for source in ('LICENSE', 'Installers/license.txt', 'Installers/COMMUNITY_DISTRIBUTION.txt', 'Installers/AGPL-3.0.txt', 'docs/RELEASE_NOTES.md'):
        shutil.copy2(ROOT / source, destination / Path(source).name)

def package_macos(arch, version):
    label = 'x64' if arch == 'x86_64' else arch
    stage = ROOT / 'build/package-macos' / arch
    if stage.exists():
        raise SystemExit(f'Packaging directory already exists: {stage}; use a clean build.')
    bundles = [('Standalone/NeuralNote.app', 'Applications/NeuralNote.app'),
               ('VST3/NeuralNote.vst3', 'Library/Audio/Plug-Ins/VST3/NeuralNote.vst3'),
               ('AU/NeuralNote.component', 'Library/Audio/Plug-Ins/Components/NeuralNote.component')]
    for source, target in bundles:
        bundle = stage / target
        shutil.copytree(RELEASE / source, bundle, symlinks=True)
        executable = bundle / 'Contents/MacOS/NeuralNote'
        result = subprocess.check_output(['lipo', '-archs', str(executable)], text=True).strip()
        if result != arch:
            raise SystemExit(f'{bundle}: expected {arch}, found {result}')
        if not (bundle / 'Contents/Resources/default.metallib').is_file():
            raise SystemExit(f'Missing Metal library: {bundle}')
        # All dependencies are linked statically by this project. Sign any nested Mach-O first.
        for path in sorted(bundle.rglob('*')):
            if path.is_file() and not path.is_symlink() and path != executable:
                kind = subprocess.check_output(['file', '-b', str(path)], text=True)
                if 'Mach-O' in kind:
                    run('codesign', '--force', '--sign', '-', '--timestamp=none', path)
        executable.chmod(0o755)
        run('codesign', '--force', '--sign', '-', '--timestamp=none', bundle)
        run('codesign', '--verify', '--deep', '--strict', bundle)
    copy_notices(stage / 'Library/Application Support/NeuralNote/Notices')
    pkg = DIST / f'NeuralNote_Mac_{label}_unnotarized.pkg'
    run('pkgbuild', '--root', stage, '--install-location', '/', '--identifier', 'com.cleafw.neuralnote.community',
        '--version', version, '--ownership', 'recommended', pkg)
    dmgroot = ROOT / 'build/package-dmg' / arch
    dmgroot.mkdir(parents=True)
    shutil.copytree(stage / 'Applications/NeuralNote.app', dmgroot / 'NeuralNote.app', symlinks=True)
    (dmgroot / 'Applications').symlink_to('/Applications', target_is_directory=True)
    copy_notices(dmgroot / 'Notices')
    (dmgroot / 'READ_ME_FIRST.txt').write_text(
        'Community build: ad-hoc signed, NOT Apple Developer ID signed or notarized.\n'
        'Drag NeuralNote.app to Applications. For AU/VST3 installation use the separate .pkg.\n'
        'macOS may block first launch: System Settings > Privacy & Security > Open Anyway.\n'
        'See https://support.apple.com/102445 . No disabling of system security is required.\n'
        'Models are downloaded separately inside the application.\n', encoding='utf8')
    run('hdiutil', 'create', '-volname', f'NeuralNote {version}', '-srcfolder', dmgroot, '-ov', '-format', 'UDZO',
        DIST / f'NeuralNote_Mac_{label}_unnotarized.dmg')

def package_linux(version):
    stage = ROOT / 'build/package-linux'
    if stage.exists():
        raise SystemExit('Use a clean packaging directory.')
    (stage / 'usr/bin').mkdir(parents=True)
    executable = stage / 'usr/bin/NeuralNote'
    shutil.copy2(RELEASE / 'Standalone/NeuralNote', executable)
    executable.chmod(0o755)
    vst = stage / 'usr/lib/vst3/NeuralNote.vst3'
    shutil.copytree(RELEASE / 'VST3/NeuralNote.vst3', vst)
    plugins = list(vst.rglob('*.so'))
    if not plugins:
        raise SystemExit('No Linux VST3 binary found')
    for binary in [executable, *plugins]:
        result = subprocess.check_output(['ldd', str(binary)], text=True)
        print(result)
        if 'not found' in result:
            raise SystemExit(f'Unresolved dependency: {binary}')
    desktop = stage / 'usr/share/applications/neuralnote.desktop'
    desktop.parent.mkdir(parents=True)
    desktop.write_text('[Desktop Entry]\nType=Application\nName=NeuralNote\nComment=Local audio to MIDI transcription\n'
                       'Exec=NeuralNote\nIcon=neuralnote\nTerminal=false\nCategories=AudioVideo;Audio;\n', encoding='utf8')
    icon = stage / 'usr/share/icons/hicolor/256x256/apps/neuralnote.png'
    icon.parent.mkdir(parents=True)
    shutil.copy2(ROOT / 'NeuralNote/Assets/Logo/neuralnote-1024.png', icon)
    copy_notices(stage / 'usr/share/doc/neuralnote')
    # Ask the distribution's dependency scanner, rather than guessing ABI version floors.
    with tempfile.TemporaryDirectory() as temp:
        work = Path(temp)
        (work / 'debian').mkdir()
        (work / 'debian/control').write_text('Source: neuralnote\n\nPackage: neuralnote\nArchitecture: amd64\nDescription: NeuralNote\n')
        result = subprocess.check_output(['dpkg-shlibdeps', '-O', *[f'-e{p}' for p in [executable, *plugins]]], cwd=work, text=True)
    dependencies = next(line.split('=', 1)[1] for line in result.splitlines() if line.startswith('shlibs:Depends='))
    debian = stage / 'DEBIAN'
    debian.mkdir()
    (debian / 'control').write_text(
        f'Package: neuralnote\nVersion: {version}\nArchitecture: amd64\nMaintainer: cleafw <noreply@github.com>\n'
        f'Depends: {dependencies}\nSection: sound\nPriority: optional\n'
        'Homepage: https://github.com/cleafw/NeuralNote\n'
        'Description: Local multi-instrument audio-to-MIDI transcription\n Community edition with Chinese and English UI, Vulkan and multitrack export.\n', encoding='utf8')
    run('dpkg-deb', '--build', '--root-owner-group', stage, DIST / 'NeuralNote_Linux_x64.deb')
    run('dpkg-deb', '--info', DIST / 'NeuralNote_Linux_x64.deb')
    portable = ROOT / 'build/NeuralNote-Linux-x64'
    portable.mkdir()
    shutil.copytree(stage / 'usr', portable / 'usr')
    (portable / 'install-user.sh').write_text(
        '#!/bin/sh\nset -eu\ncd "$(dirname "$0")"\n'
        'target="${XDG_DATA_HOME:-$HOME/.local/share}"\n'
        'mkdir -p "$HOME/.local/bin" "$HOME/.vst3" "$target/applications" "$target/icons/hicolor/256x256/apps"\n'
        'cp usr/bin/NeuralNote "$HOME/.local/bin/NeuralNote"\n'
        'cp -R usr/lib/vst3/NeuralNote.vst3 "$HOME/.vst3/"\n'
        'sed "s|Exec=NeuralNote|Exec=$HOME/.local/bin/NeuralNote|" usr/share/applications/neuralnote.desktop > "$target/applications/neuralnote.desktop"\n'
        'cp usr/share/icons/hicolor/256x256/apps/neuralnote.png "$target/icons/hicolor/256x256/apps/"\n'
        'echo "Installed. Start $HOME/.local/bin/NeuralNote"\n', encoding='utf8')
    (portable / 'install-user.sh').chmod(0o755)
    (portable / 'README.txt').write_text(
        f'Ubuntu 24.04 x64 build. Required system packages: {dependencies}\n'
        'This is a relocatable install tree, not a self-contained AppImage.\n'
        'Install dependencies first, then run ./install-user.sh, or run ./usr/bin/NeuralNote.\n'
        'On Debian/Ubuntu the .deb package is preferred: sudo apt install ./NeuralNote_Linux_x64.deb\n'
        'A Vulkan-capable graphics driver is needed for GPU use; CPU is also available.\n', encoding='utf8')
    with tarfile.open(DIST / 'NeuralNote_Linux_x64.tar.gz', 'w:gz') as tar:
        tar.add(portable, arcname=portable.name)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('platform', choices=['macos', 'linux'])
    parser.add_argument('--arch', choices=['arm64', 'x86_64'])
    args = parser.parse_args()
    version = re.search(r'project\(NeuralNotePlugin VERSION ([\d.]+)\)', (ROOT/'CMakeLists.txt').read_text())[1]
    DIST.mkdir(exist_ok=True)
    if args.platform == 'macos':
        if not args.arch:
            parser.error('--arch is required for macOS')
        package_macos(args.arch, version)
    else:
        package_linux(version)

if __name__ == '__main__':
    main()
