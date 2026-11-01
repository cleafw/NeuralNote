#include "PluginProcessor.h"
#include "PianoRoll.h"
#include "NNFileUtils.h"
#include "Sidebar.h"
#include "NnEditorConstrainer.h"
#include "NeuralNoteMainView.h"
#include "ComputeDevices.h"
#include <cstdio>

static juce::MouseEvent eventFor(juce::Component& component, juce::Point<float> position, int buttons)
{
    return {juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys(buttons),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, juce::Time::getCurrentTime(),
            position, juce::Time::getCurrentTime(), 1, false};
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1 && (juce::String(argv[1]) == "--model-paths" || juce::String(argv[1]) == "--model-paths-file")) {
        juce::String report = "data=" + NNFileUtils::getNeuralNoteDirectory().getFullPathName() + "\n";
        for (const auto size : ALL_MODEL_SIZES) {
            const auto file = NNFileUtils::getModelFile(size);
            report += juce::String(modelSizeToString(size)) + " | " + file.getFullPathName() + " | installed=" + juce::String((int)NNFileUtils::isModelInstalled(size)) + "\n";
            std::printf("%s | %s | %lld bytes | installed=%d\n", modelSizeToString(size),
                file.getFullPathName().toRawUTF8(), static_cast<long long>(file.getSize()),
                NNFileUtils::isModelInstalled(size));
        }
        report += "legacyConfigBytes=" + juce::String(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NeuralNote/model-directory-small.txt").getSize()) + "\n";
        if (argc > 2 && juce::String(argv[1]) == "--model-paths-file") juce::File(juce::String::fromUTF8(argv[2])).replaceWithText(report);
        return 0;
    }
    int failures = 0;
    const auto check = [&](bool passed, const char* what) {
        std::printf("%s: %s\n", passed ? "PASS" : "FAIL", what);
        if (!passed) ++failures;
    };
    NeuralNoteAudioProcessor processor;
    Keyboard keyboard;
    keyboard.setVisible(true);
    keyboard.setSize(60, 200);
    keyboard.setWhiteKeyHeight(20);
    keyboard.setDisplayedRange({48, 72});
    PianoRoll roll(&processor, keyboard, 100);
    juce::Viewport viewport;
    viewport.setSize(400, 200);
    roll.setSize(2000, 200);
    viewport.setViewedComponent(&roll, false);
    viewport.setViewPosition(200, 0);
    const auto startPitch = keyboard.getLowestVisibleKey();
    roll.mouseDown(eventFor(roll, {300, 80}, juce::ModifierKeys::middleButtonModifier));
    roll.mouseDrag(eventFor(roll, {260, 110}, juce::ModifierKeys::middleButtonModifier));
    check(viewport.getViewPositionX() == 240, "middle drag pans time in grab direction");
    check(keyboard.getLowestVisibleKey() > startPitch, "middle drag pans pitch with keyboard");
    roll.mouseUp(eventFor(roll, {260, 110}, 0));
    const auto stoppedX = viewport.getViewPositionX();
    roll.mouseDrag(eventFor(roll, {0, 0}, 0));
    check(viewport.getViewPositionX() == stoppedX, "release ends panning");
    roll.mouseDown(eventFor(roll, {300, 80}, juce::ModifierKeys::middleButtonModifier));
    roll.mouseDrag(eventFor(roll, {10000, 80}, juce::ModifierKeys::middleButtonModifier));
    check(viewport.getViewPositionX() == 0, "pan clamps at audio start");
    roll.mouseUp(eventFor(roll, {0, 0}, 0));

    int previewNote = -1;
    keyboard.onPreviewKey = [&](int note) { previewNote = note; };
    keyboard.setLowestVisibleKey(60);
    const int expectedNote = 64;
    const auto keyPosition = juce::Point<float>(55, keyboard.getRectangleForKey(expectedNote).getCentreY());
    keyboard.mouseDown(eventFor(keyboard, keyPosition, juce::ModifierKeys::leftButtonModifier));
    check(expectedNote >= 0 && previewNote == expectedNote, "key hit testing starts correct pitch");
    const auto blackPosition = keyboard.getRectangleForKey(61).getCentre();
    keyboard.mouseDrag(eventFor(keyboard, blackPosition, juce::ModifierKeys::leftButtonModifier));
    check(previewNote == 61, "drag to black key uses its pitch, not the white key underneath");
    keyboard.mouseDrag(eventFor(keyboard, {-10, -10}, juce::ModifierKeys::leftButtonModifier));
    check(previewNote == -1, "drag outside keyboard releases preview");
    keyboard.mouseUp(eventFor(keyboard, keyPosition, 0));
    check(previewNote == -1, "key release stops preview");
    keyboard.onPreviewKey = {};

    auto* player = processor.getPlayer();
    player->prepareToPlay(48000, 512);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    float peak = 0;
    // Font decoding is asynchronous. Re-requesting under the callback lock also
    // checks that preview becomes usable once its font has finished loading.
    for (int attempt = 0; attempt < 100 && peak == 0; ++attempt) {
        player->previewKey(-1);
        audio.clear(); player->processBlock(audio, midi);
        player->previewKey(60);
        for (int i = 0; i < 4; ++i) {
            audio.clear(); player->processBlock(audio, midi);
            peak = juce::jmax(peak, audio.getMagnitude(0, 512));
        }
        if (peak == 0) juce::Thread::sleep(20);
    }
    check(peak > 0.0001f, "piano preview produces audio while transport is stopped");
    check(!player->isPlaying(), "preview leaves transport stopped");
    check(midi.isEmpty(), "preview does not modify exported MIDI stream");
    player->previewKey(-1);
    for (int i = 0; i < 1000; ++i) { audio.clear(); player->processBlock(audio, midi); }
    check(audio.getMagnitude(0, 512) < 0.0001f, "released preview decays to silence");

    const std::vector<NoteEvent> fixture {{0.25,1.0,60,0.5,0}, {0.5,2.0,64,0.6,2}, {1.0,1.2,36,0.7,msl::DRUM_PROGRAM}};
    processor.getInstrumentMixer()->rebuildFromNotes(fixture);
    Sidebar sidebar(processor);
    const auto transcriptionChoice = processor.getValueTree().getProperty(NnId::SelectedInstrumentGroupsId);
    sidebar.selectTrack(0, {});
    check(sidebar.getSelectedPrograms() == std::vector<int>{0}, "single track selection");
    sidebar.selectTrack(2, juce::ModifierKeys(juce::ModifierKeys::ctrlModifier));
    check(sidebar.getSelectedPrograms() == std::vector<int>({0,2}), "Ctrl adds a selected track");
    sidebar.selectTrack(0, juce::ModifierKeys(juce::ModifierKeys::ctrlModifier));
    check(sidebar.getSelectedPrograms() == std::vector<int>{2}, "Ctrl removes a selected track");
    sidebar.selectTrack(msl::DRUM_PROGRAM, juce::ModifierKeys(juce::ModifierKeys::shiftModifier));
    check(sidebar.getSelectedPrograms() == std::vector<int>({0,2,msl::DRUM_PROGRAM}), "Shift selects contiguous rows from anchor");
    check(processor.getValueTree().getProperty(NnId::SelectedInstrumentGroupsId) == transcriptionChoice, "track selection leaves transcription choice unchanged");
    sidebar.selectTrack(2, {});
    auto* mixer = processor.getInstrumentMixer();
    check(mixer->getPan(2) == 0, "old session pan defaults to centre");
    mixer->setPan(2, -1);
    mixer->setMuted(2, true);
    player->previewKey(60);
    for (int i=0;i<20;++i) { audio.clear(); player->processBlock(audio,midi); }
    check(audio.getMagnitude(0,0,512) > 0.0001f && audio.getMagnitude(1,0,512) < 0.00001f, "selected electric piano previews left even when its track is muted");
    mixer->setPan(2, 1);
    for (int i=0;i<20;++i) { audio.clear(); player->processBlock(audio,midi); }
    check(audio.getMagnitude(1,0,512) > 0.0001f && audio.getMagnitude(0,0,512) < 0.00001f, "preview pan updates smoothly to right");
    mixer->setGainDb(2, -100);
    for (int i=0;i<20;++i) { audio.clear(); player->processBlock(audio,midi); }
    check(audio.getMagnitude(0,512) < 0.00001f, "preview follows selected instrument gain");
    player->previewKey(-1);
    mixer->setGainDb(2, -6);
    juce::MemoryBlock session;
    processor.getStateInformation(session);
    { NeuralNoteAudioProcessor restored; restored.setStateInformation(session.getData(),(int)session.getSize());
      check(restored.getInstrumentMixer()->getPan(2) == 1, "session restores instrument pan");
      check((int)restored.getValueTree().getProperty("activePreviewProgram") == 2, "session restores active preview instrument");
      check(restored.getValueTree().getProperty("selectedTrackPrograms").toString() == "2", "session restores track selection"); }

    auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("NeuralNote-export-test", ".mid");
    const auto filtered = MidiFileWriter::selectPrograms(fixture,{2,msl::DRUM_PROGRAM});
    check(filtered.size() == 2 && filtered[0] == fixture[1] && filtered[1] == fixture[2], "selected export preserves notes and timings");
    for (bool includeMix : {false,true}) {
        const std::map<int,MidiMixSettings> mix = includeMix ? std::map<int,MidiMixSettings>{{2,{-6,-1}},{msl::DRUM_PROGRAM,{0,1}}} : std::map<int,MidiMixSettings>{};
        check(MidiFileWriter().writeMidiFile(filtered,temp,120,0.5,MidiOverflowMode::ReuseChannels,mix), "write selected multitrack MIDI");
        juce::FileInputStream input(temp); juce::MidiFile file;
        check(file.readFrom(input) && file.getNumTracks() == 3, "read back conductor and two selected tracks");
        int notes=0, volume=0, pan=0; bool drumChannel=false, offset=false, mixValues=true;
        for(int t=0;t<file.getNumTracks();++t) for(int i=0;i<file.getTrack(t)->getNumEvents();++i) {
            const auto& m=file.getTrack(t)->getEventPointer(i)->message;
            if(m.isNoteOn()) { ++notes; drumChannel |= m.getChannel()==10; offset |= m.getNoteNumber()==64 && std::abs(m.getTimeStamp()-1920.0)<1 && std::abs(m.getFloatVelocity()-0.6f)<0.01f; }
            if(m.isController()) { volume += m.getControllerNumber()==7; pan += m.getControllerNumber()==10;
                if(m.getControllerNumber()==7) mixValues &= m.getControllerValue()==(m.getChannel()==10?100:50);
                if(m.getControllerNumber()==10) mixValues &= m.getControllerValue()==(m.getChannel()==10?127:0); }
        }
        check(notes==2 && drumChannel && offset, "MIDI preserves selection, percussion channel and export offset");
        check(mixValues && volume==(includeMix?2:0) && pan==(includeMix?2:0), "mix CCs are opt-in");
    }
    for(const auto& note: fixture) {
        check(MidiFileWriter().writeMidiFile(MidiFileWriter::selectPrograms(fixture,{note.program}),temp,120,0,MidiOverflowMode::ReuseChannels), "write one instrument per file");
        juce::FileInputStream input(temp); juce::MidiFile file; check(file.readFrom(input)&&file.getNumTracks()==2,"single instrument export has one musical track");
    }
    temp.deleteFile();
    NnEditorConstrainer constrainer; constrainer.configure(nullptr);
    check(constrainer.getFixedAspectRatio() == 0, "window aspect ratio is unrestricted");
    { NeuralNoteMainView view(processor); view.setSize(1280,800);
      Sidebar* tracks=nullptr; VisualizationPanel* panel=nullptr;
      for(auto* child:view.getChildren()) { if(auto* p=dynamic_cast<Sidebar*>(child)) tracks=p; if(auto* p=dynamic_cast<VisualizationPanel*>(child)) panel=p; }
      const int trackWidth=tracks->getWidth(), contentWidth=panel->getWidth();
      view.setSize(1600,950);
      check(tracks->getWidth()==trackWidth && panel->getWidth()==contentWidth+320,"larger window expands content without scaling sidebar");
      view.setSize(960,600);
      bool contained=true;
      for(auto* child:view.getChildren()) if(auto* top=dynamic_cast<TopBar*>(child))
          for(auto* control:top->getChildren()) if(control->isVisible()) contained &= top->getLocalBounds().contains(control->getBounds());
      check(contained && tracks->getWidth()==trackWidth,"minimum window wraps toolbar instead of shrinking controls"); }
    const auto& devices = ComputeDevices::get();
    bool gpu=false; for(const auto& device:devices) gpu |= device.backend == "Vulkan";
    check(gpu,"Vulkan GPU remains available");

    { juce::Component host, editor; host.addChildComponent(editor);
      NnEditorConstrainer constraints; constraints.configure(&editor);
      host.getProperties().set("nnPresentationFullScreen", true);
      juce::Rectangle<int> fullBounds(0,0,4000,2500);
      constraints.checkBounds(fullBounds, {0,0,1280,800}, {0,0,4000,2500}, false,false,true,true);
      check(fullBounds.getWidth()==4000 && fullBounds.getHeight()==2500,"full screen bypasses normal work-area size limits"); }
    auto sourceFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("NeuralNote-source-routing", ".wav");
    {
        juce::WavAudioFormat format;
        auto stream = sourceFile.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000,2,16,{},0));
        check(writer != nullptr,"create original audio routing fixture");
        if(writer) { stream.release(); juce::AudioBuffer<float> data(2,48000);
            for(int ch=0;ch<2;++ch) juce::FloatVectorOperations::fill(data.getWritePointer(ch),0.1f,48000);
            writer->writeFromAudioSampleBuffer(data,0,48000); }
    }
    processor.setRateAndBufferSizeDetails(48000,512);
    processor.prepareToPlay(48000,512);
    check(processor.getSourceAudioManager()->onFileDrop(sourceFile.getFullPathName()),"load original audio without a transcription model");
    mixer->rebuildFromNotes(fixture);
    auto scheduled = fixture; player->getSynthController()->setNotes(scheduled);
    mixer->setGainDb(SOURCE_AUDIO_TRACK,0); mixer->setPan(SOURCE_AUDIO_TRACK,0);
    mixer->setMuted(SOURCE_AUDIO_TRACK,false); mixer->setSoloed(SOURCE_AUDIO_TRACK,true);
    check(mixer->isAudible(SOURCE_AUDIO_TRACK) && !mixer->isAudible(0),"solo original audio excludes instrument playback");
    player->setPlayingState(true);
    for(int i=0;i<12;++i) { audio.clear(); player->processBlock(audio,midi); }
    check(audio.getMagnitude(0,512)>0.05f,"original audio plays through its own track");
    mixer->setMuted(SOURCE_AUDIO_TRACK,true);
    for(int i=0;i<12;++i) { audio.clear(); player->processBlock(audio,midi); }
    check(audio.getMagnitude(0,512)<0.0001f,"original track playback switch silences original audio");
    mixer->setMuted(SOURCE_AUDIO_TRACK,false); mixer->setSoloed(SOURCE_AUDIO_TRACK,false);
    mixer->setSoloed(0,true);
    check(!mixer->isAudible(SOURCE_AUDIO_TRACK) && mixer->isAudible(0),"solo instrument excludes original audio");
    mixer->setSoloed(SOURCE_AUDIO_TRACK,true);
    check(mixer->isAudible(SOURCE_AUDIO_TRACK) && mixer->isAudible(0),"original and selected instruments can be soloed together");
    mixer->setSoloed(0,false); mixer->setSoloed(SOURCE_AUDIO_TRACK,false);
    mixer->setMuted(0,false);
    check(mixer->isAudible(SOURCE_AUDIO_TRACK) && mixer->isAudible(0),"original and instruments can play together without a crossfade");
    mixer->setPan(SOURCE_AUDIO_TRACK,-1); mixer->setSoloed(SOURCE_AUDIO_TRACK,true);
    for(int i=0;i<12;++i) { audio.clear(); player->processBlock(audio,midi); }
    check(audio.getMagnitude(0,0,512)>0.05f && audio.getMagnitude(1,0,512)<0.0001f,"original track pan controls its stereo output");
    mixer->setMuted(SOURCE_AUDIO_TRACK,true);
    mixer->resetStoredSettings();
    check(mixer->isMuted(SOURCE_AUDIO_TRACK) && mixer->getPan(SOURCE_AUDIO_TRACK)==-1,"new transcription preserves original track controls");
    juce::MemoryBlock sourceSession; processor.getStateInformation(sourceSession);
    { NeuralNoteAudioProcessor restored; restored.setStateInformation(sourceSession.getData(),(int)sourceSession.getSize());
      check(restored.getInstrumentMixer()->isMuted(SOURCE_AUDIO_TRACK) && restored.getInstrumentMixer()->getPan(SOURCE_AUDIO_TRACK)==-1,"session restores original track playback and pan"); }
    check(mixer->findEntry(SOURCE_AUDIO_TRACK)==nullptr,"original track is not a MIDI instrument");
    player->setPlayingState(false);
    sourceFile.deleteFile();
    viewport.setViewedComponent(nullptr, false);
    return failures == 0 ? 0 : 1;
}
