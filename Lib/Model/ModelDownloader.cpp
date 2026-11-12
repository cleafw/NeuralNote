//
// Created by Damien Ronssin on 12.09.26.
//

#include "ModelDownloader.h"

#include <atomic>

#include "ModelManifest.h"
#include "NNFileUtils.h"
#include "NnLogger.h"
#include "NnLocalisation.h"

namespace
{
constexpr int CONNECTION_TIMEOUT_MS = 30000;

// HuggingFace answers every file with a redirect to its CDN, which honours the Range header.
constexpr int MAX_REDIRECTS = 5;

constexpr int CHUNK_BYTES = 1 << 20;

// Delays before the attempts that follow a failed one. Reset whenever an attempt writes something,
// so a long download is not given up on because of a few drops spread across it.
constexpr std::array<int, 3> RETRY_DELAYS_MS = {2000, 5000, 10000};

struct DownloadAsset {
    juce::URL url;
    juce::int64 start;
    juce::int64 size;
};

DownloadAsset getDownloadAsset(ModelSize inModelSize, ModelDownloader::Source inSource, juce::int64 inOffset)
{
    const auto& entry = getModelManifestEntry(inModelSize);
    if (inSource == ModelDownloader::Source::GitHub) {
        const auto base = "https://github.com/cleafw/project-downloads/releases/download/"
                          "neuralnote-muscriptor-v1/" + juce::String(entry.fileName);
        if (inModelSize == ModelSize::Large) {
            constexpr juce::int64 firstPartBytes = 1610612736;
            if (inOffset < firstPartBytes) return {juce::URL(base + ".part01"), 0, firstPartBytes};
            return {juce::URL(base + ".part02"), firstPartBytes, entry.numBytes - firstPartBytes};
        }
        return {juce::URL(base), 0, entry.numBytes};
    }
    return {juce::URL("https://huggingface.co/" + juce::String(MODEL_REPO) + "/resolve/" + MODEL_REVISION + "/"
                      + MODEL_REPO_DIRECTORY + "/" + entry.fileName), 0, entry.numBytes};
}

bool rewindPart(const juce::File& inPart, juce::int64 inOffset)
{
    juce::FileOutputStream output(inPart);
    return output.openedOk() && output.setPosition(inOffset) && output.truncate().wasOk();
}

size_t indexOf(ModelSize inModelSize)
{
    return static_cast<size_t>(inModelSize);
}
} // namespace

class ModelDownloader::Job : public juce::Thread
{
public:
    explicit Job(ModelSize inModelSize)
        : juce::Thread("NeuralNote model download (" + juce::String(modelSizeToString(inModelSize)) + ")")
        , mModelSize(inModelSize)
    {
    }

    ~Job() override
    {
        requestStop();

        // Not bounded: a timeout here kills the thread, and verifying the large checkpoint takes
        // seconds with nothing to interrupt it.
        stopThread(-1);
    }

    bool begin(const juce::File& inDirectory, Source inSource, bool inReplaceInstalled,
               const juce::File& inLocalFile = {})
    {
        if (isThreadRunning()) {
            return false;
        }

        // Before the thread starts, so a status read in between does not see the last run's phase.
        mTargetFile = inDirectory.getChildFile(getModelManifestEntry(mModelSize).fileName);
        mSource = inSource;
        mReplaceInstalled = inReplaceInstalled;
        mLocalFile = inLocalFile;
        mPartFile = NNFileUtils::getModelPartFile(mModelSize, inDirectory);
        mDownloadedBytes = mPartFile.getSize();
        mPhase = mLocalFile == juce::File() ? Phase::Downloading : Phase::Verifying;

        // Consumes a notify() left by a stop that found no retry delay to wake.
        wait(0);

        if (startThread()) return true;
        mPhase = Phase::Idle;
        return false;
    }

    void requestStop()
    {
        signalThreadShouldExit();

        // Wakes a retry delay.
        notify();

        const juce::ScopedLock lock(mStreamLock);

        if (mStream != nullptr) {
            mStream->cancel();
        }
    }

    Status getStatus() const
    {
        Status status;
        status.phase = mPhase.load();
        status.totalBytes = getModelManifestEntry(mModelSize).numBytes;
        status.downloadedBytes =
            status.isBusy() ? mDownloadedBytes.load() : NNFileUtils::getModelPartFile(mModelSize).getSize();

        if (status.phase == Phase::Failed) {
            const juce::ScopedLock lock(mErrorLock);
            status.errorMessage = mErrorMessage;
        }

        return status;
    }

private:
    /** How one request ended. An empty failure means the part file is complete. */
    struct Attempt {
        juce::String failure;
        bool canRetry = true;
    };

    void run() override
    {
        const juce::String failure = _download();

        if (failure.isNotEmpty() && !threadShouldExit()) {
            NnLogger::write(NnLogger::Level::Warning,
                            "ModelDownloader: " + juce::String(modelSizeToString(mModelSize)) + ": " + failure);

            const juce::ScopedLock lock(mErrorLock);
            mErrorMessage = failure;
            mPhase = Phase::Failed;
        } else {
            mPhase = Phase::Idle;
        }
    }

    /** @return Why it failed, or empty once installed or cancelled. */
    juce::String _download()
    {
        const ModelManifestEntry& entry = getModelManifestEntry(mModelSize);

        if (!mReplaceInstalled && mLocalFile == juce::File() && mTargetFile.getSize() == entry.numBytes) {
            return {};
        }

        // Instances in this process share this object and cannot get here twice for one size. The
        // lock is for another process: a standalone next to a host, or a host that runs plugins out
        // of process.
        juce::InterProcessLock process_lock("NeuralNoteModelDownload_" + juce::String(modelSizeToString(mModelSize)));

        if (!process_lock.enter(0)) {
            return nn::tr("Another NeuralNote is already downloading this model");
        }

        const juce::File directory = mTargetFile.getParentDirectory();

        if (mLocalFile != juce::File()) {
            if (mLocalFile != mTargetFile || mLocalFile.getSize() != entry.numBytes
                || juce::SHA256(mLocalFile).toHexString() != entry.sha256)
                return nn::tr("This file does not match the official MuScriptor model.");
            if (threadShouldExit()) return {};
            if (!NNFileUtils::setModelDirectory(mModelSize, directory, false))
                return nn::tr("Could not save the local model location.");
            return {};
        }

        if (!NNFileUtils::ensureDirectoryExists(directory)) {
            return nn::tr("Could not create ") + directory.getFullPathName();
        }

        const juce::File part = mPartFile;
        _deleteOtherPartFiles(part);
        if (part.getSize() > entry.numBytes && !part.deleteFile()) {
            return nn::tr("Could not write to ") + part.getFullPathName();
        }

        size_t failed_attempts = 0;

        while (part.getSize() != entry.numBytes) {
            const juce::int64 size_before = part.getSize();
            const Attempt attempt = _attempt(part);

            if (threadShouldExit()) {
                return {};
            }

            if (attempt.failure.isEmpty()) {
                failed_attempts = 0;
                continue;
            }

            if (!attempt.canRetry) {
                return attempt.failure;
            }

            if (part.getSize() > size_before) {
                failed_attempts = 0;
            }

            if (failed_attempts == RETRY_DELAYS_MS.size()) {
                return attempt.failure;
            }

            wait(RETRY_DELAYS_MS[failed_attempts++]);

            if (threadShouldExit()) {
                return {};
            }
        }

        mPhase = Phase::Verifying;

        const juce::String digest = juce::SHA256(part).toHexString();

        if (threadShouldExit()) {
            return {};
        }

        if (digest != entry.sha256) {
            NnLogger::write(NnLogger::Level::Error,
                            "ModelDownloader: " + juce::String(modelSizeToString(mModelSize))
                                + ": checksum mismatch, got " + digest);
            part.deleteFile();
            return nn::tr("The download was corrupted. Try again");
        }

        // Replaces a checkpoint of the wrong size, which is what a file from another release is.
        if (!part.replaceFileIn(mTargetFile)) {
            return nn::tr("Could not move the model into ") + directory.getFullPathName();
        }
        if (mReplaceInstalled && !NNFileUtils::setModelDirectory(mModelSize, directory)) {
            return nn::tr("Could not save the local model location.");
        }

        NnLogger::write(NnLogger::Level::Info,
                        "ModelDownloader: " + juce::String(modelSizeToString(mModelSize)) + " installed");
        return {};
    }

    /** One request, from wherever the part file ends. */
    Attempt _attempt(const juce::File& inPart)
    {
        const auto absoluteOffset = inPart.getSize();
        const auto asset = getDownloadAsset(mModelSize, mSource, absoluteOffset);
        const auto offset = absoluteOffset - asset.start;
        const auto host = asset.url.getDomain();
        mDownloadedBytes = absoluteOffset;

        juce::WebInputStream stream(asset.url, false);
        stream.withConnectionTimeout(CONNECTION_TIMEOUT_MS).withNumRedirectsToFollow(MAX_REDIRECTS);

        if (offset > 0) {
            stream.withExtraHeaders("Range: bytes=" + juce::String(offset) + "-");
        }

        {
            const juce::ScopedLock lock(mStreamLock);
            mStream = &stream;
        }

        // Declared after the stream, so it runs before the stream is destroyed.
        const juce::ScopeGuard forget_stream {[this] {
            const juce::ScopedLock lock(mStreamLock);
            mStream = nullptr;
        }};

        // A stop requested before mStream was published cancelled nothing.
        if (threadShouldExit()) {
            return {};
        }

        if (!stream.connect(nullptr)) {
            return {nn::tr("Could not reach {host}").replace("{host}", host)};
        }

        const int status_code = stream.getStatusCode();
        const juce::String content_range = stream.getResponseHeaders()["Content-Range"];

        NnLogger::write(NnLogger::Level::Info,
                        "ModelDownloader: " + asset.url.toString(true) + ": from byte "
                            + juce::String(offset) + ", HTTP " + juce::String(status_code)
                            + (content_range.isEmpty() ? juce::String() : ", Content-Range " + content_range));

        if (status_code == 206) {
            // "bytes <first>-<last>/<total>"
            const bool range_matches =
                content_range.startsWith("bytes " + juce::String(offset) + "-")
                && content_range.fromLastOccurrenceOf("/", false, false).getLargeIntValue() == asset.size;

            // Not a continuation of the part file: start again, as for 416.
            if (!range_matches) {
                if (offset > 0 && !rewindPart(inPart, asset.start))
                    return {nn::tr("Could not write to ") + inPart.getFullPathName(), false};
                return {nn::tr("Unexpected response from {host}").replace("{host}", host)};
            }
        } else if (status_code == 200) {
            // The whole file, whether or not a range was asked for.
            if (offset > 0 && !rewindPart(inPart, asset.start)) {
                return {nn::tr("Could not write to ") + inPart.getFullPathName(), false};
            }

            mDownloadedBytes = asset.start;
        } else if (status_code == 416) {
            // The part file is not a prefix the server recognises: start again.
            if (offset > 0 && !rewindPart(inPart, asset.start))
                return {nn::tr("Could not write to ") + inPart.getFullPathName(), false};
            return {nn::tr("The partial download could not be resumed")};
        } else {
            return {nn::tr("{host} answered HTTP ").replace("{host}", host) + juce::String(status_code),
                    status_code >= 500};
        }

        juce::FileOutputStream output(inPart);

        if (output.failedToOpen()) {
            return {nn::tr("Could not write to ") + inPart.getFullPathName(), false};
        }

        juce::HeapBlock<char> buffer(CHUNK_BYTES);

        while (!threadShouldExit()) {
            const int num_read = stream.read(buffer.get(), CHUNK_BYTES);

            if (num_read <= 0) {
                break;
            }

            if (mDownloadedBytes.load() + num_read > asset.start + asset.size) {
                return {nn::tr("{host} sent more than the expected size").replace("{host}", host), false};
            }

            if (!output.write(buffer.get(), static_cast<size_t>(num_read))) {
                return {nn::tr("Could not write to ") + inPart.getFullPathName(), false};
            }

            mDownloadedBytes += num_read;
        }

        output.flush();

        if (output.getStatus().failed()) {
            return {nn::tr("Could not write to ") + inPart.getFullPathName(), false};
        }

        // A connection that drops mid-body ends the stream early rather than reporting an error.
        if (mDownloadedBytes.load() != asset.start + asset.size) {
            return {nn::tr("The connection dropped")};
        }

        return {};
    }

    /** A part named for another digest was started by a build pinned to other weights. */
    static void _deleteOtherPartFiles(const juce::File& inPart)
    {
        const juce::String pattern = inPart.getFileName().upToFirstOccurrenceOf(".gguf", true, false) + ".*.part";

        for (const juce::File& file:
             inPart.getParentDirectory().findChildFiles(juce::File::findFiles, false, pattern)) {
            if (file != inPart) {
                file.deleteFile();
            }
        }
    }

    const ModelSize mModelSize;
    Source mSource = Source::HuggingFace;
    bool mReplaceInstalled = false;
    juce::File mLocalFile;
    juce::File mTargetFile;
    juce::File mPartFile;

    std::atomic<Phase> mPhase {Phase::Idle};
    std::atomic<juce::int64> mDownloadedBytes {0};

    juce::CriticalSection mErrorLock;
    juce::String mErrorMessage;

    // The request in flight, for requestStop to cancel from another thread.
    juce::CriticalSection mStreamLock;
    juce::WebInputStream* mStream = nullptr;
};

float ModelDownloader::Status::getProgress() const
{
    if (totalBytes <= 0) {
        return 0.0f;
    }

    return juce::jlimit(
        0.0f, 1.0f, static_cast<float>(static_cast<double>(downloadedBytes) / static_cast<double>(totalBytes)));
}

ModelDownloader::ModelDownloader()
{
    for (const ModelSize size: ALL_MODEL_SIZES) {
        mJobs[indexOf(size)] = std::make_unique<Job>(size);
    }
}

ModelDownloader::~ModelDownloader()
{
    // All at once, so they wind down together rather than one after the other as each is destroyed.
    for (auto& job: mJobs) {
        job->requestStop();
    }
}

void ModelDownloader::start(ModelSize inModelSize, const juce::File& inDirectory, Source inSource,
                            bool inReplaceInstalled)
{
    mJobs[indexOf(inModelSize)]->begin(inDirectory, inSource, inReplaceInstalled);
}

bool ModelDownloader::useLocalFile(ModelSize inModelSize, const juce::File& inFile)
{
    if (!inFile.existsAsFile()) return false;
    return mJobs[indexOf(inModelSize)]->begin(inFile.getParentDirectory(), Source::HuggingFace, false, inFile);
}

void ModelDownloader::cancel(ModelSize inModelSize)
{
    mJobs[indexOf(inModelSize)]->requestStop();
}

ModelDownloader::Status ModelDownloader::getStatus(ModelSize inModelSize) const
{
    return mJobs[indexOf(inModelSize)]->getStatus();
}
