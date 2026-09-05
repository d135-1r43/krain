#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIds.h"
#include "dsp/GrainEngine.h"

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

    //==============================================================================
    /** The editor attaches its sliders to this. */
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //==============================================================================
    /** Reads every parameter into a plain snapshot. Called once per block on the
        audio thread - all reads go through std::atomic, so no locking is needed. */
    graindelay::GrainEngine::Parameters collectParameters() const noexcept;

    /** Resolves the delay time, honouring tempo sync when it is switched on. */
    float resolveDelayTimeMs() const noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // Cached raw pointers into the APVTS. getRawParameterValue() does a string lookup,
    // which is fine in the constructor but not in processBlock(). The APVTS owns the
    // atomics; these are non-owning observers and stay valid for its lifetime.
    std::atomic<float>* delayTimeParam = nullptr;
    std::atomic<float>* syncEnabledParam = nullptr;
    std::atomic<float>* syncDivisionParam = nullptr;
    std::atomic<float>* grainSizeParam = nullptr;
    std::atomic<float>* densityParam = nullptr;
    std::atomic<float>* jitterParam = nullptr;
    std::atomic<float>* pitchParam = nullptr;
    std::atomic<float>* pitchSprayParam = nullptr;
    std::atomic<float>* positionSprayParam = nullptr;
    std::atomic<float>* reverseProbabilityParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* filterCutoffParam = nullptr;
    std::atomic<float>* dryWetParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;

    graindelay::GrainEngine engine;

    /** Host tempo, refreshed once per block on the audio thread and read by
        resolveDelayTimeMs(). Plain double: only the audio thread touches it. */
    double hostBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainDelayAudioProcessor)
};
