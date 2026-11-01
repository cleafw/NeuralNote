#include "NnLocalisation.h"
#include <cstdio>

// Isolate tests from the user's saved preferences.
static juce::String preference;
namespace NnGlobalSettings { juce::String getLanguage() { return preference; } }
int main()
{
    int failures = 0;
    const auto check = [&](bool condition, const char* name) {
        if (!condition) { std::printf("FAIL: %s\n", name); ++failures; }
    };
    check(nn::resolvesToChinese("system", "zh-CN"), "Chinese Windows UI language");
    check(nn::resolvesToChinese("system", "zh-TW"), "Chinese locale fallback");
    check(nn::resolvesToChinese("system", "zh_Hans"), "underscore locale");
    check(!nn::resolvesToChinese("system", "ja-JP"), "unsupported system language defaults to English");
    check(!nn::resolvesToChinese("system", ""), "missing system language defaults to English");
    check(!nn::resolvesToChinese("en", "zh-CN"), "explicit English overrides system");
    check(nn::resolvesToChinese("zh-CN", "en-US"), "explicit Chinese overrides system");
    preference = "zh-CN";
    nn::applyLanguage();
    const auto chinese = juce::String::fromUTF8(reinterpret_cast<const char*>(u8"语言"));
    check(nn::tr("Language") == chinese, "UTF-8 dictionary decoding");
    check(juce::translate("Language") == chinese, "framework translation map");
    check(nn::tr("__missing_key__") == "__missing_key__", "missing translation stays readable");
    check(nn::tr("Piano") != "Piano", "instrument dictionary");
    const auto settingsTitle = juce::translate("Audio/MIDI Settings");
    check(settingsTitle != "Audio/MIDI Settings", "audio settings window title");
    for (const auto* key : {"Options", "Settings...", "Mute audio input", "Feedback Loop:",
                            "MIDI Output:", "Active MIDI inputs:", "Show advanced settings...",
                            "Hide advanced settings...", "Save current state", "Load a saved state"})
        check(juce::translate(key) != key, key);
    check(nn::retranslate("NVIDIA GeForce RTX 3060") == "NVIDIA GeForce RTX 3060", "preserve device names");
    preference = "en";
    nn::applyLanguage();
    check(nn::tr("Language") == "Language", "switch back to English");
    check(juce::translate("Language") == "Language", "clear framework Chinese map");
    check(nn::retranslate(settingsTitle) == "Audio/MIDI Settings", "existing window switches back to English");
    preference = "system";
    nn::applyLanguage();
    check(nn::usesChinese() == nn::resolvesToChinese("system", juce::SystemStats::getDisplayLanguage()),
          "follow current OS language");
    juce::LocalisedStrings::setCurrentMappings(nullptr);
    std::printf("Localisation checks: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
