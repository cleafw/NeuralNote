"""Reproducible, idempotent translation hooks for the pinned JUCE submodule.

Keep patches here in the parent repository: a fresh recursive clone must get
the same fixes without requiring a separately published JUCE fork.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'ThirdParty/JUCE/modules'
patches = {
    'juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h': [
        ('DocumentWindow (title, backgroundColour, DocumentWindow::minimiseButton | DocumentWindow::closeButton)', 'DocumentWindow (title, backgroundColour, DocumentWindow::minimiseButton | DocumentWindow::maximiseButton | DocumentWindow::closeButton)'),
        ('setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::closeButton, false);', 'setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::maximiseButton | DocumentWindow::closeButton, false);'),
        ('Component::addAndMakeVisible (optionsButton);', 'Component::addChildComponent (optionsButton);'),
        ('shouldShowNotification = newInputMutedValue;', 'shouldShowNotification = false; // Input remains muted; status is available in Options.'),
        ('        setContentOwned (content, resizeAutomatically);', '        setContentOwned (content, resizeAutomatically);\n        if (auto* editor = pluginHolder->processor->getActiveEditor()) {\n            const Component::SafePointer<StandaloneFilterWindow> safe(this);\n            editor->getProperties().set("nnHostAction", var(var::NativeFunction([safe](const var::NativeFunctionArgs& args) -> var {\n                if (!safe) return {};\n                const int action = args.numArguments ? (int)args.arguments[0] : 0;\n                if (action == 7) return safe->getProperties()["nnPresentationFullScreen"];\n                if (action == 6 || action == 8) {\n                    auto& props = safe->getProperties();\n                    const bool wasFull = (bool)props["nnPresentationFullScreen"];\n                    if (action == 8 && !wasFull) return false;\n                    if (!wasFull) {\n                        props.set("nnPreviousMaximized", safe->isFullScreen());\n                        props.set("nnPreviousNativeTitle", safe->isUsingNativeTitleBar());\n                        props.set("nnPreviousTitleHeight", safe->getTitleBarHeight());\n                        props.set("nnPresentationFullScreen", true);\n                        if (safe->isFullScreen()) safe->setFullScreen(false);\n                        safe->setUsingNativeTitleBar(false); safe->setTitleBarHeight(0); safe->setFullScreen(true);\n                    } else {\n                        safe->setFullScreen(false);\n                        props.set("nnPresentationFullScreen", false);\n                        safe->setUsingNativeTitleBar((bool)props["nnPreviousNativeTitle"]);\n                        safe->setTitleBarHeight((int)props["nnPreviousTitleHeight"]);\n                        if ((bool)props["nnPreviousMaximized"]) safe->setFullScreen(true);\n                    }\n                    return props["nnPresentationFullScreen"];\n                }\n                if (action == 5) safe->pluginHolder->shouldMuteInput.setValue(!(bool)safe->pluginHolder->shouldMuteInput.getValue());\n                else if (action != 0) safe->handleMenuResult(action);\n                return safe ? safe->pluginHolder->shouldMuteInput.getValue() : var();\n            })));\n        }\n'),
        ('optionsButton ("Options")', 'optionsButton (TRANS ("Options"))'),
        ('shouldMuteLabel  ("Feedback Loop:", "Feedback Loop:")',
         'shouldMuteLabel  ("Feedback Loop:", TRANS ("Feedback Loop:"))'),
        ('shouldMuteButton ("Mute audio input")', 'shouldMuteButton (TRANS ("Mute audio input"))'),
        ('notification ("notification", "Audio input is muted to avoid feedback loop")',
         'notification ("notification", TRANS ("Audio input is muted to avoid feedback loop"))'),
        ('settingsButton ("Unmute Input")', 'settingsButton (TRANS ("Unmute Input"))'),
        ('settingsButton ("Settings...")', 'settingsButton (TRANS ("Settings..."))'),
    ],
    'juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp': [
        ('#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>', '#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>\nnamespace NNFileUtils { juce::File getNeuralNoteDirectory(); }'),
        ('        appProperties.setStorageParameters (options);', '#if JUCE_WINDOWS\n        options.folderName = ::NNFileUtils::getNeuralNoteDirectory().getFullPathName();\n#endif\n        appProperties.setStorageParameters (options);'),
    ],
    'juce_audio_utils/gui/juce_AudioDeviceSelectorComponent.cpp': [
        ('" samples ("', 'TRANS (" samples (")'),
        ('(showAdvancedSettingsButton->getToggleState() ? "Hide " : "Show ")\n'
         '                                                   + String ("advanced settings...")',
         '(showAdvancedSettingsButton->getToggleState() ? TRANS ("Hide advanced settings...")\n'
         '                                                                                 : TRANS ("Show advanced settings..."))'),
    ],
}
for relative, replacements in patches.items():
    path = root / relative
    newline = '\r\n' if b'\r\n' in path.read_bytes() else '\n'
    original = path.read_text(encoding='utf-8')
    text = original
    if relative.endswith('juce_StandaloneFilterWindow.h'):
        anchor = '        setContentOwned (content, resizeAutomatically);'
        start = text.index(anchor) + len(anchor)
        prefix = '\n        if (auto* editor = pluginHolder->processor->getActiveEditor()) {'
        suffix = '            })));\n        }\n'
        while text.startswith(prefix, start):
            end = text.index(suffix, start) + len(suffix)
            text = text[:start] + text[end:]
    for old, new in replacements:
        if new in text:
            continue
        if text.count(old) != 1:
            raise RuntimeError(f'JUCE changed; review localisation patch: {relative}: {old}')
        text = text.replace(old, new)
    if text != original:
        path.write_text(text, encoding='utf-8', newline=newline)
