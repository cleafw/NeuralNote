"""Only make the draft public once every required installer is present."""
import hashlib
import json
import os
import subprocess
from pathlib import Path

def main():
    tag = os.environ['RELEASE_TAG']
    env = dict(os.environ, GH_REPO=os.environ['GITHUB_REPOSITORY'])
    data = json.loads(subprocess.check_output(['gh', 'release', 'view', tag, '--json', 'assets'], env=env))
    names = {asset['name'] for asset in data['assets']}
    required = {
        'NeuralNote_Installer_Windows_x64.exe',
        'NeuralNote_Mac_arm64_unnotarized.pkg', 'NeuralNote_Mac_arm64_unnotarized.dmg',
        'NeuralNote_Mac_x64_unnotarized.pkg', 'NeuralNote_Mac_x64_unnotarized.dmg',
        'NeuralNote_Linux_x64.deb', 'NeuralNote_Linux_x64.tar.gz',
        f'NeuralNote-{tag}-source-with-submodules.tar.gz',
    }
    if required - names:
        raise SystemExit(f'Missing release files: {sorted(required - names)}')
    Path('release-downloads').mkdir(exist_ok=True)
    subprocess.run(['gh', 'release', 'download', tag, '--dir', 'release-downloads', '--clobber'], env=env, check=True)
    sums = []
    for path in sorted(Path('release-downloads').iterdir()):
        if path.name != 'SHA256SUMS.txt':
            h = hashlib.sha256()
            with path.open('rb') as stream:
                for block in iter(lambda: stream.read(1024*1024), b''):
                    h.update(block)
            sums.append(f'{h.hexdigest()}  {path.name}\n')
    Path('SHA256SUMS.txt').write_text(''.join(sums), encoding='utf8')
    subprocess.run(['gh', 'release', 'upload', tag, 'SHA256SUMS.txt', '--clobber'], env=env, check=True)
    subprocess.run(['gh', 'release', 'edit', tag, '--draft=false', '--latest'], env=env, check=True)

if __name__ == '__main__':
    main()
