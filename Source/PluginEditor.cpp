#include "PluginEditor.h"

#include "ParameterIds.h"

namespace
{
constexpr int headerHeight = 40;
constexpr int margin = 10;
} // namespace

//==============================================================================
KrainAudioProcessorEditor::KrainAudioProcessorEditor (KrainAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
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
    setResizeLimits (760, 440, 1520, 880);
    setSize (900, 480);
}

KrainAudioProcessorEditor::~KrainAudioProcessorEditor() = default;

//==============================================================================
void KrainAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (processorRef.getName(),
                getLocalBounds().removeFromTop (headerHeight).reduced (margin + 6, 0),
                juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (13.0f));
    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.6f));
    g.drawText ("granular delay",
                getLocalBounds().removeFromTop (headerHeight).reduced (margin + 6, 0),
                juce::Justification::centredRight);
}

void KrainAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);
    bounds.removeFromTop (headerHeight);

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
