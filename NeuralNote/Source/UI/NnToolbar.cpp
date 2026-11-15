//
// Created by Damien Ronssin on 07.08.26.
//

#include "NnToolbar.h"

#include "NeuralNoteTooltips.h"
#include "NNFileUtils.h"
#include "NnFonts.h"
#include "NnIcons.h"
#include "NnLogger.h"
#include "NnLook.h"
#include "PluginProcessor.h"
#include "NnLocalisation.h"

namespace
{
constexpr int PADDING_SIDE = 14;
constexpr int FILENAME_GAP = 12; // the least the filename keeps clear of the tempo pill
constexpr int PILL_PADDING = 10;
constexpr int PILL_GAP = 8;
constexpr int TEMPO_VALUE_WIDTH = 40;
constexpr float LABEL_TRACKING = 0.09f;

juce::String tempoStatusText(SourceAudioManager::TempoStatus status)
{
    using Status = SourceAudioManager::TempoStatus;
    switch (status) {
        case Status::Analysing: return nn::tr("Analysing audio...");
        case Status::Estimated: return nn::tr("Estimated from audio");
        case Status::Uncertain: return nn::tr("No reliable beat found. Tap or enter BPM.");
        case Status::Manual: return nn::tr("Entered manually");
        case Status::Tapped: return nn::tr("Set by tapping");
        case Status::Host: return nn::tr("From the host tempo");
        case Status::Restored: return nn::tr("Restored from session");
        case Status::Default: return nn::tr("Default (not measured)");
    }
    return {};
}

class TempoTools final : public juce::Component, private juce::Timer
{
public:
    explicit TempoTools(std::function<juce::String()> inStatus) : status(std::move(inStatus))
    {
        heading.setText(nn::tr("Tempo tools"), juce::dontSendNotification);
        hint.setText(nn::tr("Tap the beat at least 4 times."), juce::dontSendNotification);
        hint.setJustificationType(juce::Justification::centredLeft);
        for (auto* child : std::array<juce::Component*, 6>{&heading, &readout, &analyse, &tap, &reset, &hint})
            addAndMakeVisible(*child);
        setSize(332, 145);
        timerCallback();
        startTimerHz(5);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        heading.setBounds(area.removeFromTop(22));
        readout.setBounds(area.removeFromTop(25));
        analyse.setBounds(area.removeFromTop(27));
        area.removeFromTop(5);
        auto actions = area.removeFromTop(27);
        tap.setBounds(actions.removeFromLeft(actions.getWidth() * 2 / 3).withTrimmedRight(6));
        reset.setBounds(actions);
        hint.setBounds(area);
    }

    juce::TextButton analyse {nn::tr("Analyse audio")}, tap {nn::tr("Tap the beat")}, reset {nn::tr("Reset taps")};
    juce::Label hint;

private:
    void timerCallback() override { readout.setText(status(), juce::dontSendNotification); }
    std::function<juce::String()> status;
    juce::Label heading, readout;
};
} // namespace

NnToolbar::NnToolbar(NeuralNoteAudioProcessor& inProcessor)
    : mProcessor(inProcessor)
    , mDragButton(&inProcessor)
{
    auto tempo_is_valid = [](const juce::String& inText) {
        if (inText.isEmpty()) {
            return false;
        }
        if (inText.indexOfChar('.') != inText.lastIndexOfChar('.')) return false;

        const float tempo = inText.getFloatValue();
        return tempo >= 20.0f && tempo <= 999.0f;
    };

    auto correct_tempo = [this](const juce::String&) {
        return NumericTextEditor<double>::numberToStr(
            static_cast<double>(mProcessor.getValueTree().getProperty(NnId::ExportTempoId, 120.0)));
    };

    mTempoEditor = std::make_unique<NumericTextEditor<double>>(
        &mProcessor, NnId::ExportTempoId, 6, 120.0, juce::Justification::centredLeft, tempo_is_valid, correct_tempo);
    mTempoEditor->setTooltip(nn::tr(NeuralNoteTooltips::export_tempo));
    mTempoEditor->onValidEdit = [this] {
        mTapTempo.reset();
        mProcessor.getSourceAudioManager()->markManualTempo();
    };

    // TextEditor only centres vertically when the line fits below its top indent, so the indent goes
    // and the editor takes the pill's full height.
    mTempoEditor->setIndents(mTempoEditor->getLeftIndent(), 0);
    addAndMakeVisible(*mTempoEditor);
    mTempoToolsButton.setLabel(nn::tr("Measure"), nn::fonts::buttonLabel());
    mTempoToolsButton.setPadding(8, 8, 0);
    mTempoToolsButton.setColour(NnFlatButton::backgroundColourId, nn::colours::bgControlAlt);
    mTempoToolsButton.setTooltip(nn::tr("Analyse the audio or tap to set BPM"));
    mTempoToolsButton.onClick = [this] { _showTempoTools(); };
    addAndMakeVisible(mTempoToolsButton);

    mExportButton.setIcon(nn::icons::folderStroked, NnFlatButton::IconStyle::stroked, 13.0f);
    mExportButton.setLabel(nn::tr("Export MIDI out"), nn::fonts::buttonLabel());
    mExportButton.setPadding(12, 12, 7);
    mExportButton.setColour(NnFlatButton::backgroundColourId, nn::colours::bgControlAlt);
    mExportButton.setColour(NnFlatButton::iconColourId, nn::colours::textIconSoft);
    mExportButton.setColour(NnFlatButton::textColourId, nn::colours::textButton);
    mExportButton.setTooltip(nn::tr("Write the transcribed MIDI to a file"));
    mExportButton.setWantsKeyboardFocus(false);
    mExportButton.onClick = [this] { _exportMidiFile(); };
    addAndMakeVisible(mExportButton);

    mDragButton.setWantsKeyboardFocus(false);
    addAndMakeVisible(mDragButton);

    mClearButton.setIcon(nn::icons::trashStroked, NnFlatButton::IconStyle::stroked, 13.0f);
    mClearButton.setColour(NnFlatButton::backgroundColourId, nn::colours::bgControlAlt);
    mClearButton.setColour(NnFlatButton::iconColourId, nn::colours::textIconSoft);
    mClearButton.setTooltip(nn::tr(NeuralNoteTooltips::clear));
    mClearButton.setWantsKeyboardFocus(false);
    mClearButton.onClick = [this] { _clearOneStep(); };
    addAndMakeVisible(mClearButton);

    mRegenerateButton.setLabel(nn::tr("Regenerate MIDI"), nn::fonts::buttonLabel());
    mRegenerateButton.setPadding(10, 10, 0);
    mRegenerateButton.setTooltip(nn::tr("Transcribe the same audio again using the selected model"));
    mRegenerateButton.setWantsKeyboardFocus(false);
    mRegenerateButton.onClick = [this] { mProcessor.getTranscriptionManager()->requestRestartTranscription(); };
    addAndMakeVisible(mRegenerateButton);

    updateEnablements();
}

void NnToolbar::resized()
{
    auto bounds = getLocalBounds().withTrimmedBottom(1).reduced(PADDING_SIDE, 0);

    const int button_height = nn::metrics::toolbarButton;

    auto place_right = [&bounds](juce::Component& inComponent, int inWidth) {
        inComponent.setBounds(bounds.removeFromRight(inWidth).withSizeKeepingCentre(inWidth, button_height));
        bounds.removeFromRight(nn::metrics::controlGap);
    };

    place_right(mClearButton, button_height);
    place_right(mDragButton, mDragButton.getIdealWidth());
    place_right(mExportButton, mExportButton.getIdealWidth());
    place_right(mTempoToolsButton, mTempoToolsButton.getIdealWidth());

    const int label_width =
        juce::roundToInt(std::ceil(nn::trackedTextWidth("BPM", nn::fonts::pillLabel(), LABEL_TRACKING)));
    const int tempo_width = 2 * PILL_PADDING + label_width + PILL_GAP + TEMPO_VALUE_WIDTH;

    mTempoPill = bounds.removeFromRight(tempo_width).withSizeKeepingCentre(tempo_width, button_height);
    mTempoEditor->setBounds(
        mTempoPill.getX() + PILL_PADDING + label_width + PILL_GAP, mTempoPill.getY(), TEMPO_VALUE_WIDTH, button_height);
    bounds.removeFromRight(nn::metrics::controlGap);
    place_right(mRegenerateButton, mRegenerateButton.getIdealWidth());
}

void NnToolbar::paint(juce::Graphics& g)
{
    g.fillAll(nn::colours::bgRoot);
    nn::drawBottomBorder(g, getLocalBounds(), nn::colours::divSoft);

    _paintTempoPill(g);

    auto filename_area = getLocalBounds()
                             .withTrimmedBottom(1)
                             .reduced(PADDING_SIDE, 0)
                             .withTrimmedRight(getWidth() - mRegenerateButton.getX() + FILENAME_GAP);

    // Nothing at all when there is no file: the waveform's drop zone right below already says the
    // window is empty, and a second sentence saying it is one too many.
    const juce::String filename = mProcessor.getSourceAudioManager()->getDroppedFilename();

    g.setColour(nn::colours::textFile);
    g.setFont(nn::fonts::filename());
    g.drawText(filename, filename_area, juce::Justification::centredLeft, true);
}

void NnToolbar::updateEnablements()
{
    const bool is_finished = mProcessor.getState() == PopulatedAudioAndMidiRegions;
    mRegenerateButton.setEnabled(mProcessor.getTranscriptionManager()->canRestartTranscription());

    // Export and drag stay off mid-transcription: a half-finished file gives no sign that it is one.
    mExportButton.setEnabled(is_finished);
    mDragButton.setEnabled(is_finished);
    const bool has_audio = mProcessor.canPlay() && mProcessor.getState() != Recording;
    mTempoEditor->setEnabled(has_audio);
    mTempoEditor->setAlpha(has_audio ? 1.0f : nn::DISABLED_ALPHA);
    mTempoToolsButton.setEnabled(has_audio);
    if (!has_audio) mTapTempo.reset();

    // Not while recording: clearing would stop the recording behind the record button's back.
    const State state = mProcessor.getState();
    mClearButton.setEnabled(state == AudioLoaded || state == Processing || state == Paused
                            || state == PopulatedAudioAndMidiRegions);

    resized();
    repaint();
}

void NnToolbar::_showTempoTools()
{
    const juce::Component::SafePointer<NnToolbar> safe(this);
    auto tools = std::make_unique<TempoTools>([safe]() -> juce::String {
        if (!safe) return {};
        const auto status = safe->mProcessor.getSourceAudioManager()->getTempoStatus();
        const auto description = tempoStatusText(status);
        if (status == SourceAudioManager::TempoStatus::Analysing || status == SourceAudioManager::TempoStatus::Uncertain)
            return description;
        const double bpm = safe->mProcessor.getValueTree().getProperty(NnId::ExportTempoId, 120.0);
        return juce::String(bpm, 1) + " BPM  -  " + description;
    });
    auto* controls = tools.get();
    mTapTempo.reset();
    controls->analyse.onClick = [safe, controls] {
        if (!safe) return;
        safe->mTapTempo.reset();
        controls->hint.setText(nn::tr("Tap the beat at least 4 times."), juce::dontSendNotification);
        safe->mProcessor.getSourceAudioManager()->analyseTempo();
    };
    controls->tap.onClick = [safe, controls] {
        if (!safe || !safe->mTempoToolsButton.isEnabled()) return;
        const auto bpm = safe->mTapTempo.tap(juce::Time::getMillisecondCounterHiRes() / 1000.0);
        if (bpm) safe->mProcessor.getSourceAudioManager()->applyTapTempo(*bpm);
        controls->hint.setText(nn::tr("Taps: {count}").replace("{count}", juce::String(safe->mTapTempo.count())),
                               juce::dontSendNotification);
    };
    controls->reset.onClick = [safe, controls] {
        if (!safe) return;
        safe->mTapTempo.reset();
        controls->hint.setText(nn::tr("Tap the beat at least 4 times."), juce::dontSendNotification);
    };
    juce::CallOutBox::launchAsynchronously(std::move(tools), mTempoToolsButton.getScreenBounds(), nullptr);
}

void NnToolbar::_exportMidiFile()
{
    const juce::String filename =
        NNFileUtils::getMidiExportFileName(mProcessor.getSourceAudioManager()->getDroppedFilename());

    mFileChooser = std::make_shared<juce::FileChooser>(
        nn::tr("Export MIDI"), NNFileUtils::getDefaultMidiExportDirectory().getChildFile(filename), "*.mid");

    mFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& inChooser) {
            const juce::File file = inChooser.getResult();

            if (file == juce::File {}) {
                return;
            }

            const double export_bpm = mProcessor.getValueTree().getProperty(NnId::ExportTempoId, 120.0);
            const auto overflow_mode =
                static_cast<MidiOverflowMode>(static_cast<int>(mProcessor.getValueTree().getProperty(
                    NnId::MidiOverflowModeId, static_cast<int>(MidiOverflowMode::ReuseChannels))));

            const bool success =
                mMidiFileWriter.writeMidiFile(mProcessor.getTranscriptionManager()->getNoteEventVector(),
                                              file,
                                              export_bpm,
                                              mProcessor.getSourceAudioManager()->getExportStartOffsetSeconds(),
                                              overflow_mode);

            if (!success) {
                NnLogger::write(NnLogger::Level::Error, "NnToolbar: Could not write the MIDI file.");
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::NoIcon, nn::tr("Error"), nn::tr("Could not write the MIDI file."));
            }
        });
}

void NnToolbar::_clearOneStep()
{
    switch (mProcessor.getState()) {
        case Processing:
            // A discarded run ends in clearTranscription(), so the audio stays for the next click.
            mProcessor.getTranscriptionManager()->discardTranscription();
            break;

        case Paused:
        case PopulatedAudioAndMidiRegions:
            mProcessor.clearTranscription();
            break;

        case AudioLoaded:
            mProcessor.clear();
            break;

        // The button is disabled in both.
        case Recording:
        case EmptyAudioAndMidiRegions:
            break;
    }
}

void NnToolbar::_paintTempoPill(juce::Graphics& g) const
{
    if (mTempoPill.isEmpty()) {
        return;
    }

    // Dimmed rather than absent before there is a transcription to export: the export group holds
    // its place in the toolbar either way, and an empty gap there reads as something failing to
    // draw.
    const float alpha = mTempoEditor->isEnabled() ? 1.0f : nn::DISABLED_ALPHA;

    g.setColour(nn::colours::bgControlAlt.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(mTempoPill.toFloat(), static_cast<float>(nn::metrics::controlCorner));

    auto content = mTempoPill.reduced(PILL_PADDING, 0);

    g.setColour(nn::colours::textDim.withMultipliedAlpha(alpha));
    nn::drawTrackedText(
        g, "BPM", nn::fonts::pillLabel(), content.toFloat(), juce::Justification::centredLeft, LABEL_TRACKING);
}
