#include "PluginEditor.h"

#include "ParameterIds.h"
#include "gui/KrainLookAndFeel.h"

namespace
{
constexpr int headerHeight = 46;
constexpr int cloudHeight = 196;
constexpr int margin = 14;
} // namespace

//==============================================================================
KrainAudioProcessorEditor::KrainAudioProcessorEditor (KrainAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      cloud (p.getGrainEventQueue()),
      delayTime (p.getValueTreeState(), krain::params::delayTime, "Time"),
      syncEnabled (p.getValueTreeState(), krain::params::syncEnabled, "Sync"),
      syncDivision (p.getValueTreeState(), krain::params::syncDivision, "Division"),
      feedback (p.getValueTreeState(), krain::params::feedback, "Feedback"),
      grainSize (p.getValueTreeState(), krain::params::grainSize, "Size"),
      density (p.getValueTreeState(), krain::params::density, "Density"),
      jitter (p.getValueTreeState(), krain::params::jitter, "Jitter"),
      positionSpray (p.getValueTreeState(), krain::params::positionSpray, "Pos Spray"),
      reverseProbability (p.getValueTreeState(), krain::params::reverseProbability, "Reverse"),
      pitch (p.getValueTreeState(), krain::params::pitch, "Pitch"),
      intervals (p.getValueTreeState(), krain::params::intervals, "Intervals"),
      pitchSpray (p.getValueTreeState(), krain::params::pitchSpray, "Spray"),
      drift (p.getValueTreeState(), krain::params::drift, "Drift"),
      filterCutoff (p.getValueTreeState(), krain::params::filterCutoff, "Filter"),
      diffusion (p.getValueTreeState(), krain::params::diffusion, "Diffusion"),
      stereoWidth (p.getValueTreeState(), krain::params::stereoWidth, "Width"),
      dryWet (p.getValueTreeState(), krain::params::dryWet, "Dry/Wet"),
      freeze (p.getValueTreeState(), krain::params::freeze, "Freeze")
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (cloud);

    for (auto* group : { &delayGroup, &grainGroup, &pitchGroup, &spaceGroup })
        addAndMakeVisible (*group);

    delayGroup.addControl (delayTime);
    delayGroup.addControl (syncEnabled);
    delayGroup.addControl (syncDivision);
    delayGroup.addControl (feedback);

    grainGroup.addControl (grainSize);
    grainGroup.addControl (density);
    grainGroup.addControl (jitter);
    grainGroup.addControl (positionSpray);
    grainGroup.addControl (reverseProbability);

    pitchGroup.addControl (pitch);
    pitchGroup.addControl (intervals);
    pitchGroup.addControl (pitchSpray);
    pitchGroup.addControl (drift);

    spaceGroup.addControl (filterCutoff);
    spaceGroup.addControl (diffusion);
    spaceGroup.addControl (stereoWidth);
    spaceGroup.addControl (dryWet);
    spaceGroup.addControl (freeze);

    setResizable (true, true);
    setResizeLimits (820, 560, 1640, 1120);
    setSize (940, 640);
}

KrainAudioProcessorEditor::~KrainAudioProcessorEditor()
{
    // Detach before the LookAndFeel member is destroyed, or the children would be
    // left pointing at freed memory during their own teardown.
    setLookAndFeel (nullptr);
}

//==============================================================================
void KrainAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace krain::gui;

    g.fillAll (palette::ink);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 0);

    // Wordmark, tracked out by hand. JUCE has no letter-spacing, and the spacing is
    // the point: it turns five letters into a mark rather than a word.
    const auto markFont = font (Face::display, 25.0f);
    g.setColour (palette::bone);

    auto x = (float) header.getX();
    const auto baseline = (float) header.getCentreY() + 8.0f;

    g.setFont (markFont);

    for (const auto character : processorRef.getName())
    {
        const juce::String glyph (juce::String::charToString (character));
        g.drawSingleLineText (glyph, juce::roundToInt (x), juce::roundToInt (baseline));
        x += juce::GlyphArrangement::getStringWidth (markFont, glyph) + 3.4f;
    }

    g.setColour (palette::boneDim);
    g.setFont (font (Face::caption, 12.0f));
    g.drawText ("granular delay", header.withTrimmedLeft (juce::roundToInt (x - (float) header.getX()) + 14),
                juce::Justification::centredLeft);

    g.setColour (palette::halo.withAlpha (0.75f));
    g.setFont (font (Face::value, 10.0f));
    g.drawText (juce::String (cloud.getNumVisibleGrains()) + " grains", header,
                juce::Justification::centredRight);

    g.setColour (palette::line);
    g.fillRect (margin, headerHeight - 1, getWidth() - margin * 2, 1);
}

void KrainAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);
    bounds.removeFromTop (headerHeight);

    cloud.setBounds (bounds.removeFromTop (cloudHeight));
    bounds.removeFromTop (margin);

    auto topRow = bounds.removeFromTop (bounds.getHeight() / 2);
    bounds.removeFromTop (margin);

    // Columns are sized by how many controls each group holds, so every knob ends
    // up the same width across the whole window.
    const auto split = [] (juce::Rectangle<int> row, int leftControls, int rightControls)
    {
        const auto leftWidth = row.getWidth() * leftControls / (leftControls + rightControls);
        return std::pair { row.removeFromLeft (leftWidth).withTrimmedRight (margin / 2), row.withTrimmedLeft (margin / 2) };
    };

    const auto [delayArea, grainArea] = split (topRow, 4, 5);
    delayGroup.setBounds (delayArea);
    grainGroup.setBounds (grainArea);

    const auto [pitchArea, spaceArea] = split (bounds, 4, 5);
    pitchGroup.setBounds (pitchArea);
    spaceGroup.setBounds (spaceArea);
}
