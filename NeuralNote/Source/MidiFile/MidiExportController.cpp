#include "MidiExportController.h"
#include "InstrumentInfo.h"
#include "NNFileUtils.h"
#include "NnLocalisation.h"
#include <set>

void MidiExportController::start(const std::vector<int>& programs, bool separate)
{
    if (mBusy || mProcessor.getState() == Processing || mProcessor.getState() == Recording) return;
    auto job = std::make_shared<Job>();
    const std::set<int> selected(programs.begin(), programs.end());
    job->notes = MidiFileWriter::selectPrograms(mProcessor.getTranscriptionManager()->getNoteEventVector(), programs);
    if (job->notes.empty()) return;
    for (int program : selected)
        job->mix[program] = {mProcessor.getInstrumentMixer()->getGainDb(program), mProcessor.getInstrumentMixer()->getPan(program)};
    job->separate = separate;
    job->source = juce::File::createLegalFileName(mProcessor.getSourceAudioManager()->getDroppedFilename());
    if (job->source.isEmpty()) job->source = "NeuralNote";
    job->bpm = mProcessor.getValueTree().getProperty(NnId::ExportTempoId, 120.0);
    job->offset = mProcessor.getSourceAudioManager()->getExportStartOffsetSeconds();
    job->overflow = static_cast<MidiOverflowMode>((int)mProcessor.getValueTree().getProperty(NnId::MidiOverflowModeId, 0));
    std::set<int> melodic;
    for (const auto& note : job->notes) if (note.program != msl::DRUM_PROGRAM) melodic.insert(note.program);
    auto message = nn::tr("Choose whether to include instrument volume and pan in the MIDI.");
    if (!separate && melodic.size() > 15)
        message += "\n\n" + nn::tr("More than 15 melodic instruments: the configured MIDI channel overflow rule will apply. Shared channels also share instrument and mix controls.");
    auto* dialog = new juce::AlertWindow(nn::tr("Export MIDI"), message, juce::MessageBoxIconType::NoIcon);
    auto* checkbox = new juce::ToggleButton(nn::tr("Include volume and pan"));
    // AlertWindow does not own custom components; the job's callback owns this one.
    auto checkboxOwner = std::shared_ptr<juce::ToggleButton>(checkbox);
    checkbox->setSize(340, 26);
    dialog->addCustomComponent(checkbox);
    dialog->addButton(nn::tr("Export"), 1);
    dialog->addButton(nn::tr("Cancel"), 0);
    mBusy = true;
    const juce::Component::SafePointer<MidiExportController> safe(this);
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe,job,checkboxOwner](int result) {
        if (!safe) return;
        if (result != 1) { safe->mBusy = false; return; }
        job->includeMix = checkboxOwner->getToggleState();
        safe->choose(job);
    }), true);
}

void MidiExportController::choose(std::shared_ptr<Job> job)
{
    auto initial = NNFileUtils::getDefaultMidiExportDirectory();
    if (!job->separate) initial = initial.getChildFile(job->source + "_NeuralNote.mid");
    job->chooser = std::make_unique<juce::FileChooser>(nn::tr("Export MIDI"), initial, job->separate ? "" : "*.mid", true);
    const juce::Component::SafePointer<MidiExportController> safe(this);
    const int chooserFlags = job->separate ? juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories
        : juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
    job->chooser->launchAsync(chooserFlags, [safe,job](const juce::FileChooser& chooser) {
        juce::MessageManager::callAsync([job] { job->chooser.reset(); });
        if (!safe) return;
        const auto destination = chooser.getResult();
        if (destination == juce::File()) { safe->mBusy = false; return; }
        if (job->separate) {
            std::map<int, std::vector<NoteEvent>> grouped;
            for (const auto& note : job->notes) grouped[note.program].push_back(note);
            for (auto& [program, notes] : grouped) {
                const auto name = juce::File::createLegalFileName(job->source + "_" + nn::tr(instrumentDisplayFor(program).name)
                    + "_" + juce::String(program) + ".mid");
                job->files.emplace_back(destination.getChildFile(name), std::move(notes));
            }
        } else job->files.emplace_back(destination.withFileExtension("mid"), job->notes);
        bool collision = false;
        for (const auto& file : job->files) collision |= file.first.existsAsFile();
        if (job->separate && collision) {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle(nn::tr("Files already exist"))
                .withMessage(nn::tr("Choose how to handle existing MIDI files."))
                .withButton(nn::tr("Overwrite")).withButton(nn::tr("Skip existing")).withButton(nn::tr("Cancel")),
                [safe,job](int result) { if (!safe) return; if (result == 1 || result == 2) safe->write(job, result == 2); else safe->mBusy = false; });
        } else safe->write(job, false);
    });
}

void MidiExportController::write(std::shared_ptr<Job> job, bool skipExisting)
{
    juce::StringArray failed;
    for (const auto& [file, notes] : job->files) {
        if (skipExisting && file.existsAsFile()) continue;
        if (!MidiFileWriter().writeMidiFile(notes, file, job->bpm, job->offset, job->overflow,
                                           job->includeMix ? job->mix : std::map<int,MidiMixSettings>{}))
            failed.add(file.getFullPathName());
    }
    mBusy = false;
    if (!failed.isEmpty())
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, nn::tr("Export failed"), failed.joinIntoString("\n"));
}
