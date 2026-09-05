#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** The plug-in's audio engine.

    JUCE instantiates exactly one of these per plug-in instance; the host owns it
    and deletes it when the instance goes away.

    C++ note (Java dev): there is no garbage collector here. Every member below is
    either a value (stack/inline storage, destroyed automatically with the object -
    that is "RAII") or a std::unique_ptr, which is the C++ equivalent of "this object
    owns that one and frees it in its destructor".
*/
class GrainDelayAudioProcessor final : public juce::AudioProcessor
{
public:
    GrainDelayAudioProcessor();
    ~GrainDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    /** Called on the audio thread. Hard realtime: no allocation, no locks, no logging. */
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    // JUCE_DECLARE_NON_COPYABLE... is a macro that deletes the copy constructor and
    // assignment operator. In Java every object is a reference; in C++ objects are
    // values and would be copied silently, which is never what you want for a
    // processor. The macro also adds a leak detector that fires in debug builds.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainDelayAudioProcessor)
};
