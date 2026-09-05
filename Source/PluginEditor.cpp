#include "PluginEditor.h"

//==============================================================================
GrainDelayAudioProcessorEditor::GrainDelayAudioProcessorEditor (GrainDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (720, 420);
}

GrainDelayAudioProcessorEditor::~GrainDelayAudioProcessorEditor() = default;

//==============================================================================
void GrainDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f));
    g.drawFittedText (processorRef.getName(), getLocalBounds(), juce::Justification::centred, 1);
}

void GrainDelayAudioProcessorEditor::resized()
{
}
