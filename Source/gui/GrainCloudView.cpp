#include "gui/GrainCloudView.h"

namespace krain::gui
{

namespace
{
constexpr int frameRateHz = 40;
constexpr float semitoneRange = 24.0f;

// Unison sits below centre on purpose: krain's interval sets mostly rise, so the
// halo gets the room it needs and the layout says which way the effect goes.
constexpr float unisonHeightFraction = 0.66f;

constexpr float minLengthMs = 5.0f;
constexpr float maxLengthMs = 1000.0f;

// A grain lives for a tenth of a second, so drawing only living grains leaves two
// or three dots on screen - true, and useless. The afterglow keeps a fading trace
// the way a phosphor screen does, so the *shape* of the cloud becomes legible
// while the bright cores stay honest about what is sounding right now.
constexpr double afterglowSeconds = 1.6;
} // namespace

//==============================================================================
GrainCloudView::GrainCloudView (GrainEventQueue& queue) : eventQueue (queue)
{
    // The pool can hold 64 live grains and each may linger a moment; reserving here
    // means no allocation happens while animating.
    particles.reserve (1024);

    eventQueue.clear(); // do not open with a burst of stale grains
    setInterceptsMouseClicks (false, false);
    startTimerHz (frameRateHz);
}

GrainCloudView::~GrainCloudView() { stopTimer(); }

void GrainCloudView::timerCallback() { advance (1.0 / (double) frameRateHz); }

//==============================================================================
void GrainCloudView::advance (double deltaSeconds)
{
    const auto count = eventQueue.pop (drained.data(), (int) drained.size());

    for (int i = 0; i < count; ++i)
    {
        const auto& event = drained[(size_t) i];

        if (particles.size() >= particles.capacity())
            break;

        Particle particle;
        particle.pan = juce::jlimit (-1.0f, 1.0f, event.pan);
        particle.semitones = juce::jlimit (-semitoneRange, semitoneRange, event.semitones);
        particle.reversed = event.reversed;
        particle.life = juce::jmax (0.02, (double) event.lengthMs * 0.001);

        // Longer grains read as heavier points. Square-rooted, or a one-second grain
        // would be two hundred times the area of a five-millisecond one.
        const auto normalised = juce::jlimit (0.0f, 1.0f,
                                              (event.lengthMs - minLengthMs) / (maxLengthMs - minLengthMs));
        particle.radius = 1.4f + 3.4f * std::sqrt (normalised);

        particles.push_back (particle);
    }

    for (auto& particle : particles)
        particle.age += deltaSeconds;

    std::erase_if (particles, [] (const Particle& p) { return p.age >= p.life + afterglowSeconds; });

    repaint();
}

//==============================================================================
float GrainCloudView::yForSemitones (float semitones, juce::Rectangle<float> area) const noexcept
{
    const auto horizon = area.getY() + area.getHeight() * unisonHeightFraction;

    if (semitones >= 0.0f)
        return horizon - (semitones / semitoneRange) * (horizon - area.getY());

    return horizon + (-semitones / semitoneRange) * (area.getBottom() - horizon);
}

void GrainCloudView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto field = bounds.reduced (1.0f);

    // Deeper at the floor, lighter at the top - the ground itself rises.
    g.setGradientFill (juce::ColourGradient (palette::ink.brighter (0.06f), bounds.getCentreX(), bounds.getY(),
                                             palette::inkDeep, bounds.getCentreX(), bounds.getBottom(), false));
    g.fillRect (bounds);

    // --- structure, drawn whether or not anything is playing ------------------
    const auto horizon = yForSemitones (0.0f, field);

    for (const auto semitones : { 19.0f, 12.0f, -12.0f })
    {
        const auto y = yForSemitones (semitones, field);
        g.setColour (palette::forSemitones (semitones).withAlpha (0.16f));
        g.fillRect (field.getX() + 40.0f, y, field.getWidth() - 52.0f, 1.0f);

        g.setColour (palette::forSemitones (semitones).withAlpha (0.55f));
        g.setFont (font (Face::value, 9.5f));
        g.drawText (semitones > 0 ? "+" + juce::String ((int) semitones) : juce::String ((int) semitones),
                    juce::Rectangle<float> (field.getX() + 6.0f, y - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    g.setColour (palette::line.brighter (0.25f));
    g.fillRect (field.getX() + 40.0f, horizon, field.getWidth() - 52.0f, 1.0f);
    g.setColour (palette::boneDim.withAlpha (0.75f));
    g.setFont (font (Face::value, 9.5f));
    g.drawText ("0", juce::Rectangle<float> (field.getX() + 6.0f, horizon - 7.0f, 30.0f, 14.0f),
                juce::Justification::centredLeft);

    g.setColour (palette::boneDim.withAlpha (0.35f));
    g.setFont (font (Face::caption, 10.0f));
    g.drawText ("L", field.reduced (8.0f, 6.0f), juce::Justification::bottomLeft);
    g.drawText ("R", field.reduced (8.0f, 6.0f), juce::Justification::bottomRight);

    // --- the grains ----------------------------------------------------------
    for (const auto& particle : particles)
    {
        const auto sounding = particle.age < particle.life;

        float core = 0.0f, glow = 0.0f;

        if (sounding)
        {
            // The same shape that windows the grain in the audio, so a dot brightens
            // and fades exactly as its sound does.
            const auto phase = juce::jlimit (0.0, 1.0, particle.age / particle.life);
            core = (float) std::sin (juce::MathConstants<double>::pi * phase);
            glow = core * 0.16f;
        }
        else
        {
            const auto fade = (float) juce::jlimit (0.0, 1.0,
                                                    1.0 - (particle.age - particle.life) / afterglowSeconds);
            core = fade * fade * 0.34f;
            glow = fade * 0.07f;
        }

        if (core <= 0.01f && glow <= 0.005f)
            continue;

        const auto x = field.getCentreX() + particle.pan * field.getWidth() * 0.46f;
        const auto y = yForSemitones (particle.semitones, field);
        const auto colour = palette::forSemitones (particle.semitones);

        // A soft halo under each point is what makes the cloud read as luminous
        // rather than as a scatter plot.
        if (glow > 0.005f)
        {
            g.setColour (colour.withAlpha (glow));
            g.fillEllipse (x - particle.radius * 3.0f, y - particle.radius * 3.0f,
                           particle.radius * 6.0f, particle.radius * 6.0f);
        }

        g.setColour (colour.withAlpha (core));

        if (particle.reversed)
            g.drawEllipse (x - particle.radius, y - particle.radius,
                           particle.radius * 2.0f, particle.radius * 2.0f, 1.2f);
        else
            g.fillEllipse (x - particle.radius, y - particle.radius,
                           particle.radius * 2.0f, particle.radius * 2.0f);
    }

    if (particles.empty())
    {
        g.setColour (palette::boneDim.withAlpha (0.5f));
        g.setFont (font (Face::caption, 12.0f));
        g.drawText ("Play something through krain to see the cloud",
                    field, juce::Justification::centred);
    }

    g.setColour (palette::line);
    g.drawRect (bounds, 1.0f);
}

} // namespace krain::gui
