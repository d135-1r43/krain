#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GrainDelayAudioProcessor::GrainDelayAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

GrainDelayAudioProcessor::~GrainDelayAudioProcessor() = default;

//==============================================================================
void GrainDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void GrainDelayAudioProcessor::releaseResources()
{
}

bool GrainDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void GrainDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);

    // Tells the CPU to flush denormals to zero for the lifetime of this scope.
    // Classic RAII: the constructor sets the FPU flag, the destructor restores it.
    juce::ScopedNoDenormals noDenormals;

    // Any output channel that has no matching input must be cleared, otherwise it
    // contains whatever the host left in it.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Pass-through for now - the DSP lands in the next step.
}

//==============================================================================
juce::AudioProcessorEditor* GrainDelayAudioProcessor::createEditor()
{
    // The host takes ownership of the returned pointer and deletes it.
    return new GrainDelayAudioProcessorEditor (*this);
}

void GrainDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void GrainDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// The entry point the plug-in wrappers call to create an instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainDelayAudioProcessor();
}
