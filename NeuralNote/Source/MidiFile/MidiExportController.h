#pragma once
#include "PluginProcessor.h"
#include "MidiFileWriter.h"

/** One async export workflow shared by the file menu and track context menus. */
class MidiExportController : public juce::Component
{
public:
    explicit MidiExportController(NeuralNoteAudioProcessor& processor) : mProcessor(processor) {}
    void start(const std::vector<int>& programs, bool separate);
private:
    struct Job {
        std::vector<NoteEvent> notes;
        std::map<int, MidiMixSettings> mix;
        double bpm = 120, offset = 0;
        MidiOverflowMode overflow = MidiOverflowMode::ReuseChannels;
        bool separate = false, includeMix = false;
        juce::String source;
        std::unique_ptr<juce::FileChooser> chooser;
        std::vector<std::pair<juce::File, std::vector<NoteEvent>>> files;
    };
    void choose(std::shared_ptr<Job> job);
    void write(std::shared_ptr<Job> job, bool skipExisting);
    NeuralNoteAudioProcessor& mProcessor;
    bool mBusy = false;
};
