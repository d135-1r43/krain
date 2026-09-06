#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/GrainEventQueue.h"
#include "gui/KrainLookAndFeel.h"

#include <array>
#include <vector>

namespace krain::gui
{

//==============================================================================
/** The grain cloud.

    Every dot is a grain the DSP actually triggered - horizontal position is where
    it sits in the stereo field, vertical position is how far it was transposed,
    and its brightness follows the same window curve that shapes it in the audio.

    The horizon is unison. Everything above it is the halo: with the Shimmer
    interval set you can watch roughly half the grains hold the line while the rest
    float up to the octave and the twelfth. That structure is the whole idea behind
    the effect, and it is not visible in any number on the panel.

    Grains are animated here rather than polled from the engine. The audio thread
    posts a birth and moves on; this view knows the grain's length, so it can live
    out the rest on its own without ever touching engine state.
*/
class GrainCloudView final : public juce::Component,
                             private juce::Timer
{
public:
    explicit GrainCloudView (GrainEventQueue& queue);
    ~GrainCloudView() override;

    void paint (juce::Graphics&) override;

    /** Drains new births and ages the existing ones. The timer calls this; tests
        and offline renders can drive it by hand. */
    void advance (double deltaSeconds);

    int getNumVisibleGrains() const noexcept { return (int) particles.size(); }

private:
    void timerCallback() override;

    float yForSemitones (float semitones, juce::Rectangle<float> area) const noexcept;

    struct Particle
    {
        float pan = 0.0f;
        float semitones = 0.0f;
        float radius = 2.0f;
        bool reversed = false;
        double age = 0.0;
        double life = 0.1;
    };

    GrainEventQueue& eventQueue;
    std::vector<Particle> particles;
    std::array<GrainEvent, 128> drained {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainCloudView)
};

} // namespace krain::gui
