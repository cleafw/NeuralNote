"""Create a draft and a complete source archive, including pinned submodules."""
import os
import re
import subprocess
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def main():
    os.chdir(ROOT)
    version = re.search(r'project\(NeuralNotePlugin VERSION ([\d.]+)\)', Path('CMakeLists.txt').read_text())[1]
    installer_version = re.search(r'^AppVersion=(.*)$', Path('Installers/Windows/neuralnote.iss').read_text(encoding='utf8'), re.M)[1].strip()
    if installer_version != version:
        raise SystemExit('CMake and Windows installer versions disagree')
    tag = f'v{version}'
    ref = os.environ['GITHUB_REF']
    if ref.startswith('refs/tags/') and ref != f'refs/tags/{tag}':
        raise SystemExit(f'Tag {ref} does not match CMake version {version}')
    repo = os.environ['GITHUB_REPOSITORY']
    env = dict(os.environ, GH_REPO=repo)
    gh = lambda *args: subprocess.run(['gh', *args], env=env, check=True)
    tag_ref = subprocess.run(['gh', 'api', f'repos/{repo}/git/ref/tags/{tag}', '--jq', '.object.sha'],
                             env=env, capture_output=True, text=True)
    if tag_ref.returncode != 0:
        gh('api', f'repos/{repo}/git/refs', '-f', f'ref=refs/tags/{tag}', '-f', f"sha={os.environ['GITHUB_SHA']}")
    existing = subprocess.run(['gh', 'release', 'view', tag, '--json', 'isDraft,targetCommitish'], env=env, capture_output=True, text=True)
    if existing.returncode == 0:
        import json
        if not json.loads(existing.stdout)['isDraft']:
            raise SystemExit('This version is already public. Bump the version before releasing again.')
    else:
        gh('release', 'create', tag, '--draft', '--target', os.environ['GITHUB_SHA'],
           '--title', f'NeuralNote {version} 社区版 / Community edition',
           '--notes-file', 'docs/RELEASE_NOTES.md')
    # Bind retries to the same source commit; do not mix packages from different branches.
    target = subprocess.check_output(['gh', 'api', f'repos/{repo}/git/ref/tags/{tag}', '--jq', '.object.sha'], env=env, text=True).strip()
    if target != os.environ['GITHUB_SHA']:
        raise SystemExit('Release tag points at a different commit. Use a new version or rerun the tagged revision.')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write(f'tag={tag}\n')
    subprocess.run(['python3', 'Tools/patch_juce_localisation.py'], check=True)
    files = subprocess.check_output(['git', 'ls-files', '-z', '--recurse-submodules']).decode().split('\0')
    archive = ROOT / f'NeuralNote-{tag}-source-with-submodules.tar.gz'
    with tarfile.open(archive, 'w:gz') as tar:
        for name in files:
            if name and Path(name).is_file():
                tar.add(name, arcname=f'NeuralNote-{tag}/{name}', recursive=False)
    gh('release', 'upload', tag, str(archive), '--clobber')

if __name__ == '__main__':
    main()
