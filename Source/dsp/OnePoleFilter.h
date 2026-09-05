#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace graindelay
{

//==============================================================================
/** The simplest useful filter: a one-pole (6 dB/octave) lowpass, plus its
    complementary highpass for free.

    y[n] = (1 - a) * x[n] + a * y[n-1]

    `a` is the pole position, between 0 (wide open) and just below 1 (very dark).
    Computing it needs an exp(), which is far too expensive to do per sample - so
    the coefficient is passed in and the caller smooths *it* instead of the cutoff.
*/
class OnePoleFilter
{
public:
    /** Turns a cutoff in Hz into a pole coefficient. Call once per block, not per sample. */
    static float coefficientFor (float cutoffHz, double sampleRate) noexcept
    {
        const auto nyquist = (float) (sampleRate * 0.5);
        const auto clamped = juce::jlimit (10.0f, juce::jmax (20.0f, nyquist * 0.99f), cutoffHz);
        const auto a = std::exp (-2.0f * juce::MathConstants<float>::pi * clamped / (float) sampleRate);

        return juce::jlimit (0.0f, 0.9999f, a);
    }

    void reset() noexcept { state = 0.0f; }

    float processLowpass (float input, float coefficient) noexcept
    {
        state = (1.0f - coefficient) * input + coefficient * state;
        return state;
    }

    float processHighpass (float input, float coefficient) noexcept
    {
        return input - processLowpass (input, coefficient);
    }

private:
    float state = 0.0f;
};

//==============================================================================
/** Symmetric soft saturation. Placed in the feedback loop so that a feedback
    amount at or even above 1.0 saturates into a stable, musical limit instead of
    running away to infinity. */
inline float softClip (float x) noexcept
{
    return std::tanh (x);
}

} // namespace graindelay
