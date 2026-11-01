#pragma once
#include <JuceHeader.h>

namespace nn {
// Call on the message thread after loading or changing the language preference.
void applyLanguage();
bool usesChinese();
bool resolvesToChinese(const juce::String& preference, const juce::String& systemLanguage);
juce::String tr(const juce::String& english);
// Translate existing framework controls when the user changes language.
juce::String retranslate(const juce::String& text);
}
