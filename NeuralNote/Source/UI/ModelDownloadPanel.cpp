//
// Created by Damien Ronssin on 12.09.26.
//

#include "ModelDownloadPanel.h"

#include "ModelManifest.h"
#include "NeuralNoteTooltips.h"
#include "NNFileUtils.h"
#include "NnFonts.h"
#include "NnGlobalSettings.h"
#include "NnIcons.h"
#include "NnLook.h"
#include "PluginProcessor.h"
#include "NnLocalisation.h"

namespace
{
constexpr int PANEL_WIDTH = 488;
constexpr float PANEL_CORNER = 8.0f;

// Header, rows' contents and footer all start here, so they share one left edge.
constexpr int CONTENT_X = 20;

constexpr int PAD_TOP = 16;
constexpr int PAD_BOTTOM = 14;
constexpr int TITLE_HEIGHT = 18;
constexpr int SUBTITLE_HEIGHT = 16;
constexpr int HEADER_GAP = 10;

// A row's background reaches closer to the panel's edge than its contents do.
constexpr int ROW_INSET_X = 10;
constexpr int ROW_HEIGHT = 46;
constexpr int ROW_GAP = 2;
constexpr float ROW_CORNER = 6.0f;
constexpr int ROW_NAME_TOP = 8;
constexpr int ROW_META_TOP = 26;
constexpr int ROW_LINE_HEIGHT = 14;
constexpr int CHECKBOX_GAP = 12;

// Wide enough for a progress bar, its percentage and the cross, and every other control is
// right-aligned in the same column.
constexpr int CONTROL_COLUMN_WIDTH = 172;
constexpr int CONTROL_GAP = 10;
constexpr int PERCENT_WIDTH = 32;

constexpr int FOOTER_GAP = 10;
constexpr int BUTTON_HEIGHT = 26;
constexpr const char* QUARK_DOWNLOAD_URL = "https://pan.quark.cn/s/75f64075d604";
constexpr float ICON_SIZE = 13.0f;
constexpr float BAR_CORNER = 2.0f;
constexpr int POLL_HZ = 10;

constexpr const char* VERIFYING_CAPTION = "VERIFYING";
constexpr float CAPTION_TRACKING = 0.06f;

/** Relative to the other sizes: they are the same model at three scales. */
juce::String hintFor(ModelSize inModelSize)
{
    switch (inModelSize) {
        case ModelSize::Small:
            return nn::tr("Fastest");
        case ModelSize::Medium:
            return nn::tr("Recommended");
        case ModelSize::Large:
            return nn::tr("Largest, slowest");
    }

    return {};
}

int percentOf(const ModelDownloader::Status& inStatus)
{
    return juce::roundToInt(100.0f * inStatus.getProgress());
}

/** e.g. "618 MB", "2.7 GB". */
juce::String formatSize(juce::int64 inNumBytes)
{
    if (inNumBytes >= 1'000'000'000) {
        return juce::String(static_cast<double>(inNumBytes) / 1.0e9, 1) + " GB";
    }

    return juce::String(juce::roundToInt(static_cast<double>(inNumBytes) / 1.0e6)) + " MB";
}

/** A row's contents: its background minus the inset that lines them up with the header. */
juce::Rectangle<int> contentOf(juce::Rectangle<int> inRowBounds)
{
    return inRowBounds.reduced(CONTENT_X - ROW_INSET_X, 0);
}

juce::File existingFolder(juce::File folder)
{
    while (!folder.isDirectory()) {
        const auto parent = folder.getParentDirectory();
        if (parent == folder) return {};
        folder = parent;
    }
    return folder;
}
} // namespace

ModelDownloadPanel::Row::Row(ModelSize inModelSize)
    : modelSize(inModelSize)
    , downloadButton("DownloadModel")
    , cancelButton("StopModelDownload")
{
}

ModelDownloadPanel::ModelDownloadPanel(NeuralNoteAudioProcessor& inProcessor)
    : mProcessor(inProcessor)
{
    for (const ModelSize size: ALL_MODEL_SIZES) {
        auto row = std::make_unique<Row>(size);

        // The Transcribe button's colours: it is the same call to action, one step earlier.
        row->downloadButton.setIcon(nn::icons::downloadStroked, NnFlatButton::IconStyle::stroked, ICON_SIZE);
        row->downloadButton.setPadding(10, 12, 7);
        row->downloadButton.setCornerRadius(static_cast<float>(nn::metrics::controlCorner));
        row->downloadButton.setColour(NnFlatButton::backgroundColourId, nn::colours::ctaFill());
        row->downloadButton.setColour(NnFlatButton::outlineColourId, nn::colours::ctaBorder);
        row->downloadButton.setColour(NnFlatButton::iconColourId, nn::colours::ctaText);
        row->downloadButton.setColour(NnFlatButton::textColourId, nn::colours::ctaText);
        row->downloadButton.setWantsKeyboardFocus(false);
        row->downloadButton.onClick = [this, size] {
            _chooseDownloadDirectory(size);
        };
        addChildComponent(row->downloadButton);

        row->cancelButton.setIcon(
            nn::icons::crossStroked, NnFlatButton::IconStyle::stroked, nn::metrics::cancelGlyphSize);
        row->cancelButton.setCornerRadius(4.0f);
        row->cancelButton.setTooltip(nn::tr(NeuralNoteTooltips::stop_model_download));
        row->cancelButton.setWantsKeyboardFocus(false);
        row->cancelButton.onClick = [this, size] { mProcessor.getModelDownloader().cancel(size); };
        addChildComponent(row->cancelButton);

        mRows[static_cast<size_t>(size)] = std::move(row);
    }

    mCloseButton.setIcon(nn::icons::crossStroked, NnFlatButton::IconStyle::stroked, nn::metrics::cancelGlyphSize);
    mCloseButton.setCornerRadius(4.0f);
    mCloseButton.setTooltip(nn::tr("Close"));
    mCloseButton.setWantsKeyboardFocus(false);
    mCloseButton.onClick = [this] {
        if (onCloseRequested != nullptr) {
            onCloseRequested();
        }
    };
    addAndMakeVisible(mCloseButton);

    mOpenFolderButton.setIcon(nn::icons::folderStroked, NnFlatButton::IconStyle::stroked, ICON_SIZE);
    auto addFooterButton = [this](NnFlatButton& button, const juce::String& label) {
        button.setLabel(label, nn::fonts::buttonLabel());
        button.setPadding(10, 12, 7);
        button.setCornerRadius(static_cast<float>(nn::metrics::controlCorner));
        button.setColour(NnFlatButton::backgroundColourId, nn::colours::popupRowHover);
        button.setColour(NnFlatButton::outlineColourId, nn::colours::popupBorder);
        button.setColour(NnFlatButton::iconColourId, nn::colours::textIcon);
        button.setColour(NnFlatButton::textColourId, nn::colours::textButton);
        addAndMakeVisible(button);
    };
    addFooterButton(mOpenFolderButton, nn::tr("Open models folder"));
    mOpenFolderButton.onClick = [] {
        const auto size = NnGlobalSettings::getModelSize();
        auto folder = existingFolder(NNFileUtils::getModelDirectory(size));
        if (folder != juce::File()) NNFileUtils::openDirectory(folder);
    };
    addFooterButton(mLocalModelButton, nn::tr("Load local model..."));
    mLocalModelButton.onClick = [this] { _chooseLocalModel(); };
    addFooterButton(mManualDownloadButton, nn::tr("Quark download"));
    mManualDownloadButton.onClick = [] { juce::URL(QUARK_DOWNLOAD_URL).launchInDefaultBrowser(); };

    mHasInstalledModel = NNFileUtils::isAnyModelInstalled();
    _updateRows(true);

    // Also while hidden: noticing the last checkpoint disappear is what brings the panel back.
    startTimerHz(POLL_HZ);
}

void ModelDownloadPanel::_chooseDownloadDirectory(ModelSize inModelSize)
{
    if (mChoosingDirectory || mProcessor.getModelDownloader().getStatus(inModelSize).isBusy()) {
        return;
    }
    mChoosingDirectory = true;
    _showDownloadLocation(inModelSize, NNFileUtils::suggestModelDownloadDirectory(inModelSize).getFullPathName());
}

void ModelDownloadPanel::_chooseLocalModel()
{
    if (mLocalModelChooser || mImportingSize) return;
    mLocalModelChooser = std::make_unique<juce::FileChooser>(
        nn::tr("Load local model..."), juce::File(), "*.gguf", true, false, this);
    const juce::Component::SafePointer<ModelDownloadPanel> safeThis(this);
    mLocalModelChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            juce::MessageManager::callAsync([safeThis, file] {
                if (!safeThis) return;
                safeThis->mLocalModelChooser.reset();
                if (file == juce::File()) return;
                for (const auto size : ALL_MODEL_SIZES) {
                    const auto& entry = getModelManifestEntry(size);
                    if (file.getFileName() == entry.fileName) {
                        if (safeThis->mProcessor.getModelDownloader().useLocalFile(size, file)) {
                            safeThis->mImportingSize = size;
                            safeThis->timerCallback();
                            return;
                        }
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon, nn::tr("Load local model..."),
                            nn::tr("This model is busy or the file is no longer available."));
                        return;
                    }
                }
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon, nn::tr("Load local model..."),
                    nn::tr("Choose a complete MuScriptor GGUF file with its original name."));
            });
        });
}

void ModelDownloadPanel::_showDownloadLocation(ModelSize inModelSize, const juce::String& inPath,
                                               const juce::String& inError)
{
    auto message = nn::tr("Enter a full folder path or browse, then choose a download source.");
    if (NNFileUtils::isModelInstalled(inModelSize))
        message += "\n\n" + nn::tr("The existing model is kept until the new download is verified.");
    if (inError.isNotEmpty()) message = inError + "\n\n" + message;
    mDownloadLocation = std::make_unique<juce::AlertWindow>(
        nn::tr("Choose model download folder"), message, juce::MessageBoxIconType::NoIcon, this);
    mDownloadLocation->addTextEditor("folder", inPath, nn::tr("Model folder"));
    mDownloadLocation->addButton(nn::tr("Browse..."), 2);
    mDownloadLocation->addButton(nn::tr("Download from Hugging Face"), 1);
    mDownloadLocation->addButton(nn::tr("Download from GitHub"), 3);
    mDownloadLocation->addButton(nn::tr("Cancel"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<ModelDownloadPanel> safeThis(this);
    mDownloadLocation->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, inModelSize](int result) {
        if (!safeThis || !safeThis->mDownloadLocation) return;
        const auto path = safeThis->mDownloadLocation->getTextEditorContents("folder").trim().unquoted();
        // Keep the dialog alive until its modal callback has returned.
        juce::MessageManager::callAsync([safeThis, inModelSize, result, path] {
            if (!safeThis) return;
            safeThis->mDownloadLocation.reset();
            if (result == 2) safeThis->_browseDownloadLocation(inModelSize, path);
            else if (result == 1 || result == 3)
                safeThis->_confirmDownloadLocation(inModelSize, path, result == 3
                    ? ModelDownloader::Source::GitHub : ModelDownloader::Source::HuggingFace);
            else safeThis->mChoosingDirectory = false;
        });
    }));
}

void ModelDownloadPanel::_browseDownloadLocation(ModelSize inModelSize, const juce::String& inPath)
{
    auto initial = juce::File::isAbsolutePath(inPath) ? existingFolder(juce::File(inPath)) : juce::File();
    if (initial == juce::File()) initial = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    mDirectoryChooser = std::make_unique<juce::FileChooser>(
        nn::tr("Choose model download folder"), initial, "", true, false, this);
    const juce::Component::SafePointer<ModelDownloadPanel> safeThis(this);
    mDirectoryChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safeThis, inModelSize, inPath](const juce::FileChooser& chooser) {
            const auto directory = chooser.getResult();
            juce::MessageManager::callAsync([safeThis, inModelSize, inPath, directory] {
                if (!safeThis) return;
                safeThis->mDirectoryChooser.reset();
                safeThis->_showDownloadLocation(inModelSize, directory == juce::File() ? inPath : directory.getFullPathName());
            });
        });
}

void ModelDownloadPanel::_confirmDownloadLocation(ModelSize inModelSize, const juce::String& inPath,
                                                  ModelDownloader::Source inSource)
{
    auto& downloader = mProcessor.getModelDownloader();
    if (downloader.getStatus(inModelSize).isBusy()) {
        mChoosingDirectory = false;
        return;
    }
    if (!juce::File::isAbsolutePath(inPath)) {
        _showDownloadLocation(inModelSize, inPath, nn::tr("Enter a full folder path."));
        return;
    }
    const juce::File directory(inPath);
    const auto volumeFolder = existingFolder(directory);
    const bool replacing = NNFileUtils::isModelInstalled(inModelSize);
    const auto required = NNFileUtils::getModelDownloadBytesRequired(inModelSize, directory, replacing);
    const auto available = volumeFolder == juce::File() ? -1 : volumeFolder.getBytesFreeOnVolume();
    if (volumeFolder != juce::File() && available >= 0 && available < required) {
        const auto error = nn::tr("Not enough space in this folder. Required: {required}. Available: {available}.")
            .replace("{required}", formatSize(required)).replace("{available}", formatSize(available));
        _showDownloadLocation(inModelSize, inPath, error);
        return;
    }
    if (volumeFolder == juce::File()
        || (replacing ? (!NNFileUtils::ensureDirectoryExists(directory) || !directory.hasWriteAccess())
                      : !NNFileUtils::setModelDirectory(inModelSize, directory))) {
        _showDownloadLocation(inModelSize, inPath,
                              nn::tr("Could not save the model folder. Please choose a writable folder."));
        return;
    }
    mChoosingDirectory = false;
    downloader.start(inModelSize, directory, inSource, replacing);
    timerCallback();
}

int ModelDownloadPanel::getIdealWidth()
{
    return PANEL_WIDTH;
}

int ModelDownloadPanel::getIdealHeight()
{
    const int num_rows = static_cast<int>(ALL_MODEL_SIZES.size());

    return PAD_TOP + TITLE_HEIGHT + SUBTITLE_HEIGHT + HEADER_GAP + num_rows * ROW_HEIGHT + (num_rows - 1) * ROW_GAP
           + FOOTER_GAP + BUTTON_HEIGHT + PAD_BOTTOM;
}

void ModelDownloadPanel::setCloseButtonVisible(bool inVisible)
{
    mCloseButton.setVisible(inVisible);
}

void ModelDownloadPanel::timerCallback()
{
    if (mImportingSize) {
        const auto status = mProcessor.getModelDownloader().getStatus(*mImportingSize);
        if (!status.isBusy()) {
            if (status.phase == ModelDownloader::Phase::Idle && NNFileUtils::isModelInstalled(*mImportingSize))
                NnGlobalSettings::setModelSize(*mImportingSize);
            mImportingSize.reset();
        }
    }
    const bool has_installed_model = NNFileUtils::isAnyModelInstalled();
    const bool installed_changed = has_installed_model != mHasInstalledModel;
    mHasInstalledModel = has_installed_model;

    // The title follows whether anything is installed, so that alone is worth a repaint.
    if (_updateRows(false) || installed_changed) {
        repaint();
    }

    if (installed_changed && onInstalledModelsChanged != nullptr) {
        onInstalledModelsChanged();
    }
}

bool ModelDownloadPanel::_updateRows(bool inForce)
{
    bool changed = inForce;
    const std::optional<ModelSize> in_use = NNFileUtils::getInstalledModelSize(NnGlobalSettings::getModelSize());

    for (auto& row: mRows) {
        ModelDownloader::Status status = mProcessor.getModelDownloader().getStatus(row->modelSize);
        const bool is_installed = NNFileUtils::isModelInstalled(row->modelSize);
        const bool is_in_use = in_use == row->modelSize;
        const ModelDownloader::Status& shown = row->status;

        changed = changed || is_installed != row->isInstalled || is_in_use != row->isInUse
                  || status.phase != shown.phase || percentOf(status) != percentOf(shown)
                  || (status.downloadedBytes > 0) != (shown.downloadedBytes > 0)
                  || status.errorMessage != shown.errorMessage;

        row->status = std::move(status);
        row->isInstalled = is_installed;
        row->isInUse = is_in_use;
    }

    if (!changed) {
        return false;
    }

    for (auto& row: mRows) {
        const ModelDownloader::Status& status = row->status;

        row->downloadButton.setLabel(status.phase == ModelDownloader::Phase::Failed ? nn::tr("Retry in...")
                                     : status.downloadedBytes > 0                   ? nn::tr("Resume in...")
                                     : row->isInstalled                             ? nn::tr("Download again...")
                                                                                    : nn::tr("Download to..."),
                                     nn::fonts::buttonLabel());
        row->downloadButton.setVisible(!status.isBusy());
        row->cancelButton.setVisible(status.phase == ModelDownloader::Phase::Downloading);
    }

    // A relabelled button is a resized one.
    resized();
    return true;
}

void ModelDownloadPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(PAD_TOP);

    mCloseButton.setBounds(area.withHeight(TITLE_HEIGHT)
                               .withTrimmedRight(CONTENT_X)
                               .removeFromRight(nn::metrics::cancelHitSize)
                               .withSizeKeepingCentre(nn::metrics::cancelHitSize, nn::metrics::cancelHitSize));

    area.removeFromTop(TITLE_HEIGHT + SUBTITLE_HEIGHT + HEADER_GAP);

    for (auto& row: mRows) {
        row->bounds = area.removeFromTop(ROW_HEIGHT).reduced(ROW_INSET_X, 0);
        area.removeFromTop(ROW_GAP);

        auto column = contentOf(row->bounds).removeFromRight(CONTROL_COLUMN_WIDTH);

        const int button_width = row->downloadButton.getIdealWidth();
        row->downloadButton.setBounds(
            column.withLeft(column.getRight() - button_width).withSizeKeepingCentre(button_width, BUTTON_HEIGHT));

        row->cancelButton.setBounds(column.withLeft(column.getRight() - nn::metrics::cancelHitSize)
                                        .withSizeKeepingCentre(nn::metrics::cancelHitSize, nn::metrics::cancelHitSize));
    }

    area.removeFromTop(FOOTER_GAP - ROW_GAP);

    auto footer = area.removeFromTop(BUTTON_HEIGHT).reduced(CONTENT_X, 0);
    for (auto* button : {&mOpenFolderButton, &mLocalModelButton, &mManualDownloadButton}) {
        button->setBounds(footer.removeFromLeft(button->getIdealWidth()));
        footer.removeFromLeft(CONTROL_GAP);
    }
}

void ModelDownloadPanel::paint(juce::Graphics& g)
{
    const auto surface = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(nn::colours::popupBg);
    g.fillRoundedRectangle(surface, PANEL_CORNER);
    g.setColour(nn::colours::popupBorder);
    g.drawRoundedRectangle(surface, PANEL_CORNER, 1.0f);

    auto header = getLocalBounds().reduced(CONTENT_X, 0);
    header.removeFromTop(PAD_TOP);

    g.setColour(nn::colours::textPrimary);
    g.setFont(nn::fonts::filename());
    g.drawText(mHasInstalledModel ? nn::tr("Transcription model") : nn::tr("No transcription model installed"),
               header.removeFromTop(TITLE_HEIGHT).withTrimmedRight(nn::metrics::cancelHitSize + CONTROL_GAP),
               juce::Justification::centredLeft,
               true);

    g.setColour(nn::colours::textLabel);
    g.setFont(nn::fonts::menuItem());
    g.drawText(mHasInstalledModel ? nn::tr("Tick the model to transcribe with.") : nn::tr("Download a model to start transcribing."),
               header.removeFromTop(SUBTITLE_HEIGHT),
               juce::Justification::centredLeft,
               true);

    for (int i = 0; i < static_cast<int>(mRows.size()); i++) {
        const Row& row = *mRows[static_cast<size_t>(i)];
        const ModelDownloader::Status& status = row.status;

        if (row.isInUse) {
            g.setColour(nn::colours::accentFillActive());
            g.fillRoundedRectangle(row.bounds.toFloat(), ROW_CORNER);
        } else if (row.isInstalled && i == mHoveredRow) {
            g.setColour(nn::colours::popupRowHover);
            g.fillRoundedRectangle(row.bounds.toFloat(), ROW_CORNER);
        }

        auto content = contentOf(row.bounds);

        nn::drawCheckbox(g,
                         content.removeFromLeft(nn::metrics::checkboxSize),
                         row.isInUse,
                         row.isInstalled ? 1.0f : nn::DISABLED_ALPHA);
        content.removeFromLeft(CHECKBOX_GAP);

        // What the controls on the right leave for the name and the line under it.
        auto column = content;
        column.removeFromLeft(content.getWidth() - CONTROL_COLUMN_WIDTH);
        auto text = content;

        if (status.phase == ModelDownloader::Phase::Downloading) {
            text.setRight(column.getX() - CONTROL_GAP);

            column.removeFromRight(nn::metrics::cancelHitSize + CONTROL_GAP);

            g.setColour(nn::colours::progressText);
            g.setFont(nn::fonts::statusBar());
            g.drawText(juce::String(percentOf(status)) + "%",
                       column.removeFromRight(PERCENT_WIDTH),
                       juce::Justification::centredRight);

            column.removeFromRight(CONTROL_GAP);

            const auto bar = column.withSizeKeepingCentre(column.getWidth(), nn::metrics::progressBarHeight).toFloat();

            g.setColour(nn::colours::progressTrack);
            g.fillRoundedRectangle(bar, BAR_CORNER);

            const float filled = bar.getWidth() * status.getProgress();

            if (filled > 0.0f) {
                g.setColour(nn::colours::progressFill);
                g.fillRoundedRectangle(bar.withWidth(std::max(filled, 2.0f * BAR_CORNER)), BAR_CORNER);
            }
        } else if (status.phase == ModelDownloader::Phase::Verifying) {
            text.setRight(column.getX() - CONTROL_GAP);

            g.setColour(nn::colours::progressText);
            nn::drawTrackedText(g,
                                nn::tr(VERIFYING_CAPTION),
                                nn::fonts::statusBar(),
                                column.toFloat(),
                                juce::Justification::centredRight,
                                CAPTION_TRACKING);
        } else {
            // Up to the button rather than the column, so an error message gets what room there is.
            text.setRight(row.downloadButton.getX() - CONTROL_GAP);
        }

        g.setColour(row.isInUse       ? nn::colours::popupItemTicked
                    : row.isInstalled ? nn::colours::popupItem
                                      : nn::colours::textLabel);
        g.setFont(row.isInUse ? nn::fonts::menuItemTicked() : nn::fonts::instrumentName());
        g.drawText(nn::tr(modelSizeToDisplayName(row.modelSize)),
                   text.withTrimmedTop(ROW_NAME_TOP).withHeight(ROW_LINE_HEIGHT),
                   juce::Justification::centredLeft,
                   true);

        const bool failed = status.phase == ModelDownloader::Phase::Failed;
        const bool busy = status.phase == ModelDownloader::Phase::Downloading;
        const juce::String separator = "  " + nn::separatorDot() + "  ";

        const juce::String meta = failed ? status.errorMessage
                                  : busy ? formatSize(status.downloadedBytes) + nn::tr(" of ") + formatSize(status.totalBytes)
                                         : formatSize(status.totalBytes) + separator + hintFor(row.modelSize);

        g.setColour(failed ? nn::colours::warn : nn::colours::textFaint);
        g.setFont(nn::fonts::meta());
        g.drawText(meta,
                   text.withTrimmedTop(ROW_META_TOP).withHeight(ROW_LINE_HEIGHT),
                   juce::Justification::centredLeft,
                   true);
    }
}

int ModelDownloadPanel::_rowAt(juce::Point<int> inPosition) const
{
    for (int i = 0; i < static_cast<int>(mRows.size()); i++) {
        if (mRows[static_cast<size_t>(i)]->bounds.contains(inPosition)) {
            return i;
        }
    }

    return -1;
}

void ModelDownloadPanel::mouseMove(const juce::MouseEvent& inEvent)
{
    const int hovered = _rowAt(inEvent.getPosition());
    const bool can_pick = hovered >= 0 && mRows[static_cast<size_t>(hovered)]->isInstalled;

    setMouseCursor(can_pick ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);

    if (hovered != mHoveredRow) {
        mHoveredRow = hovered;
        repaint();
    }
}

void ModelDownloadPanel::mouseExit(const juce::MouseEvent&)
{
    if (mHoveredRow != -1) {
        mHoveredRow = -1;
        repaint();
    }
}

void ModelDownloadPanel::mouseUp(const juce::MouseEvent& inEvent)
{
    const int clicked = _rowAt(inEvent.getPosition());

    if (!inEvent.mouseWasClicked() || clicked < 0) {
        return;
    }

    const Row& row = *mRows[static_cast<size_t>(clicked)];

    if (row.isInstalled && !row.isInUse) {
        NnGlobalSettings::setModelSize(row.modelSize);
        timerCallback();
    }
}
