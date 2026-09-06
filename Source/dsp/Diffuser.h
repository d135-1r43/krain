#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

namespace krain
{

//==============================================================================
/** A Schroeder allpass section.

    Passes every frequency at equal level but smears them in time. That is the whole
    trick behind reverb-like diffusion: the spectrum is untouched, the transients are
    not. Chaining a few with mutually prime delay lengths turns a discrete echo into
    a wash.
*/
class Allpass
{
public:
    /** Allocates. prepareToPlay only. */
    void prepare (int maxDelaySamples)
    {
        buffer.assign ((size_t) juce::jmax (2, maxDelaySamples), 0.0f);
        writePosition = 0;
        delaySamples = juce::jlimit (1, (int) buffer.size() - 1, delaySamples);
    }

    void setDelaySamples (int samples) noexcept
    {
        delaySamples = juce::jlimit (1, (int) buffer.size() - 1, samples);
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    /** `gain` must stay below 1 for stability; the caller clamps it. */
    float process (float input, float gain) noexcept
    {
        if (buffer.empty())
            return input;

        auto readPosition = writePosition - delaySamples;

        if (readPosition < 0)
            readPosition += (int) buffer.size();

        const auto delayed = buffer[(size_t) readPosition];
        const auto stored = input + gain * delayed;

        buffer[(size_t) writePosition] = stored;

        if (++writePosition >= (int) buffer.size())
            writePosition = 0;

        return delayed - gain * stored;
    }

private:
    std::vector<float> buffer;
    int writePosition = 0;
    int delaySamples = 1;
};

//==============================================================================
/** Four allpasses in series.

    The stage lengths are mutually prime so their echo patterns never line up into
    an audible ringing. `spread` shifts one channel's lengths against the other's,
    which decorrelates left from right - that is where the width of the tail comes
    from, without any of it being fake stereo.
*/
class Diffuser
{
public:
    static constexpr int numStages = 4;

    /** Allocates. prepareToPlay only. */
    void prepare (double sampleRate, double spread)
    {
        // Milliseconds, chosen to be awkward multiples of each other.
        constexpr std::array<double, numStages> lengthsMs { 4.77, 9.31, 17.13, 29.71 };

        for (size_t i = 0; i < numStages; ++i)
        {
            const auto ms = lengthsMs[i] * spread;
            const auto samples = (int) std::ceil (ms * 0.001 * sampleRate) + 2;
            stages[i].prepare (samples);
            stages[i].setDelaySamples (samples - 2);
        }
    }

    void reset() noexcept
    {
        for (auto& stage : stages)
            stage.reset();
    }

    /** `amount` is 0..1; it is scaled to a safe allpass coefficient internally. */
    float process (float input, float amount) noexcept
    {
        const auto gain = juce::jlimit (0.0f, 0.72f, amount * 0.72f);

        if (gain <= 1.0e-5f)
            return input;

        auto value = input;

        for (auto& stage : stages)
            value = stage.process (value, gain);

        return value;
    }

private:
    std::array<Allpass, numStages> stages;
};

} // namespace krain
