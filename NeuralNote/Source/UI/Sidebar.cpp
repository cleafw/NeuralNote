//
// Created by Damien Ronssin on 07.08.26.
//

#include "Sidebar.h"

#include <algorithm>
#include <cmath>

#include "InstrumentSelection.h"
#include "MeterScale.h"
#include "NeuralNoteTooltips.h"
#include "NnFonts.h"
#include "NnIcons.h"
#include "NnLook.h"
#include "Player.h"
#include "PluginProcessor.h"
#include "NnLocalisation.h"

namespace
{
constexpr int PADDING_SIDE = 14;
constexpr int HEADER_GAP = 8;
constexpr int ADD_BUTTON_SIZE = 18;
constexpr int MASTER_HEIGHT = 63;
constexpr int MASTER_PAD_TOP = 12;
constexpr int MASTER_LABEL_HEIGHT = 12;
constexpr float HEADER_TRACKING = 0.13f;

// Where the master meter sits, from the panel's bounds. Shared by resized() and the paint below so
// the label and the meter cannot drift apart.
juce::Rectangle<int> masterMeterBounds(juce::Rectangle<int> inBounds)
{
    auto content = inBounds.reduced(PADDING_SIDE, 0);
    content.removeFromTop(MASTER_PAD_TOP + MASTER_LABEL_HEIGHT + nn::metrics::masterMeterTopGap);

    return content.removeFromTop(nn::metrics::masterMeterHeight);
}

void paintMasterPanel(juce::Graphics& g, juce::Rectangle<int> inBounds)
{
    if (inBounds.isEmpty()) {
        return;
    }

    nn::drawTopBorder(g, inBounds, nn::colours::divSoft);

    auto content = inBounds.reduced(PADDING_SIDE, 0);
    content.removeFromTop(MASTER_PAD_TOP);
    auto label_row = content.removeFromTop(MASTER_LABEL_HEIGHT);

    g.setColour(nn::colours::textLabel);
    nn::drawTrackedText(g,
                        nn::tr("MASTER"),
                        nn::fonts::sectionHeader(),
                        label_row.toFloat(),
                        juce::Justification::centredLeft,
                        HEADER_TRACKING);
}

} // namespace

void Sidebar::StripList::resized()
{
    int y = 0;
    if (source) { source->setBounds(0,y,getWidth(),nn::metrics::stripHeight); y += nn::metrics::stripHeight; }

    for (const std::unique_ptr<InstrumentStrip>& strip: strips) {
        strip->setBounds(0, y, getWidth(), nn::metrics::stripHeight);
        y += nn::metrics::stripHeight;
    }
}

Sidebar::Sidebar(NeuralNoteAudioProcessor& inProcessor)
    : mProcessor(inProcessor)
{
    setWantsKeyboardFocus(true);
    const auto storedSelection = mProcessor.getValueTree().getProperty("selectedTrackPrograms");
    for (const auto& value : juce::StringArray::fromTokens(storedSelection.toString(), ",", ""))
        if (value.isNotEmpty()) mSelected.insert(value.getIntValue());
    mStripList.source = std::make_unique<InstrumentStrip>(*mProcessor.getInstrumentMixer(), SOURCE_AUDIO_TRACK);
    mStripList.addAndMakeVisible(*mStripList.source);
    refreshSourceStrip();
    mViewport.setViewedComponent(&mStripList, false);
    mViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(mViewport);

    mAddButton.setIcon(nn::icons::plusStroked, NnFlatButton::IconStyle::stroked, 13.0f);
    mAddButton.setCornerRadius(4.0f);
    mAddButton.setColour(NnFlatButton::backgroundColourId, nn::colours::bgControlSubtle);
    mAddButton.setColour(NnFlatButton::iconColourId, nn::colours::textIconSoft);
    mAddButton.setColour(NnFlatButton::backgroundOnColourId, nn::colours::accentFillToggle());
    mAddButton.setColour(NnFlatButton::iconOnColourId, nn::colours::accentText);
    mAddButton.setTooltip(nn::tr(NeuralNoteTooltips::add_instrument));
    mAddButton.setWantsKeyboardFocus(false);
    mAddButton.onClick = [this] {
        if (onAddInstrument != nullptr) {
            onAddInstrument();
        }
    };
    addAndMakeVisible(mAddButton);

    mMasterMeter.setColour(NnLevelMeter::unlitColourId, nn::colours::meterUnlitMaster);
    addAndMakeVisible(mMasterMeter);

    mProcessor.getInstrumentMixer()->addChangeListener(this);
    mProcessor.addListenerToStateValueTree(this);

    _pushSelectionToMixer();
    _rebuildStrips();
    updateEnablements();
}

Sidebar::~Sidebar()
{
    mProcessor.removeListenerFromStateValueTree(this);
    mProcessor.getInstrumentMixer()->removeChangeListener(this);
}

void Sidebar::updateEnablements()
{
    refreshSourceStrip();
    // The selection only takes effect when a run starts -- the library treats it as a decoder
    // constraint rather than a filter -- so the button goes away once there are notes.
    mAddButton.setVisible(!mProcessor.hasTranscription());
    repaint(mHeaderBounds);

    for (const auto& strip: mStripList.strips) {
        strip->setTranscriptionFinished(mProcessor.getState() == PopulatedAudioAndMidiRegions);
    }
}

void Sidebar::resized()
{
    auto bounds = getLocalBounds().withTrimmedRight(1);

    mHeaderBounds = bounds.removeFromTop(nn::metrics::sidebarHeader);
    mAddButton.setBounds(mHeaderBounds.getRight() - PADDING_SIDE - ADD_BUTTON_SIZE,
                         mHeaderBounds.getCentreY() - ADD_BUTTON_SIZE / 2,
                         ADD_BUTTON_SIZE,
                         ADD_BUTTON_SIZE);

    mMasterBounds = bounds.removeFromBottom(MASTER_HEIGHT);
    mMasterMeter.setBounds(masterMeterBounds(mMasterBounds));

    mViewport.setBounds(bounds);
    mStripList.setSize(bounds.getWidth(), (static_cast<int>(mStripList.strips.size()) + 1) * nn::metrics::stripHeight);
}

void Sidebar::paint(juce::Graphics& g)
{
    g.fillAll(nn::colours::bgSidebar);
    nn::drawRightBorder(g, getLocalBounds(), nn::colours::divStrong);

    _paintHeader(g, mHeaderBounds);
    paintMasterPanel(g, mMasterBounds);
}

void Sidebar::changeListenerCallback(juce::ChangeBroadcaster* inSource)
{
    if (inSource != mProcessor.getInstrumentMixer()) {
        return;
    }

    _rebuildStrips();
    refreshSourceStrip();
    repaint();
}

void Sidebar::valueTreePropertyChanged(juce::ValueTree& inTree, const juce::Identifier& inProperty)
{
    juce::ignoreUnused(inTree);

    if (inProperty == juce::Identifier("selectedTrackPrograms")) {
        mSelected.clear();
        for (const auto& value : juce::StringArray::fromTokens(inTree.getProperty(inProperty).toString(), ",", ""))
            if (value.isNotEmpty()) mSelected.insert(value.getIntValue());
        for (auto& strip : mStripList.strips) strip->setTrackSelected(mSelected.contains(strip->getProgram()), strip->getProgram() == (int)inTree.getProperty("activePreviewProgram", 0));
    }
    if (inProperty == juce::Identifier("activePreviewProgram")) {
        for (auto& strip : mStripList.strips) strip->setTrackSelected(mSelected.contains(strip->getProgram()), strip->getProgram() == (int)inTree.getProperty(inProperty));
    }
    if (inProperty == NnId::SelectedInstrumentGroupsId) {
        // Also the path a session reload takes, so the restored selection reaches the strips.
        _pushSelectionToMixer();
        repaint();
    }
}

void Sidebar::_pushSelectionToMixer()
{
    mProcessor.getInstrumentMixer()->setSelectedPrograms(
        InstrumentSelection::selectedPrograms(mProcessor.getValueTree()));
}

juce::Point<int> Sidebar::getMenuAnchor() const
{
    return {getWidth() - nn::metrics::menuAnchorRight, nn::metrics::menuAnchorTop};
}

void Sidebar::setMenuOpen(bool inIsOpen)
{
    mAddButton.setToggleState(inIsOpen, juce::dontSendNotification);
}

void Sidebar::_onVBlankCallback(double inTimestampSeconds)
{
    // Clamped so a stalled frame does not drop the release by a whole range at once.
    const float dt = mLastFrameSeconds < 0.0
                         ? 0.0f
                         : static_cast<float>(juce::jlimit(0.0, 0.1, inTimestampSeconds - mLastFrameSeconds));

    mLastFrameSeconds = inTimestampSeconds;

    Player* const player = mProcessor.getPlayer();
    const std::uint32_t frame = player->getMeterFrameCount();

    if (frame != mLastMeterFrame) {
        mLastMeterFrame = frame;
        mLastMeterFrameSeconds = inTimestampSeconds;
    }

    // Bypassed or suspended, the plugin never reaches processBlock and the published levels simply
    // stop changing, so without this the meters would sit lit wherever they were left. Two block
    // periods rather than a fixed half second: a 32768-sample block at 44.1 kHz is 743 ms on its
    // own, and a threshold under one block would have a running plugin's meters sag and snap back
    // once per block.
    const double block_seconds = mProcessor.getSampleRate() > 0.0
                                     ? static_cast<double>(mProcessor.getBlockSize()) / mProcessor.getSampleRate()
                                     : 0.0;

    const bool stale = inTimestampSeconds - mLastMeterFrameSeconds > std::max(0.5, 2.0 * block_seconds);

    auto level_db = [stale](float inMeanSquare) {
        // Stale reads as silence rather than as an instant drop: it goes in as an instant level, so
        // the release walks the meter down over the range's second and a half like any other decay.
        // Floored at the meter's own bottom rather than at -inf, so it is a real number to walk to.
        return stale ? MeterScale::METER_MIN_DB
                     : juce::Decibels::gainToDecibels(std::sqrt(inMeanSquare), MeterScale::METER_MIN_DB);
    };

    mStripList.source->updateMeter(level_db(player->getSourceMeterMeanSquare()), dt);
    const InstrumentSynth* const synth = player->getInstrumentSynth();

    for (const std::unique_ptr<InstrumentStrip>& strip: mStripList.strips) {
        strip->updateMeter(level_db(synth->getMeterMeanSquare(strip->getProgram())), dt);
    }

    mMasterMeter.setLevelDb(level_db(player->getMasterMeterMeanSquare()), dt);
}

void Sidebar::_rebuildStrips()
{
    const std::vector<InstrumentEntry>& entries = mProcessor.getInstrumentMixer()->getEntries();

    // Strips are recreated only when the set of instruments changes, not on every mixer message: a
    // transcription publishes a chunk every few seconds, and rebuilding would drop a fader out from
    // under a mouse that was dragging it.
    bool same_instruments = entries.size() == mStripList.strips.size();

    for (std::size_t i = 0; same_instruments && i < entries.size(); i++) {
        same_instruments = entries[i].program == mStripList.strips[i]->getProgram();
    }

    if (!same_instruments) {
        mStripList.strips.clear();

        for (const InstrumentEntry& entry: entries) {
            auto strip = std::make_unique<InstrumentStrip>(*mProcessor.getInstrumentMixer(), entry.program);
            strip->onSelect = [this, program = entry.program](const juce::MouseEvent& e) { trackClicked(program, e); };
            mStripList.addAndMakeVisible(*strip);
            mStripList.strips.push_back(std::move(strip));
        }
    }

    for (std::size_t i = 0; i < entries.size(); i++) {
        mStripList.strips[i]->setEntry(entries[i]);
        mStripList.strips[i]->setTranscriptionFinished(mProcessor.getState() == PopulatedAudioAndMidiRegions);
        mStripList.strips[i]->refreshFromMixer();
    }

    mStripList.setSize(mViewport.getWidth(), (static_cast<int>(mStripList.strips.size()) + 1) * nn::metrics::stripHeight);
    updateTrackSelection();
}

void Sidebar::_paintHeader(juce::Graphics& g, juce::Rectangle<int> inBounds) const
{
    if (inBounds.isEmpty()) {
        return;
    }

    nn::drawBottomBorder(g, inBounds, nn::colours::divSoft);

    auto content = inBounds.reduced(PADDING_SIDE, 0).toFloat();

    g.setColour(nn::colours::textLabel);
    nn::drawTrackedText(
        g, nn::tr("TRACKS"), nn::fonts::sectionHeader(), content, juce::Justification::centredLeft, HEADER_TRACKING);

    g.setColour(nn::colours::textFainter);
    g.setFont(nn::fonts::mono(10.0f, 400));
    const int right_trim = mAddButton.isVisible() ? PADDING_SIDE + ADD_BUTTON_SIZE + HEADER_GAP : PADDING_SIDE;

    g.drawText(juce::String(mProcessor.getInstrumentMixer()->getEntries().size() + 1),
               inBounds.withTrimmedRight(right_trim),
               juce::Justification::centredRight);
}

void Sidebar::updateTrackSelection()
{
    const auto* mixer = mProcessor.getInstrumentMixer();
    std::erase_if(mSelected, [mixer](int p) { return mixer->findEntry(p) == nullptr; });
    int active = (int)mProcessor.getValueTree().getProperty("activePreviewProgram", -1);
    if (!mixer->findEntry(active)) {
        active = mixer->getEntries().empty() ? 0 : mixer->getEntries().front().program;
        mProcessor.getValueTree().setProperty("activePreviewProgram", active, nullptr);
    }
    juce::StringArray values;
    for (int p : mSelected) values.add(juce::String(p));
    mProcessor.getValueTree().setProperty("selectedTrackPrograms", values.joinIntoString(","), nullptr);
    for (auto& strip : mStripList.strips) strip->setTrackSelected(mSelected.contains(strip->getProgram()), strip->getProgram() == active);
}
void Sidebar::selectTrack(int program, const juce::ModifierKeys& mods)
{
    const auto& entries = mProcessor.getInstrumentMixer()->getEntries();
    if (mods.isShiftDown() && mAnchor >= 0) {
        int first = -1, last = -1;
        for (int i = 0; i < (int)entries.size(); ++i) { if (entries[i].program == mAnchor) first = i; if (entries[i].program == program) last = i; }
        if (!mods.isCommandDown()) mSelected.clear();
        if (first >= 0 && last >= 0) for (int i = std::min(first,last); i <= std::max(first,last); ++i) mSelected.insert(entries[i].program);
        else mSelected.insert(program);
    } else {
        if (!mods.isCommandDown()) mSelected.clear();
        if (mods.isCommandDown() && mSelected.contains(program)) mSelected.erase(program); else mSelected.insert(program);
        mAnchor = program;
    }
    mProcessor.getValueTree().setProperty("activePreviewProgram", program, nullptr);
    mProcessor.getPlayer()->refreshPreviewInstrument();
    updateTrackSelection();
}
void Sidebar::trackClicked(int program, const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (!e.mods.isPopupMenu() || !mSelected.contains(program)) selectTrack(program, e.mods);
    else { mProcessor.getValueTree().setProperty("activePreviewProgram", program, nullptr); mProcessor.getPlayer()->refreshPreviewInstrument(); updateTrackSelection(); }
    if (!e.mods.isPopupMenu()) return;
    juce::PopupMenu menu;
    const auto selected = getSelectedPrograms();
    const bool idle = mProcessor.getState() != Processing && mProcessor.getState() != Recording;
    const bool hasNotes = idle && mProcessor.getInstrumentMixer()->findEntry(program)->hasNotes();
    bool anyNotes = false;
    for (int p : selected) if (auto* entry = mProcessor.getInstrumentMixer()->findEntry(p)) anyNotes |= entry->hasNotes();
    const juce::Component::SafePointer<Sidebar> safe(this);
    menu.addItem(nn::tr("Export this instrument MIDI"), hasNotes, false, [safe,program] { if(safe && safe->onExportTracks) safe->onExportTracks({program}, false); });
    menu.addItem(nn::tr("Export selected MIDI"), idle && anyNotes, false, [safe,selected] { if(safe && safe->onExportTracks) safe->onExportTracks(selected, false); });
    menu.addItem(nn::tr("Export selected separately"), idle && anyNotes, false, [safe,selected] { if(safe && safe->onExportTracks) safe->onExportTracks(selected, true); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)));
}
bool Sidebar::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'A') {
        for (const auto& e : mProcessor.getInstrumentMixer()->getEntries()) mSelected.insert(e.program);
        updateTrackSelection(); return true;
    }
    return false;
}

void Sidebar::refreshSourceStrip()
{
    InstrumentEntry source {SOURCE_AUDIO_TRACK, "Source audio", "SRC", nn::colours::accentText};
    source.noteCount = mProcessor.getSourceAudioManager()->getAudioSampleDuration() > 0 ? 1 : 0;
    mStripList.source->setEntry(source);
    mStripList.source->refreshFromMixer();
}
