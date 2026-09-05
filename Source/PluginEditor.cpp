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
      pitch (p.getValueTreeState(), krain::params::pitch, "Pitch"),
      pitchSpray (p.getValueTreeState(), krain::params::pitchSpray, "Pitch Spray"),
      positionSpray (p.getValueTreeState(), krain::params::positionSpray, "Pos Spray"),
      reverseProbability (p.getValueTreeState(), krain::params::reverseProbability, "Reverse"),
      filterCutoff (p.getValueTreeState(), krain::params::filterCutoff, "Filter"),
      dryWet (p.getValueTreeState(), krain::params::dryWet, "Dry/Wet"),
      freeze (p.getValueTreeState(), krain::params::freeze, "Freeze")
{
    for (auto* group : { &delayGroup, &grainGroup, &modulationGroup, &outputGroup })
        addAndMakeVisible (*group);

    delayGroup.addControl (delayTime);
    delayGroup.addControl (syncEnabled);
    delayGroup.addControl (syncDivision);
    delayGroup.addControl (feedback);

    grainGroup.addControl (grainSize);
    grainGroup.addControl (density);
    grainGroup.addControl (jitter);

    modulationGroup.addControl (pitch);
    modulationGroup.addControl (pitchSpray);
    modulationGroup.addControl (positionSpray);
    modulationGroup.addControl (reverseProbability);

    outputGroup.addControl (filterCutoff);
    outputGroup.addControl (dryWet);
    outputGroup.addControl (freeze);

    setResizable (true, true);
    setResizeLimits (700, 420, 1400, 840);
    setSize (820, 460);
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

    const auto [delayArea, grainArea] = split (topRow, 4, 3);
    delayGroup.setBounds (delayArea);
    grainGroup.setBounds (grainArea);

    const auto [modArea, outArea] = split (bounds, 4, 3);
    modulationGroup.setBounds (modArea);
    outputGroup.setBounds (outArea);
}
