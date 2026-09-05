#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
/** The plug-in window.

    Owned by the host via the pointer returned from createEditor(). The reference
    to the processor is safe because the processor always outlives its editor.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainDelayAudioProcessorEditor)
};
