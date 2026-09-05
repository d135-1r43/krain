#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "gui/ParameterComponents.h"

//==============================================================================
/** The plug-in window.

    Owned by the host via the pointer returned from createEditor(). The reference
    to the processor is safe because the processor always outlives its editor.

    No custom LookAndFeel yet - on purpose. All drawing goes through LookAndFeel
    colour ids, so adding one later is a single setLookAndFeel() call here plus a
    new class, with no changes to the controls themselves.
*/
class GrainDelayAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GrainDelayAudioProcessorEditor (GrainDelayAudioProcessor&);
    ~GrainDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GrainDelayAudioProcessor& processorRef;

    // Declaration order matters: the controls hold attachments into the APVTS and
    // must be gone before the groups that merely point at them. Groups are declared
    // first, so they are destroyed last.
    graindelay::gui::ParameterGroup delayGroup { "DELAY" };
    graindelay::gui::ParameterGroup grainGroup { "GRAIN" };
    graindelay::gui::ParameterGroup modulationGroup { "MODULATION" };
    graindelay::gui::ParameterGroup outputGroup { "OUTPUT" };

    graindelay::gui::ParameterSlider delayTime;
    graindelay::gui::ParameterToggle syncEnabled;
    graindelay::gui::ParameterChoice syncDivision;
    graindelay::gui::ParameterSlider feedback;

    graindelay::gui::ParameterSlider grainSize;
    graindelay::gui::ParameterSlider density;
    graindelay::gui::ParameterSlider jitter;

    graindelay::gui::ParameterSlider pitch;
    graindelay::gui::ParameterSlider pitchSpray;
    graindelay::gui::ParameterSlider positionSpray;
    graindelay::gui::ParameterSlider reverseProbability;

    graindelay::gui::ParameterSlider filterCutoff;
    graindelay::gui::ParameterSlider dryWet;
    graindelay::gui::ParameterToggle freeze;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainDelayAudioProcessorEditor)
};
