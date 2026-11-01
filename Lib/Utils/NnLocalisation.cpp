#include "NnLocalisation.h"
#include "NnGlobalSettings.h"
#include <atomic>

namespace nn {
namespace {
std::atomic<bool> chinese {false};
struct Translation { const char* english; const char8_t* translated; };
const Translation translations[] = {
#include "NnChineseSettings.inc"
#include "NnChineseTranslations.inc"
#include "NnChineseMessages.inc"
};
const juce::StringPairArray& dictionary()
{
    static const auto result = [] {
        juce::StringPairArray values;
        for (const auto& entry : translations)
            values.set(entry.english, juce::String::fromUTF8(reinterpret_cast<const char*>(entry.translated)));
        return values;
    }();
    return result;
}
}
bool resolvesToChinese(const juce::String& preference, const juce::String& systemLanguage)
{
    if (preference == "zh-CN") return true;
    if (preference == "en") return false;
    const auto language = systemLanguage.trim().toLowerCase();
    return language == "zh" || language.startsWith("zh-") || language.startsWith("zh_");
}
void applyLanguage()
{
    chinese.store(resolvesToChinese(NnGlobalSettings::getLanguage(), juce::SystemStats::getDisplayLanguage()));
    // Framework dialogs (file chooser/audio settings) use JUCE's translation mechanism.
    if (chinese.load()) {
        juce::String text("language: Chinese\ncountries: CN TW HK SG\n");
        for (const auto& entry : translations) {
            const auto escape = [](juce::String value) {
                return value.replace("\\", "\\\\").replace("\"", "\\\"")
                            .replace("\n", "\\n").replace("\r", "\\r");
            };
            text += "\"" + escape(entry.english) + "\" = \""
                + escape(juce::String::fromUTF8(reinterpret_cast<const char*>(entry.translated))) + "\"\n";
        }
        juce::LocalisedStrings::setCurrentMappings(new juce::LocalisedStrings(text, false));
    } else {
        juce::LocalisedStrings::setCurrentMappings(nullptr);
    }
}
bool usesChinese() { return chinese.load(); }
juce::String tr(const juce::String& english)
{
    return usesChinese() ? dictionary().getValue(english, english) : english;
}
juce::String retranslate(const juce::String& text)
{
    if (dictionary().containsKey(text)) return tr(text);
    const auto& values = dictionary();
    for (int i = 0; i < values.size(); ++i)
        if (values.getAllValues()[i] == text) return tr(values.getAllKeys()[i]);
    return text;
}
}
