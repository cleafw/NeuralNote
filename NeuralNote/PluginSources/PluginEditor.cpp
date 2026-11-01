#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "NnGlobalSettings.h"
#include "NnLocalisation.h"
#include "NnLogger.h"
#include "NnLook.h"

namespace {
// JUCE's standalone chrome and audio settings live outside the plugin editor.
// Update only known dictionary entries, preserving driver/device names and IDs.
void refreshFrameworkComponent(juce::Component& component)
{
    if (dynamic_cast<juce::AudioProcessorEditor*>(&component) != nullptr) return;
    const auto name = nn::retranslate(component.getName());
    if (name != component.getName()) component.setName(name);
    if (auto* label = dynamic_cast<juce::Label*>(&component)) {
        const auto text = nn::retranslate(label->getText());
        if (text != label->getText()) label->setText(text, juce::dontSendNotification);
    }
    if (auto* button = dynamic_cast<juce::Button*>(&component)) {
        const auto text = nn::retranslate(button->getButtonText());
        if (text != button->getButtonText()) button->setButtonText(text);
    }
    if (auto* tooltip = dynamic_cast<juce::SettableTooltipClient*>(&component)) {
        const auto text = nn::retranslate(tooltip->getTooltip());
        if (text != tooltip->getTooltip()) tooltip->setTooltip(text);
    }
    if (auto* combo = dynamic_cast<juce::ComboBox*>(&component)) {
        for (int i = 0; i < combo->getNumItems(); ++i) {
            const auto text = nn::retranslate(combo->getItemText(i));
            if (text != combo->getItemText(i)) combo->changeItemText(combo->getItemId(i), text);
        }
    }
    for (auto* child : component.getChildren()) refreshFrameworkComponent(*child);
}
void refreshFrameworkWindows()
{
    // Never change the host application's windows when loaded as a VST/AU.
    if (!juce::JUCEApplicationBase::isStandaloneApp()) return;
    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        refreshFrameworkComponent(*desktop.getComponent(i));
}
}

NeuralNoteEditor::NeuralNoteEditor(NeuralNoteAudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    NnLogger::write(NnLogger::Level::Debug, "NeuralNoteEditor: opened");
    NnGlobalSettings::reload();
    nn::applyLanguage();
    mWasChinese = nn::usesChinese();
    startTimerHz(2);

    mMainView = std::make_unique<NeuralNoteMainView>(p);

    addAndMakeVisible(*mMainView);

    mConstrainer.configure(this);
    mConstrainer.onResizeEnd = [this] { _persistScale(); };
    setConstrainer(&mConstrainer);
    setResizable(true, true);

    // Once per editor, so a window opened after the file changed honours what it now holds.
    NnGlobalSettings::reload();

    mPersistedScale = NnGlobalSettings::getEditorScale();
    mScale = mPersistedScale;
    mConstrainer.setUiScale(mScale);
    const auto window = NnGlobalSettings::getEditorWindowSize();
    setSize(window.x, window.y);
    mConstrainer.checkComponentBounds(this);
}

NeuralNoteEditor::~NeuralNoteEditor()
{
    stopTimer();
    // The standalone's native-frame drags never reach mConstrainer.onResizeEnd, so closing the
    // window is where the size they ended at gets stored. First, before the teardown below.
    _persistScale();

    // mConstrainer dies with this class, before ~Component destroys the peer and the corner
    // resizer -- both of which hold a raw pointer to it and can still be resized during teardown.
    setResizable(false, false);
    setConstrainer(nullptr);

    NnLogger::write(NnLogger::Level::Debug, "NeuralNoteEditor: closed");
}

void NeuralNoteEditor::paint(juce::Graphics& g)
{
    g.fillAll(nn::colours::bgRoot);
}

void NeuralNoteEditor::refreshLanguage()
{
    mWasChinese = nn::usesChinese();
    // Called asynchronously, after the old menu callback has returned. Audio and
    // transcription live in the processor and survive rebuilding the view.
    mMainView.reset();
    mMainView = std::make_unique<NeuralNoteMainView>(static_cast<NeuralNoteAudioProcessor&>(processor));
    addAndMakeVisible(*mMainView);
    resized();
    repaint();
}

void NeuralNoteEditor::timerCallback()
{
    if (mWasChinese != nn::usesChinese()) refreshLanguage();
    refreshFrameworkWindows();
    auto* window = findParentComponentOfClass<juce::ResizableWindow>();
    if (!window || !window->isFullScreen()) {
        const auto geometry = juce::String(getWidth()) + "x" + juce::String(getHeight());
        auto& properties = getProperties();
        if (properties["nnObservedGeometry"].toString() != geometry) {
            properties.set("nnObservedGeometry", geometry); properties.set("nnGeometryPending", true);
        } else if ((bool)properties["nnGeometryPending"]) {
            NnGlobalSettings::setEditorWindowSize(getWidth(), getHeight()); properties.set("nnGeometryPending", false);
        }
    }
}

void NeuralNoteEditor::resized()
{
    // A zero extent would make the transform singular, and there is nothing to lay out at it.
    if (getLocalBounds().isEmpty()) {
        return;
    }

    mMainView->setTransform(juce::AffineTransform::scale((float)mScale));
    mMainView->setBounds(0, 0, juce::roundToInt(getWidth() / mScale), juce::roundToInt(getHeight() / mScale));

}

void NeuralNoteEditor::parentHierarchyChanged()
{
    // The display is only known once the editor is attached, which it is not while the constructor
    // runs. Detachment comes through here too, and clamping a window on its way out only risks
    // resizing it during teardown.
    if (!isOnDesktop() && getParentComponent() == nullptr) {
        return;
    }

    mConstrainer.checkComponentBounds(this);
}

void NeuralNoteEditor::applyScale(double inScale)
{
    // mScale is what was applied, not what was asked for: a preset the display cannot hold
    // arrives clamped, and the clamped one is what gets stored.
    _setScale(inScale);
    _persistScale();
}

void NeuralNoteEditor::_setScale(double inScale)
{
    mScale = juce::jlimit(0.5, 2.0, inScale);
    mConstrainer.setUiScale(mScale);
    mConstrainer.checkComponentBounds(this);
    resized();

}

void NeuralNoteEditor::_persistScale()
{
    auto* window = findParentComponentOfClass<juce::ResizableWindow>();
    if ((window && !window->isFullScreen()) || (!window && !juce::JUCEApplicationBase::isStandaloneApp()))
        NnGlobalSettings::setEditorWindowSize(getWidth(), getHeight());
    if (std::abs(mScale - mPersistedScale) < 0.0005) {
        return;
    }

    mPersistedScale = mScale;
    NnGlobalSettings::setEditorScale(mScale);
}
