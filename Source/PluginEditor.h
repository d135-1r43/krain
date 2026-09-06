#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "gui/GrainCloudView.h"
#include "gui/KrainLookAndFeel.h"
#include "gui/ParameterComponents.h"

//==============================================================================
/** The plug-in window.

    Owned by the host via the pointer returned from createEditor(). The reference
    to the processor is safe because the processor always outlives its editor.

    No custom LookAndFeel yet - on purpose. All drawing goes through LookAndFeel
    colour ids, so adding one later is a single setLookAndFeel() call here plus a
    new class, with no changes to the controls themselves.
*/
class KrainAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit KrainAudioProcessorEditor (KrainAudioProcessor&);
    ~KrainAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Drives the cloud animation by hand. The timer does this in normal use;
        offline renders and tests need to step it themselves. */
    void advanceCloud (double deltaSeconds) { cloud.advance (deltaSeconds); }

private:
    KrainAudioProcessor& processorRef;

    // Declared before every control: a LookAndFeel must outlive the components that
    // point at it, and members are destroyed in reverse order.
    krain::gui::KrainLookAndFeel lookAndFeel;

    krain::gui::GrainCloudView cloud;

    // Declaration order matters: the controls hold attachments into the APVTS and
    // must be gone before the groups that merely point at them. Groups are declared
    // first, so they are destroyed last.
    krain::gui::ParameterGroup delayGroup { "DELAY" };
    krain::gui::ParameterGroup grainGroup { "GRAIN" };
    krain::gui::ParameterGroup pitchGroup { "PITCH" };
    krain::gui::ParameterGroup spaceGroup { "SPACE" };

    krain::gui::ParameterSlider delayTime;
    krain::gui::ParameterToggle syncEnabled;
    krain::gui::ParameterChoice syncDivision;
    krain::gui::ParameterSlider feedback;

    krain::gui::ParameterSlider grainSize;
    krain::gui::ParameterSlider density;
    krain::gui::ParameterSlider jitter;
    krain::gui::ParameterSlider positionSpray;
    krain::gui::ParameterSlider reverseProbability;

    krain::gui::ParameterSlider pitch;
    krain::gui::ParameterChoice intervals;
    krain::gui::ParameterSlider pitchSpray;
    krain::gui::ParameterSlider drift;

    krain::gui::ParameterSlider filterCutoff;
    krain::gui::ParameterSlider diffusion;
    krain::gui::ParameterSlider stereoWidth;
    krain::gui::ParameterSlider dryWet;
    krain::gui::ParameterToggle freeze;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KrainAudioProcessorEditor)
};
