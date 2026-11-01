"""Check literal app translations and all standalone/audio-settings JUCE hooks."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
keys = set()
for path in (root / 'Lib/Utils').glob('NnChinese*.inc'):
    keys.update(re.findall(r'^\{"((?:[^"\\]|\\.)*)",', path.read_text(encoding='utf-8'), re.M))
files = list((root / 'NeuralNote').rglob('*.cpp')) + list((root / 'Lib').rglob('*.cpp'))
files += [root / 'ThirdParty/JUCE/modules' / name for name in (
    'juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h',
    'juce_audio_utils/gui/juce_AudioDeviceSelectorComponent.cpp')]
missing = []
for path in files:
    for key in re.findall(r'(?:nn::tr|TRANS)\s*\(\s*"((?:[^"\\]|\\.)*)"\s*\)', path.read_text(encoding='utf-8')):
        if key not in keys:
            missing.append(f'{path.relative_to(root)}: {key}')
if missing:
    raise SystemExit('\n'.join(missing))
print(f'Localisation coverage: PASS ({len(keys)} dictionary entries)')
