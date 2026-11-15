//
// Created by Damien Ronssin on 07.08.26.
//

#include "NnLook.h"

#include "NnFonts.h"
#include "NnIcons.h"

void nn::colours::applyTheme(bool light)
{
    const auto pick = [light](juce::uint32 dark, juce::uint32 bright) { return rgb(light ? bright : dark); };
    windowBorder = pick(0x26282e, 0xc8d1db);
    bgRoot = pick(0x131417, 0xf4f6f9);
    bgTopBar = pick(0x17181c, 0xe7edf3);
    bgSidebar = pick(0x161719, 0xedf1f6);
    bgPanel = pick(0x15161a, 0xf0f3f8);
    bgGutter = bgSidebar;
    bgControl = pick(0x1c1e23, 0xdbe4ed);
    bgControlAlt = pick(0x1b1d21, 0xe1e8f0);
    bgControlSubtle = pick(0x1f2126, 0xdce4ed);
    bgControlActive = pick(0x22242a, 0xccd7e3);
    divStrong = pick(0x24262c, 0xc2ceda);
    divSoft = pick(0x202227, 0xcbd5e0);
    divRow = pick(0x1e2024, 0xd3dce6);
    divTick = pick(0x22242a, 0xa9b8c8);
    divOctave = pick(0x232529, 0xc2ceda);
    textBright = pick(0xf2f4f7, 0x182638);
    textPrimary = pick(0xe7e9ec, 0x202c3d);
    textStrong = pick(0xdcdfe4, 0x25354a);
    textFile = pick(0xd7dade, 0x293a4d);
    textButton = pick(0xc2c6cc, 0x26394e);
    textIcon = pick(0x9ba1ab, 0x40556c);
    textIconSoft = pick(0x8e939c, 0x4c6077);
    textLabel = pick(0x7a808a, 0x485b70);
    textDim = pick(0x6b7078, 0x4b5e73);
    textFaint = pick(0x5d626b, 0x536479);
    textFainter = pick(0x585d65, 0x55687d);
    textScale = pick(0x4e535b, 0x52667c);
    textSeparator = pick(0x33363c, 0x75879b);
    accent = pick(0x6e9bff, 0x235bb6);
    accentText = pick(0xa8c2ff, 0x164995);
    warn = pick(0xf2a33c, 0x914606);
    rec = pick(0xff6b8a, 0xb51440);
    recIdle = rec.withMultipliedBrightness(0.48f);
    meterLow = pick(0x4f7a52, 0x42824c);
    meterMid = pick(0x8ec98c, 0x31743f);
    meterHot = warn;
    meterUnlitStrip = bgControlActive;
    meterUnlitMaster = bgControlSubtle;
    wavePlayed = pick(0x7d8797, 0x335f9d);
    waveUnplayed = pick(0x3a3f47, 0x8191a6);
    keyWhite = pick(0xe2e4e8, 0xfcfdff);
    keyBlack = pick(0x0e0f11, 0x172231);
    keyLabel = textLabel;
    laneBlack = pick(0x141519, 0xe5ebf3);
    laneWhite = pick(0x191a1e, 0xf5f8fc);
    faderTrack = pick(0x26282e, 0xc6d2df);
    faderTrackTop = pick(0x2b2e35, 0xb9c9da);
    faderThumb = textPrimary;
    faderThumbMuted = pick(0x5a5f67, 0x658098);
    faderFillMuted = pick(0x3a3d44, 0x9cabbc);
    volumeFill = pick(0x8e939c, 0x526c8a);
    scrollbarThumb = pick(0x2e3138, 0xa6b9cc);
    popupBg = pick(0x1b1d21, 0xf5f8fc);
    popupBorder = pick(0x2e3138, 0xbac9da);
    popupFooterBg = pick(0x191a1e, 0xe7edf4);
    popupRowHover = pick(0x22242a, 0xdfe9f6);
    popupTitle = textDim;
    popupItem = pick(0xa8adb5, 0x33475e);
    popupItemTicked = textPrimary;
    checkboxBorder = pick(0x3a3d44, 0x73879d);
    checkboxTick = pick(0x12131a, 0xffffff);
    ctaBorder = accent;
    ctaText = accentText;
    dropZoneBorder = pick(0x2b2e35, 0x8ea4bd);
    progressTrack = faderTrack;
    progressFill = accent;
    progressText = accentText;
    zoomIcon = textFainter;
    zoomTrack = faderTrack;
    zoomFill = pick(0x6b7078, 0x547090);
    zoomThumb = pick(0xc2c6cc, 0x344d68);
}

namespace nn
{
namespace
{
    /** Tracking in pixels. The em size is what CSS resolves an "em" against, not the line height. */
    float trackingPixels(const juce::Font& inFont, float inTrackingEm)
    {
        return inTrackingEm * inFont.getHeightInPoints();
    }
} // namespace

float trackedTextWidth(const juce::String& inText, const juce::Font& inFont, float inTrackingEm)
{
    const float natural = juce::GlyphArrangement::getStringWidth(inFont, inText);
    const int num_gaps = juce::jmax(0, inText.length() - 1);

    return natural + static_cast<float>(num_gaps) * trackingPixels(inFont, inTrackingEm);
}

void drawTrackedText(juce::Graphics& g,
                     const juce::String& inText,
                     const juce::Font& inFont,
                     juce::Rectangle<float> inArea,
                     juce::Justification inJustification,
                     float inTrackingEm)
{
    if (inText.isEmpty())
        return;

    juce::GlyphArrangement arrangement;
    arrangement.addLineOfText(inFont, inText, 0.0f, 0.0f);

    const int num_glyphs = arrangement.getNumGlyphs();

    if (num_glyphs == 0)
        return;

    // Spread the glyphs first, then place the result: the justification has to see the tracked
    // width, or a right-aligned label drifts by the whole accumulated spacing.
    const float tracking = trackingPixels(inFont, inTrackingEm);

    for (int i = 1; i < num_glyphs; ++i) {
        arrangement.getGlyph(i).moveBy(static_cast<float>(i) * tracking, 0.0f);
    }

    const juce::Rectangle<float> bounds = arrangement.getBoundingBox(0, num_glyphs, true);

    float x = inArea.getX();

    if (inJustification.testFlags(juce::Justification::horizontallyCentred))
        x = inArea.getCentreX() - bounds.getWidth() / 2.0f;
    else if (inJustification.testFlags(juce::Justification::right))
        x = inArea.getRight() - bounds.getWidth();

    // Baselines, not bounding boxes: two labels of different weights on the same row have
    // different bounding-box tops and would not line up if centred by box.
    float baseline = inArea.getY() + inFont.getAscent();

    if (inJustification.testFlags(juce::Justification::verticallyCentred))
        baseline = inArea.getCentreY() - (inFont.getAscent() + inFont.getDescent()) / 2.0f + inFont.getAscent();
    else if (inJustification.testFlags(juce::Justification::bottom))
        baseline = inArea.getBottom() - inFont.getDescent();

    // Against the bounding box's own left edge, not the first glyph's origin: a glyph with a left
    // side bearing would otherwise sit a fraction of a pixel off from a plain drawText beside it.
    arrangement.draw(g, juce::AffineTransform::translation(x - bounds.getX(), baseline));
}

juce::String midiNoteName(int inMidiNote)
{
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    const int note = ((inMidiNote % 12) + 12) % 12;
    const int octave = inMidiNote / 12 - 1;

    return juce::String(names[note]) + juce::String(octave);
}

void drawPopupSurface(juce::Graphics& g, juce::Rectangle<float> inBounds)
{
    g.setColour(nn::colours::popupBg);
    g.fillRoundedRectangle(inBounds, nn::metrics::menuCorner);

    g.setColour(nn::colours::popupBorder);
    g.drawRoundedRectangle(inBounds.reduced(0.5f), nn::metrics::menuCorner, 1.0f);
}

void drawCheckbox(juce::Graphics& g, juce::Rectangle<int> inArea, bool inIsTicked, float inAlpha)
{
    const auto box =
        inArea.withSizeKeepingCentre(nn::metrics::checkboxSize, nn::metrics::checkboxSize).toFloat().reduced(0.5f);

    if (inIsTicked) {
        g.setColour(nn::colours::accent.withMultipliedAlpha(inAlpha));
        g.fillRoundedRectangle(box, nn::metrics::checkboxCorner);

        g.setColour(nn::colours::checkboxTick.withMultipliedAlpha(inAlpha));
        g.strokePath(nn::icons::checkStroked(box),
                     juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    } else {
        g.setColour(nn::colours::checkboxBorder.withMultipliedAlpha(inAlpha));
        g.drawRoundedRectangle(box, nn::metrics::checkboxCorner, 1.0f);
    }
}
} // namespace nn

namespace
{
/** Room for the tick box and the padding either side of it, at the right-hand end of a row. */
constexpr int MENU_TICK_COLUMN = nn::metrics::checkboxSize + nn::metrics::menuPadX;

constexpr int MENU_SEPARATOR_HEIGHT = 9;
constexpr int MENU_MIN_WIDTH = 180;

/** Where a tooltip wraps. Two of ours are already two lines, and one long line reads worse. */
constexpr int TOOLTIP_MAX_WIDTH = 260;
constexpr int TOOLTIP_PAD_Y = 6;

/** Laid out once for the bounds and again for the drawing, so the two cannot disagree. */
juce::TextLayout tooltipLayout(const juce::String& inText)
{
    juce::AttributedString attributed;
    attributed.setJustification(juce::Justification::centredLeft);
    attributed.append(inText, nn::fonts::menuItem(), nn::colours::popupItem);

    juce::TextLayout layout;
    layout.createLayout(attributed, static_cast<float>(TOOLTIP_MAX_WIDTH));

    return layout;
}
} // namespace

nn::NeuralNoteLookAndFeel::NeuralNoteLookAndFeel()
{
    refreshTheme();
}

void nn::NeuralNoteLookAndFeel::refreshTheme()
{
    // Seeds every colour id the settings dialog's widgets read -- combo boxes, labels, buttons,
    // tick boxes, the MIDI input list and its scrollbar -- from nine values, rather than naming
    // each id here and finding out which ones were missed by looking at the dialog.
    setColourScheme({nn::colours::bgRoot,
                     nn::colours::bgControl,
                     nn::colours::popupBg,
                     nn::colours::divStrong,
                     nn::colours::textPrimary,
                     nn::colours::accent,
                     nn::colours::textBright,
                     nn::colours::accent,
                     nn::colours::popupItem});

    // After the scheme, not before: setColourScheme re-initialises every colour, these included.
    setColour(juce::PopupMenu::backgroundColourId, nn::colours::popupBg);
    setColour(juce::PopupMenu::textColourId, nn::colours::popupItem);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, nn::colours::popupRowHover);
    setColour(juce::PopupMenu::highlightedTextColourId, nn::colours::popupItemTicked);
}

juce::Font nn::NeuralNoteLookAndFeel::getPopupMenuFont()
{
    return nn::fonts::menuItem();
}

int nn::NeuralNoteLookAndFeel::getPopupMenuBorderSizeWithOptions(const juce::PopupMenu::Options& inOptions)
{
    juce::ignoreUnused(inOptions);

    return nn::metrics::menuListPadY;
}

void nn::NeuralNoteLookAndFeel::drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                                                  int inWidth,
                                                                  int inHeight,
                                                                  const juce::PopupMenu::Options& inOptions)
{
    juce::ignoreUnused(inOptions);

    nn::drawPopupSurface(
        g, juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(inWidth), static_cast<float>(inHeight)));
}

void nn::NeuralNoteLookAndFeel::getIdealPopupMenuItemSizeWithOptions(const juce::String& inText,
                                                                    bool inIsSeparator,
                                                                    int inStandardItemHeight,
                                                                    int& outIdealWidth,
                                                                    int& outIdealHeight,
                                                                    const juce::PopupMenu::Options& inOptions)
{
    juce::ignoreUnused(inStandardItemHeight, inOptions);

    if (inIsSeparator) {
        outIdealHeight = MENU_SEPARATOR_HEIGHT;
        outIdealWidth = MENU_MIN_WIDTH;
        return;
    }

    outIdealHeight = nn::metrics::menuRowHeight;
    outIdealWidth = juce::jmax(MENU_MIN_WIDTH,
                               juce::GlyphArrangement::getStringWidthInt(getPopupMenuFont(), inText)
                                   + nn::metrics::menuPadX + MENU_TICK_COLUMN + nn::metrics::menuPadX);
}

void nn::NeuralNoteLookAndFeel::drawPopupMenuItemWithOptions(juce::Graphics& g,
                                                            const juce::Rectangle<int>& inArea,
                                                            bool inIsHighlighted,
                                                            const juce::PopupMenu::Item& inItem,
                                                            const juce::PopupMenu::Options& inOptions)
{
    juce::ignoreUnused(inOptions);

    if (inItem.isSeparator) {
        g.setColour(nn::colours::divStrong);
        g.fillRect(inArea.withSizeKeepingCentre(inArea.getWidth(), 1));
        return;
    }

    if (inIsHighlighted && inItem.isEnabled) {
        g.setColour(nn::colours::popupRowHover);
        g.fillRect(inArea);
    }

    auto row = inArea.reduced(nn::metrics::menuPadX, 0);

    // The tick sits at the right-hand end, where the instrument dropdown puts it, rather than in
    // JUCE's left gutter -- which would leave every unticked row's label indented past nothing.
    const auto tick_area = row.removeFromRight(nn::metrics::checkboxSize);

    const float alpha = inItem.isEnabled ? 1.0f : nn::DISABLED_ALPHA;

    // Nothing is drawn for an unticked row: a menu item here is as often an action as a toggle, and
    // an empty box beside "Check for updates" would promise a state it does not have.
    if (inItem.isTicked) {
        nn::drawCheckbox(g, tick_area, true, alpha);
    }

    g.setColour((inItem.isTicked ? nn::colours::popupItemTicked : nn::colours::popupItem).withMultipliedAlpha(alpha));
    g.setFont(inItem.isTicked ? nn::fonts::menuItemTicked() : getPopupMenuFont());
    g.drawText(inItem.text, row.withTrimmedRight(nn::metrics::menuPadX), juce::Justification::centredLeft, true);
}

juce::Rectangle<int> nn::NeuralNoteLookAndFeel::getTooltipBounds(const juce::String& inTipText,
                                                                juce::Point<int> inScreenPos,
                                                                juce::Rectangle<int> inParentArea)
{
    const juce::TextLayout layout = tooltipLayout(inTipText);

    // Rounded up: half a pixel short of the laid-out width is a wrap that was not asked for.
    const int width = juce::roundToInt(std::ceil(layout.getWidth())) + 2 * nn::metrics::menuPadX;
    const int height = juce::roundToInt(std::ceil(layout.getHeight())) + 2 * TOOLTIP_PAD_Y;

    // JUCE's own placement, kept: the tooltip goes away from whichever edge the pointer is nearest,
    // so it never lands on top of the thing being hovered.
    return juce::Rectangle<int>(
               inScreenPos.x > inParentArea.getCentreX() ? inScreenPos.x - (width + 12) : inScreenPos.x + 24,
               inScreenPos.y > inParentArea.getCentreY() ? inScreenPos.y - (height + 6) : inScreenPos.y + 6,
               width,
               height)
        .constrainedWithin(inParentArea);
}

void nn::NeuralNoteLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& inText, int inWidth, int inHeight)
{
    // Not fillAll: the window is a transparent child of the editor, so it can round its corners the
    // way the menus do instead of being the one square panel in the window.
    const auto bounds = juce::Rectangle<int>(inWidth, inHeight).toFloat();

    nn::drawPopupSurface(g, bounds);

    tooltipLayout(inText).draw(g, bounds.reduced(nn::metrics::menuPadX, TOOLTIP_PAD_Y));
}

void nn::NeuralNoteLookAndFeel::drawCornerResizer(juce::Graphics& g,
                                                  int inWidth,
                                                  int inHeight,
                                                  bool inIsMouseOver,
                                                  bool inIsMouseDragging)
{
    const auto colour = (inIsMouseOver || inIsMouseDragging) ? nn::colours::textDim : nn::colours::textScale;

    g.setColour(colour);

    const auto size = (float) juce::jmin(inWidth, inHeight);

    for (float offset: {0.30f, 0.55f, 0.80f}) {
        const float from = size * offset;
        g.drawLine(size, from, from, size, 1.0f);
    }
}
