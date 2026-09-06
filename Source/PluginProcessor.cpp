#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using namespace krain;

juce::String msSuffix() { return " ms"; }

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id,
                                                      const juce::String& name,
                                                      juce::NormalisableRange<float> range,
                                                      float defaultValue,
                                                      const juce::String& suffix)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 },
                                                        name,
                                                        range,
                                                        defaultValue,
                                                        juce::AudioParameterFloatAttributes().withLabel (suffix));
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout KrainAudioProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Skewed ranges put the musically interesting values in the middle of the knob.
    Range delayRange { 1.0f, 4000.0f, 0.1f };
    delayRange.setSkewForCentre (500.0f);

    Range grainRange { 5.0f, 1000.0f, 0.1f };
    grainRange.setSkewForCentre (120.0f);

    Range densityRange { 0.5f, 100.0f, 0.01f };
    densityRange.setSkewForCentre (20.0f);

    Range cutoffRange { 20.0f, 20000.0f, 1.0f };
    cutoffRange.setSkewForCentre (2000.0f);

    layout.add (makeFloat (params::delayTime, "Delay Time", delayRange, 350.0f, msSuffix()));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { params::syncEnabled, 1 },
                                                            "Tempo Sync",
                                                            false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { params::syncDivision, 1 },
                                                              "Division",
                                                              params::divisionNames(),
                                                              8)); // 1/4

    layout.add (makeFloat (params::grainSize, "Grain Size", grainRange, 120.0f, msSuffix()));
    layout.add (makeFloat (params::density, "Density", densityRange, 20.0f, " /s"));
    layout.add (makeFloat (params::jitter, "Jitter", Range { 0.0f, 1.0f, 0.001f }, 0.25f, {}));

    layout.add (makeFloat (params::pitch, "Pitch", Range { -24.0f, 24.0f, 0.01f }, 0.0f, " st"));
    layout.add (makeFloat (params::pitchSpray, "Pitch Spray", Range { 0.0f, 12.0f, 0.01f }, 0.0f, " st"));

    // Defaults to Shimmer, not Free: out of the box this should already do the
    // thing it is for, rather than behave like a plain delay until you find it.
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { params::intervals, 1 },
                                                              "Intervals",
                                                              params::intervalNames(),
                                                              3));
    layout.add (makeFloat (params::positionSpray, "Position Spray", Range { 0.0f, 500.0f, 0.1f }, 0.0f, msSuffix()));
    layout.add (makeFloat (params::reverseProbability, "Reverse", Range { 0.0f, 1.0f, 0.001f }, 0.0f, {}));

    layout.add (makeFloat (params::feedback, "Feedback", Range { 0.0f, 1.2f, 0.001f }, 0.4f, {}));
    layout.add (makeFloat (params::filterCutoff, "Filter", cutoffRange, 8000.0f, " Hz"));
    layout.add (makeFloat (params::dryWet, "Dry/Wet", Range { 0.0f, 1.0f, 0.001f }, 0.5f, {}));
    layout.add (makeFloat (params::stereoWidth, "Width", Range { 0.0f, 1.0f, 0.001f }, 0.8f, {}));
    layout.add (makeFloat (params::diffusion, "Diffusion", Range { 0.0f, 1.0f, 0.001f }, 0.4f, {}));
    layout.add (makeFloat (params::drift, "Drift", Range { 0.0f, 1.0f, 0.001f }, 0.3f, {}));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { params::freeze, 1 },
                                                            "Freeze",
                                                            false));

    return layout;
}

//==============================================================================
KrainAudioProcessor::KrainAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KRAIN", createParameterLayout())
{
    // Resolve the parameter pointers once, here, so processBlock() never has to
    // look anything up by name.
    const auto fetch = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    delayTimeParam = fetch (krain::params::delayTime);
    syncEnabledParam = fetch (krain::params::syncEnabled);
    syncDivisionParam = fetch (krain::params::syncDivision);
    grainSizeParam = fetch (krain::params::grainSize);
    densityParam = fetch (krain::params::density);
    jitterParam = fetch (krain::params::jitter);
    pitchParam = fetch (krain::params::pitch);
    pitchSprayParam = fetch (krain::params::pitchSpray);
    intervalsParam = fetch (krain::params::intervals);
    positionSprayParam = fetch (krain::params::positionSpray);
    reverseProbabilityParam = fetch (krain::params::reverseProbability);
    feedbackParam = fetch (krain::params::feedback);
    filterCutoffParam = fetch (krain::params::filterCutoff);
    dryWetParam = fetch (krain::params::dryWet);
    stereoWidthParam = fetch (krain::params::stereoWidth);
    diffusionParam = fetch (krain::params::diffusion);
    driftParam = fetch (krain::params::drift);
    freezeParam = fetch (krain::params::freeze);
}

KrainAudioProcessor::~KrainAudioProcessor() = default;

//==============================================================================
void KrainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // This is where every allocation happens. After this returns, the audio thread
    // must never need another byte of heap.
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
    engine.setParameters (collectParameters());
    engine.reset();
}

void KrainAudioProcessor::releaseResources()
{
}

bool KrainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
float KrainAudioProcessor::resolveDelayTimeMs() const noexcept
{
    if (syncEnabledParam->load() < 0.5f)
        return delayTimeParam->load();

    const auto index = juce::jlimit (0, (int) krain::params::divisions.size() - 1,
                                     (int) syncDivisionParam->load());
    const auto quarterNotes = krain::params::divisions[(size_t) index].quarterNotes;
    const auto quarterNoteMs = 60000.0 / juce::jlimit (20.0, 300.0, hostBpm);

    return (float) juce::jlimit (1.0, 4000.0, quarterNotes * quarterNoteMs);
}

krain::GrainEngine::Parameters KrainAudioProcessor::collectParameters() const noexcept
{
    krain::GrainEngine::Parameters p;

    p.delayTimeMs = resolveDelayTimeMs();
    p.grainSizeMs = grainSizeParam->load();
    p.densityHz = densityParam->load();
    p.jitter = jitterParam->load();
    p.pitchSemitones = pitchParam->load();
    p.pitchSpraySemitones = pitchSprayParam->load();
    p.intervals = static_cast<krain::IntervalSet> (
        juce::jlimit (0, 4, (int) intervalsParam->load()));
    p.positionSprayMs = positionSprayParam->load();
    p.reverseProbability = reverseProbabilityParam->load();
    p.feedback = feedbackParam->load();
    p.filterCutoffHz = filterCutoffParam->load();
    p.dryWet = dryWetParam->load();
    p.stereoWidth = stereoWidthParam->load();
    p.diffusion = diffusionParam->load();
    p.drift = driftParam->load();
    p.freeze = freezeParam->load() >= 0.5f;

    return p;
}

void KrainAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);

    // Tells the CPU to flush denormals to zero for the lifetime of this scope.
    // Classic RAII: the constructor sets the FPU flag, the destructor restores it.
    juce::ScopedNoDenormals noDenormals;

    // Any output channel that has no matching input must be cleared, otherwise it
    // contains whatever the host left in it.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto bpm = position->getBpm())
                hostBpm = *bpm;

    engine.setParameters (collectParameters());
    engine.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* KrainAudioProcessor::createEditor()
{
    // The host takes ownership of the returned pointer and deletes it.
    return new KrainAudioProcessorEditor (*this);
}

void KrainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KrainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// The entry point the plug-in wrappers call to create an instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KrainAudioProcessor();
}
