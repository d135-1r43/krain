#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/DelayBuffer.h"
#include "dsp/OnePoleFilter.h"

#include <array>
#include <vector>

namespace krain
{

//==============================================================================
/** One playing grain: a short tape head scrubbing through the delay buffer.

    Plain data, no virtuals, no ownership - grains live inside a fixed array in the
    engine and are recycled. Nothing is ever allocated or freed for a grain.
*/
struct Grain
{
    bool active = false;
    double readPosition = 0.0; ///< fractional index into the delay buffer
    double increment = 1.0;    ///< pitch ratio; negative plays the grain backwards
    int age = 0;               ///< samples elapsed since the grain started
    int length = 1;            ///< total lifetime in samples
    float gain = 1.0f;
};

//==============================================================================
/** The granular delay engine: delay buffer + grain pool + scheduler + feedback path.

    Deliberately free of any JUCE plug-in types so it can be unit tested offline
    without instantiating a host or an AudioProcessor.

    Threading contract:
      - prepare() / reset() are called from the message or prepare thread. They allocate.
      - setParameters() may be called from the audio thread; it only copies POD.
      - process() is hard realtime: no allocation, no locks, no logging, no exceptions.
*/
class GrainEngine
{
public:
    static constexpr int maxGrains = 64;
    static constexpr int maxChannels = 2;

    /** Plain snapshot of every knob. Copied by value into the engine - no shared
        state, so nothing to lock. */
    struct Parameters
    {
        float delayTimeMs = 350.0f;
        float grainSizeMs = 120.0f;
        float densityHz = 20.0f;
        float jitter = 0.25f;               ///< 0..1, randomises the trigger interval
        float pitchSemitones = 0.0f;        ///< -24..+24
        float pitchSpraySemitones = 0.0f;   ///< 0..12, random per-grain detune
        float positionSprayMs = 0.0f;       ///< 0..500, random per-grain start offset
        float reverseProbability = 0.0f;    ///< 0..1
        float feedback = 0.4f;              ///< 0..1.2
        float filterCutoffHz = 8000.0f;     ///< lowpass inside the feedback loop
        float dryWet = 0.5f;                ///< 0 = dry, 1 = wet
        bool freeze = false;                ///< stop writing into the delay buffer
    };

    GrainEngine();

    /** Allocates every buffer the engine will ever need. Call from prepareToPlay(). */
    void prepare (double sampleRate, int numChannels);

    /** Silences the engine and kills all grains. Realtime-safe. */
    void reset() noexcept;

    /** Copies in a new parameter snapshot. Realtime-safe. */
    void setParameters (const Parameters& newParameters) noexcept;

    /** Processes a block in place. Hard realtime - see the class comment. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    /** Tukey window shape: 1.0 = Hann (fully tapered), 0.0 = rectangular.
        Values in between give a flatter grain with a sustained middle. */
    void setWindowShape (float alpha) noexcept;

    int getNumActiveGrains() const noexcept;
    const DelayBuffer& getDelayBuffer() const noexcept { return delay; }

private:
    void triggerGrain() noexcept;
    void scheduleNextGrain() noexcept;
    float windowValue (double normalisedAge) const noexcept;
    float lookupHann (double phase) const noexcept;
    float nextBipolarRandom() noexcept;

    static constexpr int hannTableSize = 1024;

    double sampleRate = 44100.0;
    int activeChannels = 2;

    DelayBuffer delay;

    // Fixed-size pool. std::array is a value type with its storage inline - unlike a
    // Java array there is no separate heap object and no null to check.
    std::array<Grain, maxGrains> grains {};

    std::array<OnePoleFilter, maxChannels> feedbackLowpass {};
    std::array<OnePoleFilter, maxChannels> feedbackDcBlocker {};

    // Allocated once in prepare(); only read from in process().
    std::vector<float> hannTable;

    Parameters parameters {};

    juce::SmoothedValue<float> smoothedDelaySamples;
    juce::SmoothedValue<float> smoothedFeedback;
    juce::SmoothedValue<float> smoothedDryWet;
    juce::SmoothedValue<float> smoothedLowpassCoeff;

    float currentDelaySamples = 0.0f;
    float dcBlockerCoeff = 0.99f;
    float windowAlpha = 1.0f;

    double samplesUntilNextGrain = 0.0;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainEngine)
};

} // namespace krain
