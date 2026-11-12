//
// Created by Damien Ronssin on 11.08.26.
//

#include "NNFileUtils.h"

#include <algorithm>

#include "ModelManifest.h"

namespace NNFileUtils
{

juce::File getNeuralNoteDirectory()
{
#if JUCE_WINDOWS
    // MSIX parents (including Codex) virtualise Roaming AppData for child processes.
    // A profile-root directory has the same physical identity for desktop and packaged launches.
    static const juce::File directory = [] {
        const auto profile = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        const auto target = profile.getChildFile(".neuralnote");
        const auto legacy = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NeuralNote");
        if (target.createDirectory().failed()) return legacy;
        juce::Array<juce::File> sources;
        sources.add(legacy);
        sources.add(profile.getChildFile("AppData/Roaming/NeuralNote"));
        for (const auto& package : profile.getChildFile("AppData/Local/Packages").findChildFiles(juce::File::findDirectories, false, "OpenAI.Codex_*"))
            sources.add(package.getChildFile("LocalCache/Roaming/NeuralNote"));
        // Copy preferences only; leave large model files, recordings and old sessions intact.
        // Existing canonical preferences always take precedence, so migration runs only once per file.
        for (const auto* name : {"model-directory-small.txt", "model-directory-medium.txt", "model-directory-large.txt", "global.settings", "NeuralNote.settings"}) {
            const auto destination = target.getChildFile(name);
            if (destination.existsAsFile()) continue;
            juce::File newest;
            for (const auto& source : sources) {
                const auto candidate = source.getChildFile(name);
                if (candidate.getSize() > 0 && (!newest.existsAsFile() || candidate.getLastModificationTime() > newest.getLastModificationTime()))
                    newest = candidate;
            }
            if (newest.existsAsFile()) newest.copyFileTo(destination);
        }
        for (const auto size : ALL_MODEL_SIZES) {
            const auto locationFile = target.getChildFile(juce::String("model-directory-") + modelSizeToString(size) + ".txt");
            if (locationFile.existsAsFile()) continue;
            const auto& manifest = getModelManifestEntry(size);
            for (const auto& source : sources) {
                const auto folder = source.getChildFile("models");
                if (folder.getChildFile(manifest.fileName).getSize() == manifest.numBytes) {
                    locationFile.replaceWithText(folder.getFullPathName()); break;
                }
            }
        }
        return target;
    }();
    return directory;
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NeuralNote");
#endif

}

juce::File getModelsDirectory()
{
    return getNeuralNoteDirectory().getChildFile("models");
}

juce::File getModelDirectory(ModelSize inModelSize)
{
    const auto path = getNeuralNoteDirectory()
                          .getChildFile(juce::String("model-directory-") + modelSizeToString(inModelSize) + ".txt")
                          .loadFileAsString().trim();
    return juce::File::isAbsolutePath(path) ? juce::File(path) : getModelsDirectory();
}

bool setModelDirectory(ModelSize inModelSize, const juce::File& inDirectory, bool inRequireWritable)
{
    if ((inRequireWritable && (!ensureDirectoryExists(inDirectory) || !inDirectory.hasWriteAccess()))
        || !inDirectory.isDirectory() || !ensureDirectoryExists(getNeuralNoteDirectory())) {
        return false;
    }
    return getNeuralNoteDirectory()
        .getChildFile(juce::String("model-directory-") + modelSizeToString(inModelSize) + ".txt")
        .replaceWithText(inDirectory.getFullPathName());
}

juce::File suggestModelDownloadDirectory(ModelSize inModelSize)
{
    const auto usable = [](const juce::File& directory) {
        return directory.isDirectory() && directory.hasWriteAccess();
    };
    // Keep explicit choices and legacy installed/partial models working. Merely opening
    // a picker must not change where an existing model is loaded from.
    const auto previous = getModelDirectory(inModelSize);
    if (usable(previous) && (previous != getModelsDirectory()
                            || isModelInstalled(inModelSize) || getModelPartFile(inModelSize).existsAsFile()))
        return previous;
    for (const auto size : ALL_MODEL_SIZES) {
        const auto shared = getModelDirectory(size);
        if (shared != getModelsDirectory() && usable(shared)) return shared;
    }
#if JUCE_WINDOWS
    juce::Array<juce::File> roots;
    juce::File::findFileSystemRoots(roots);
    std::sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) {
        return a.getFullPathName().compareIgnoreCase(b.getFullPathName()) < 0;
    });
    const auto systemDirectory = juce::File::getSpecialLocation(juce::File::windowsSystemDirectory);
    for (const auto& root : roots) {
        // Avoid both C: and the actual Windows volume (Windows need not be on C:).
        // Do not auto-select disconnected, network, optical or removable media.
        if (root.getFullPathName().startsWithIgnoreCase("C:")
            || systemDirectory.isAChildOf(root) || !root.isDirectory()
            || !root.isOnHardDisk() || root.isOnRemovableDrive()
            || root.getBytesFreeOnVolume() < getModelManifestEntry(inModelSize).numBytes)
            continue;
        const auto candidate = root.getChildFile("NeuralNote").getChildFile("models");
        if (!candidate.exists() || usable(candidate)) return candidate;
    }
#endif
    // A machine may have only one drive. Use a per-user writable location instead
    // of demanding administrator access to its root. The picker still allows any path.
    const auto fallback = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getChildFile("NeuralNote").getChildFile("models");
    if (!fallback.exists() || usable(fallback)) return fallback;
    const auto legacy = getModelsDirectory();
    if (!legacy.exists() || usable(legacy)) return legacy;
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

juce::File getGlobalSettingsFile()
{
    return getNeuralNoteDirectory().getChildFile("global.settings");
}

juce::File getRecordingsDirectory()
{
    return getNeuralNoteDirectory().getChildFile("recordings");
}

juce::File getCrashReportsDirectory()
{
    return getNeuralNoteDirectory().getChildFile("crashes");
}

juce::File getLogsDirectory()
{
    return getNeuralNoteDirectory().getChildFile("logs");
}

juce::File getMidiDragDirectory()
{
    return juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory).getChildFile("neuralnote");
}

juce::File getDefaultMidiExportDirectory()
{
    return juce::File::getSpecialLocation(juce::File::SpecialLocationType::userMusicDirectory);
}

bool ensureDirectoryExists(const juce::File& inDirectory)
{
    return inDirectory.isDirectory() || inDirectory.createDirectory().wasOk();
}

juce::File getModelFile(ModelSize inModelSize)
{
    return getModelDirectory(inModelSize).getChildFile(getModelManifestEntry(inModelSize).fileName);
}

bool isModelInstalled(ModelSize inModelSize)
{
    // getSize is 0 for a file that does not exist, and no checkpoint is empty.
    return getModelFile(inModelSize).getSize() == getModelManifestEntry(inModelSize).numBytes;
}

bool isAnyModelInstalled()
{
    return std::any_of(ALL_MODEL_SIZES.begin(), ALL_MODEL_SIZES.end(), isModelInstalled);
}

std::optional<ModelSize> getInstalledModelSize(ModelSize inPreferred)
{
    if (isModelInstalled(inPreferred)) {
        return inPreferred;
    }

    if (isModelInstalled(DEFAULT_MODEL_SIZE)) {
        return DEFAULT_MODEL_SIZE;
    }

    for (const ModelSize size: ALL_MODEL_SIZES) {
        if (isModelInstalled(size)) {
            return size;
        }
    }

    return std::nullopt;
}

juce::File getModelPartFile(ModelSize inModelSize)
{
    return getModelPartFile(inModelSize, getModelDirectory(inModelSize));
}

juce::File getModelPartFile(ModelSize inModelSize, const juce::File& inDirectory)
{
    const ModelManifestEntry& entry = getModelManifestEntry(inModelSize);

    return inDirectory.getChildFile(juce::String(entry.fileName) + "."
                                             + juce::String(entry.sha256).substring(0, 8) + ".part");
}

juce::int64 getModelDownloadBytesRequired(ModelSize inModelSize, const juce::File& inDirectory,
                                        bool inReplaceInstalled)
{
    const auto& entry = getModelManifestEntry(inModelSize);
    if (!inReplaceInstalled && inDirectory.getChildFile(entry.fileName).getSize() == entry.numBytes) return 0;
    const auto partBytes = getModelPartFile(inModelSize, inDirectory).getSize();
    return entry.numBytes - (partBytes <= entry.numBytes ? partBytes : 0);
}

void openDirectory(const juce::File& inDirectory)
{
    if (ensureDirectoryExists(inDirectory)) {
        inDirectory.startAsProcess();
    }
}

bool isRecordingFile(const juce::File& inFile)
{
    // The name is checked as well as the parent, so an audio file the user dropped from that
    // directory is never mistaken for one of ours.
    return inFile.getParentDirectory() == getRecordingsDirectory()
           && inFile.getFileName().startsWith(RECORDING_FILENAME_PREFIX);
}

std::filesystem::path toPath(const juce::File& inFile)
{
#if JUCE_WINDOWS
    return {inFile.getFullPathName().toWideCharPointer()};
#else
    return {inFile.getFullPathName().toStdString()};
#endif
}

juce::String getMidiExportFileName(const juce::String& inSourceAudioFileName)
{
    if (inSourceAudioFileName.isEmpty()) {
        return "NNTranscription.mid";
    }

    return inSourceAudioFileName + "_NNTranscription.mid";
}

} // namespace NNFileUtils
