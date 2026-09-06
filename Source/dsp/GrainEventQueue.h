#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace krain
{

//==============================================================================
/** One grain, described at the moment it was born. */
struct GrainEvent
{
    float pan = 0.0f;       ///< -1 hard left .. +1 hard right
    float semitones = 0.0f; ///< transposition away from unison
    float lengthMs = 0.0f;
    bool reversed = false;
};

//==============================================================================
/** A single-producer, single-consumer queue carrying grain births from the audio
    thread to the editor.

    juce::AbstractFifo does the index arithmetic without a lock; the storage is a
    fixed array, so pushing never allocates and never waits.

    When the queue is full - no editor open, or one that fell behind - new events
    are simply dropped. Losing a dot from a picture costs nothing; making the audio
    thread wait for the GUI costs a dropout.
*/
class GrainEventQueue
{
public:
    static constexpr int capacity = 512;

    // Declaring *any* constructor - and the non-copyable macro below declares a
    // deleted copy constructor - suppresses the implicit default one. So it has to
    // be asked for by name. (Java has no equivalent trap: a deleted copy there is
    // simply not a thing.)
    GrainEventQueue() = default;

    /** Audio thread only. Realtime-safe. */
    void push (const GrainEvent& event) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0)
            storage[(size_t) start1] = event;
        else if (size2 > 0)
            storage[(size_t) start2] = event;

        fifo.finishedWrite (size1 + size2);
    }

    /** Message thread only. Returns how many events were written to `destination`. */
    int pop (GrainEvent* destination, int maxItems) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (juce::jmin (maxItems, fifo.getNumReady()), start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
            destination[i] = storage[(size_t) (start1 + i)];

        for (int i = 0; i < size2; ++i)
            destination[size1 + i] = storage[(size_t) (start2 + i)];

        fifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

    /** Drops everything pending - used when an editor opens so it does not start
        by drawing a burst of stale grains. */
    void clear() noexcept { fifo.reset(); }

private:
    juce::AbstractFifo fifo { capacity };
    std::array<GrainEvent, (size_t) capacity> storage {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainEventQueue)
};

} // namespace krain
