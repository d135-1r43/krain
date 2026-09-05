#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace krain
{

//==============================================================================
/** A multi-channel circular ("ring") buffer with fractional, linearly
    interpolated reads.

    Sizing: at least 4 seconds at the current sample rate, and never smaller than
    4 seconds at 96 kHz.

    Realtime rules of the house:
      - prepare() allocates. Call it from prepareToPlay(), never from processBlock().
      - everything else is marked noexcept and touches no memory it does not own.

    C++ note (Java dev): the storage is a juce::AudioBuffer<float> *by value*, not a
    pointer. It allocates in its own constructor/setSize and frees in its destructor -
    that is RAII. No delete, no finally-block, no leak, even if an exception unwinds.
*/
class DelayBuffer
{
public:
    /** A pre-resolved read position: two sample indices plus the fraction between
        them. Computing this once per grain and reusing it for every channel keeps
        the expensive wrapping out of the per-channel inner loop. */
    struct ReadPoint
    {
        int index0 = 0;
        int index1 = 0;
        float fraction = 0.0f;
    };

    /** Allocates. Only ever call this from prepareToPlay(). */
    void prepare (int numChannels, double sampleRate);

    /** Zeroes the contents and rewinds the write head. Realtime-safe. */
    void reset() noexcept;

    int getNumChannels() const noexcept { return buffer.getNumChannels(); }
    int getNumSamples() const noexcept { return buffer.getNumSamples(); }

    /** Current write head, as a sample index into the ring. */
    int getWritePosition() const noexcept { return writePosition; }

    /** Writes one sample for one channel at the current write head. */
    void writeFrame (int channel, float value) noexcept;

    /** Moves the write head on by one sample, wrapping around. */
    void advanceWrite() noexcept;

    /** Folds any real index back into [0, size). Cheap: assumes the input is at
        most one buffer length out of range, which is all a grain can ever be. */
    double wrap (double index) const noexcept;

    /** Resolves a fractional index into a ReadPoint. */
    ReadPoint makeReadPoint (double index) const noexcept;

    /** Reads one channel at a previously resolved position. */
    float read (int channel, ReadPoint point) const noexcept;

    /** Convenience: wrap + resolve + read in one go. Handy in tests. */
    float readInterpolated (int channel, double index) const noexcept;

private:
    juce::AudioBuffer<float> buffer;
    int writePosition = 0;

    JUCE_LEAK_DETECTOR (DelayBuffer)
};

} // namespace krain
