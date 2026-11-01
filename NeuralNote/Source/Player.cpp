//
// Created by Damien Ronssin on 11.06.23.
//

#include "Player.h"
#include "PluginProcessor.h"

Player::Player(NeuralNoteAudioProcessor* inProcessor)
    : mProcessor(inProcessor)
{
    mProcessor->addListenerToStateValueTree(this);

    mSynth = std::make_unique<InstrumentSynth>();
    mPreviewSynth = std::make_unique<InstrumentSynth>();
    mSynthController = std::make_unique<SynthController>(inProcessor);

    setPlayheadPositionSeconds(mProcessor->getValueTree().getProperty(NnId::PlayheadPositionSecId, 0.0));

    mShouldOutputMidi = mProcessor->getValueTree().getProperty(NnId::MidiOut, false);
    mShouldLoop = static_cast<bool>(mProcessor->getValueTree().getProperty(NnId::LoopEnabledId, false));
}

Player::~Player()
{
    mProcessor->removeListenerFromStateValueTree(this);
}

void Player::prepareToPlay(double inSampleRate, int inSamplesPerBlock)
{
    mSynth->prepareToPlay(inSampleRate, inSamplesPerBlock);
    mPreviewSynth->prepareToPlay(inSampleRate, inSamplesPerBlock);
    mPreviewSynth->allNotesOff();
    mSoundingPreviewKey = -1;
    mPreviewKey.store(-1);
    mSynthController->setSampleRate(inSampleRate);
    mSampleRate = inSampleRate;

    // Sized to what actually goes out, not always to two: the synth renders real stereo now -- the
    // soundfont pans instruments across the field -- and a mono host would otherwise be handed only
    // the left channel and quietly lose whatever sits on the right. At one channel the synth folds.
    mInternalBuffer.setSize(jlimit(1, 2, mProcessor->getTotalNumOutputChannels()), inSamplesPerBlock);

    mMasterMeter.prepare(inSampleRate, METER_WINDOW_SECONDS);
    mMasterMeanSquare.store(0.0f, std::memory_order_relaxed);
}

void Player::processBlock(AudioBuffer<float>& inAudioBuffer, MidiBuffer& outMidiBuffer)
{
    auto old_audio_gain = mGainSourceAudio;
    const float old_source_pan = mCurrentSourcePan;
    mSourceMeanSquare.store(0.0f);
    auto old_master_gain = mMasterGain;

    int playhead_index = static_cast<int>(std::round(mPlayheadTime * mSampleRate));

    _setGains(mProcessor->getParameterValue(ParameterHelpers::MixId),
              mProcessor->getParameterValue(ParameterHelpers::MasterGainId));

    bool is_playing = mIsPlaying.load();
    mInternalBuffer.clear();

    // Clamped to what mInternalBuffer was actually sized for at prepareToPlay: a host that reports a
    // different channel count now must not send these loops past the end of it.
    int num_out_channels = std::min(mProcessor->getTotalNumOutputChannels(), mInternalBuffer.getNumChannels());
    jassert(num_out_channels > 0 && num_out_channels <= 2);

    // Before the block is generated, not after: what the host is still holding was started in an
    // earlier block, so it is exactly what is active right now. A note ending inside this block
    // would be gone from that set by the end of it, with its note-off reaching only the synth.
    if (!mShouldOutputMidi && mWasOutputtingMidi) {
        // The host stops seeing this buffer from here on, so whatever it is holding has to be
        // released now. The synth keeps its notes; only this consumer is being disconnected.
        mSynthController->emitActiveNotesOffTo(outMidiBuffer);
        mWasOutputtingMidi = false;
    }

    // Every block, playing or not: a stopped transport, a seek and a swapped note list all leave
    // note-offs to deliver, and this is the only thing that delivers them.
    auto& midi_buffer =
        mSynthController->generateNextMidiBuffer(inAudioBuffer.getNumSamples(), is_playing, mShouldLoop.load());

    if (mShouldOutputMidi) {
        outMidiBuffer.addEvents(midi_buffer, 0, inAudioBuffer.getNumSamples(), 0);
        mWasOutputtingMidi = true;
    }

    if (mShouldSilenceSynth.exchange(false)) {
        // The scheduler's note-offs are not enough on their own: the synth deliberately ignores
        // them for drums, because a General MIDI kit is one-shot and honouring a note-off 10 ms
        // after the hit would choke every cymbal. So a stop or a seek has to say so directly, or a
        // crash rings on through it.
        mSynth->allNotesOff();
    }

    // The synth is driven by the scheduler's own event list rather than by midi_buffer: a
    // transcription can name 35 instruments and MIDI has 16 channels, so the buffer the host sees
    // is the flattened rendering of the same decisions, not the full one.
    //
    // The gain ramp lives inside the synth, which needs a per-instrument one anyway.
    mSynth->processBlock(
        mInternalBuffer, mSynthController->getSynthEvents(), inAudioBuffer.getNumSamples(), mGainSynth);

    // Separate voices: releasing a preview must never stop a transcription note.
    std::array<SynthEvent, 2> previewEvents;
    int previewCount = 0;
    const int previewKey = mPreviewKey.load();
    const int previewProgram = mPreviewProgram.load();
    if (previewKey != mSoundingPreviewKey || previewProgram != mSoundingPreviewProgram) {
        if (mSoundingPreviewKey >= 0)
            previewEvents[previewCount++] = {0, mSoundingPreviewProgram, mSoundingPreviewKey, 0.0f, false};
        if (previewKey >= 0)
            previewEvents[previewCount++] = {0, previewProgram, previewKey, 0.75f, true};
        mSoundingPreviewKey = previewKey;
        mSoundingPreviewProgram = previewProgram;
    }
    mPreviewSynth->processBlock(mInternalBuffer,
        std::span<const SynthEvent>(previewEvents.data(), static_cast<size_t>(previewCount)),
        inAudioBuffer.getNumSamples(), 0.65f);

    if (is_playing && mProcessor->canPlay()) {
        const auto& source_buffer = mProcessor->getSourceAudioManager()->getSourceAudioForPlayback();
        int num_samples = std::min(inAudioBuffer.getNumSamples(), source_buffer.getNumSamples() - playhead_index);

        int num_source_channel = source_buffer.getNumChannels();

        for (int ch = 0; ch < num_out_channels; ch++) {
            int source_channel = std::min(ch, num_source_channel - 1);
            mInternalBuffer.addFromWithRamp(ch,
                                            0,
                                            source_buffer.getReadPointer(source_channel) + playhead_index,
                                            num_samples,
                                            old_audio_gain * (num_out_channels == 1 ? 1.0f : ch == 0 ? (old_source_pan > 0 ? 1-old_source_pan : 1) : (old_source_pan < 0 ? 1+old_source_pan : 1)),
                                            mGainSourceAudio * (num_out_channels == 1 ? 1.0f : ch == 0 ? (mCurrentSourcePan > 0 ? 1-mCurrentSourcePan : 1) : (mCurrentSourcePan < 0 ? 1+mCurrentSourcePan : 1)));
        }

        float meanSquare = 0;
        for (int ch = 0; ch < num_source_channel; ++ch) {
            const float rms = source_buffer.getRMSLevel(ch, playhead_index, num_samples);
            meanSquare += rms * rms;
        }
        mSourceMeanSquare.store(meanSquare * mGainSourceAudio * mGainSourceAudio / juce::jmax(1,num_source_channel));
        playhead_index += num_samples;

        if (playhead_index >= source_buffer.getNumSamples()) {
            playhead_index = 0;
        }

        mPlayheadTime = static_cast<double>(playhead_index) / mSampleRate;
    }

    // Ramped across the block, like the source gain above.
    for (int ch = 0; ch < num_out_channels; ch++) {
        mInternalBuffer.applyGainRamp(ch, 0, inAudioBuffer.getNumSamples(), old_master_gain, mMasterGain);
    }

    // Metered here rather than on the host's buffer, and after the fader: this is what comes out
    // of NeuralNote. Whatever the host sent in is passing through and is not ours to show.
    {
        const int num_metered = std::min(inAudioBuffer.getNumSamples(), mInternalBuffer.getNumSamples());
        const float* const left = mInternalBuffer.getReadPointer(0);
        const float* const right = mInternalBuffer.getNumChannels() > 1 ? mInternalBuffer.getReadPointer(1) : left;

        for (int i = 0; i < num_metered; i++) {
            mMasterMeter.push((left[i] + right[i]) * 0.5f);
        }

        mMasterMeanSquare.store(static_cast<float>(mMasterMeter.getMeanSquare()), std::memory_order_relaxed);
        mMeterFrame.fetch_add(1, std::memory_order_relaxed);
    }

    for (int ch = 0; ch < num_out_channels; ch++) {
        inAudioBuffer.addFrom(ch, 0, mInternalBuffer, ch, 0, inAudioBuffer.getNumSamples());
    }
}

bool Player::isPlaying() const
{
    return mIsPlaying.load();
}

void Player::previewKey(int note)
{
    if (note >= 0 && note <= 127) {
        const ScopedLock lock(mProcessor->getCallbackLock());
        int program = (int)mProcessor->getValueTree().getProperty("activePreviewProgram", 0);
        if (!mProcessor->getInstrumentMixer()->findEntry(program)) program = 0;
        mPreviewSynth->ensureInstrument(program);
        mPreviewSynth->setGainDb(program, mProcessor->getInstrumentMixer()->getGainDb(program));
        mPreviewSynth->setPan(program, mProcessor->getInstrumentMixer()->getPan(program));
        mPreviewProgram.store(program);
    }
    mPreviewKey.store(note >= 0 && note <= 127 ? note : -1);
}

void Player::setPlayingState(bool inIsPlaying)
{
    mIsPlaying.store(inIsPlaying);

    if (!inIsPlaying) {
        // Through the scheduler, so the same note-offs also reach the MIDI output port.
        mSynthController->stopAllNotes();
        mShouldSilenceSynth = true;
    }
}

void Player::returnToStart()
{
    setPlayingState(false);
    setPlayheadPositionSeconds(0.0);
}

void Player::reset()
{
    mSynthController->reset();
    setPlayingState(false);
    mPlayheadTime = 0;

    // The published value only, not the window behind it: this runs without the callback lock, so
    // the audio thread may be inside push(). The window drains itself over the next few blocks.
    mMasterMeanSquare.store(0.0f, std::memory_order_relaxed);
}

InstrumentSynth* Player::getInstrumentSynth() const
{
    return mSynth.get();
}

double Player::getPlayheadPositionSeconds() const
{
    return mSynthController->getCurrentTimeSeconds();
}

void Player::setPlayheadPositionSeconds(double inNewPosition)
{
    if (inNewPosition >= 0 && inNewPosition < mProcessor->getSourceAudioManager()->getAudioSampleDuration()) {
        mSynthController->setNewTimeSeconds(inNewPosition);
        mPlayheadTime = inNewPosition;

        // A drum hit that started before the seek has no note-off the synth will act on, so it
        // would otherwise carry across the jump.
        mShouldSilenceSynth = true;
    }
}

SynthController* Player::getSynthController() const
{
    return mSynthController.get();
}

void Player::saveStateToValueTree()
{
    mProcessor->getValueTree().setPropertyExcludingListener(this, NnId::PlayheadPositionSecId, mPlayheadTime, nullptr);
}

void Player::valueTreePropertyChanged(ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
{
    if (property == NnId::PlayheadPositionSecId) {
        double new_position = treeWhosePropertyHasChanged.getProperty(property);

        if (mProcessor->getState() != EmptyAudioAndMidiRegions) {
            new_position = std::clamp(new_position, 0.0, mProcessor->getSourceAudioManager()->getAudioSampleDuration());
        } else {
            new_position = 0.0;
        }

        setPlayheadPositionSeconds(new_position);
    }

    if (property == NnId::MidiOut) {
        mShouldOutputMidi = treeWhosePropertyHasChanged.getProperty(property);
    }

    if (property == NnId::LoopEnabledId) {
        mShouldLoop = static_cast<bool>(treeWhosePropertyHasChanged.getProperty(property));
    }
}

void Player::_setGains(float inMix, float inMasterGainDb)
{
    juce::ignoreUnused(inMix); // Kept in old sessions; explicit track routing replaces the crossfade.
    mGainSourceAudio = mSourceEnabled.load() ? Decibels::decibelsToGain(mSourceGainDb.load(), MIN_INF_GAIN_DB) : 0.0f;
    mCurrentSourcePan = mSourcePan.load();
    mGainSynth = 1.0f;
    mMasterGain = Decibels::decibelsToGain(inMasterGainDb, MIN_INF_GAIN_DB);
}

void Player::setSourceMix(float gainDb, float pan, bool enabled)
{
    mSourceGainDb.store(gainDb);
    mSourcePan.store(juce::jlimit(-1.0f,1.0f,pan));
    mSourceEnabled.store(enabled);
}
