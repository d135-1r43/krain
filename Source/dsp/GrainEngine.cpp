#include "dsp/GrainEngine.h"

#include <cmath>

namespace krain
{

namespace
{
constexpr double smoothingSeconds = 0.05;
constexpr float dcBlockerCutoffHz = 25.0f;
constexpr int minGrainSamples = 16;

// How far the drift oscillators may pull the grain start point, at drift = 1.
constexpr double maxDriftMs = 55.0;

// How far apart a grain's two read points may sit. Kept under the Haas limit so it
// reads as width rather than as a separate echo.
constexpr double maxChannelOffsetMs = 26.0;
constexpr double driftRateA = 0.071; // Hz
constexpr double driftRateB = 0.113; // Hz
} // namespace

GrainEngine::GrainEngine()
{
    // Fixed seed: an offline render of the same input gives the same output, which
    // is what makes the unit tests reproducible.
    random.setSeed (0x9E3779B9);
}

//==============================================================================
void GrainEngine::prepare (double newSampleRate, int numChannels)
{
    sampleRate = juce::jmax (8000.0, newSampleRate);
    activeChannels = juce::jlimit (1, maxChannels, numChannels);

    delay.prepare (maxChannels, sampleRate);

    // Window lookup table. One extra entry so the interpolation can always read
    // table[i + 1] without a bounds check in the audio thread.
    hannTable.assign ((size_t) hannTableSize + 1, 0.0f);

    for (size_t i = 0; i <= (size_t) hannTableSize; ++i)
    {
        const auto phase = (double) i / (double) hannTableSize;
        hannTable[i] = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * phase));
    }

    // The two channels get slightly different allpass lengths. That single
    // asymmetry is what makes the diffused tail wide, rather than a mono wash
    // duplicated to both sides.
    diffusers[0].prepare (sampleRate, 1.0);
    diffusers[1].prepare (sampleRate, 1.17);

    dcBlockerCoeff = OnePoleFilter::coefficientFor (dcBlockerCutoffHz, sampleRate);

    for (auto& value : { &smoothedDelaySamples, &smoothedFeedback, &smoothedDryWet, &smoothedLowpassCoeff })
        value->reset (sampleRate, smoothingSeconds);

    reset();
}

void GrainEngine::reset() noexcept
{
    delay.reset();

    for (auto& grain : grains)
        grain.active = false;

    for (auto& filter : feedbackLowpass)
        filter.reset();

    for (auto& filter : feedbackDcBlocker)
        filter.reset();

    for (auto& diffuser : diffusers)
        diffuser.reset();

    driftPhaseA = 0.0;
    driftPhaseB = 1.7;
    driftValue = 0.0f;

    currentDelaySamples = (float) (parameters.delayTimeMs * 0.001 * sampleRate);

    smoothedDelaySamples.setCurrentAndTargetValue (currentDelaySamples);
    smoothedFeedback.setCurrentAndTargetValue (parameters.feedback);
    smoothedDryWet.setCurrentAndTargetValue (parameters.dryWet);
    smoothedLowpassCoeff.setCurrentAndTargetValue (
        OnePoleFilter::coefficientFor (parameters.filterCutoffHz, sampleRate));

    samplesUntilNextGrain = 0.0;
}

void GrainEngine::setParameters (const Parameters& newParameters) noexcept
{
    parameters = newParameters;
}

void GrainEngine::setWindowShape (float alpha) noexcept
{
    windowAlpha = juce::jlimit (0.0f, 1.0f, alpha);
}

int GrainEngine::getNumActiveGrains() const noexcept
{
    int count = 0;

    for (const auto& grain : grains)
        if (grain.active)
            ++count;

    return count;
}

//==============================================================================
float GrainEngine::lookupHann (double phase) const noexcept
{
    const auto position = juce::jlimit (0.0, 1.0, phase) * (double) hannTableSize;
    const auto index = (size_t) position;
    const auto fraction = (float) (position - (double) index);

    const auto a = hannTable[index];
    const auto b = hannTable[juce::jmin (index + 1, (size_t) hannTableSize)];

    return a + fraction * (b - a);
}

float GrainEngine::windowValue (double normalisedAge) const noexcept
{
    // Tukey window: a flat top with Hann-shaped tapers of `alpha/2` on each side.
    // alpha == 1 collapses to a plain Hann window.
    const auto taper = 0.5 * (double) windowAlpha;

    if (taper <= 1.0e-6)
        return 1.0f;

    if (normalisedAge < taper)
        return lookupHann (0.5 * (normalisedAge / taper));

    if (normalisedAge > 1.0 - taper)
        return lookupHann (0.5 + 0.5 * ((normalisedAge - (1.0 - taper)) / taper));

    return 1.0f;
}

float GrainEngine::nextBipolarRandom() noexcept
{
    return random.nextFloat() * 2.0f - 1.0f;
}

float GrainEngine::nextInterval() noexcept
{
    // Unison appears twice in every set on purpose: roughly half the grains stay
    // put and hold the body of the sound while the rest form the halo above it.
    switch (parameters.intervals)
    {
        case IntervalSet::octaveUp:
        {
            static constexpr float set[] { 0.0f, 0.0f, 12.0f };
            return set[random.nextInt (3)];
        }
        case IntervalSet::fifthOctave:
        {
            static constexpr float set[] { 0.0f, 0.0f, 7.0f, 12.0f };
            return set[random.nextInt (4)];
        }
        case IntervalSet::shimmer:
        {
            static constexpr float set[] { 0.0f, 0.0f, 12.0f, 19.0f };
            return set[random.nextInt (4)];
        }
        case IntervalSet::octaveDown:
        {
            static constexpr float set[] { 0.0f, 0.0f, -12.0f };
            return set[random.nextInt (3)];
        }
        case IntervalSet::free:
        default:
            return 0.0f;
    }
}

void GrainEngine::advanceDrift (int numSamples) noexcept
{
    const auto step = (double) numSamples / sampleRate;

    driftPhaseA += juce::MathConstants<double>::twoPi * driftRateA * step;
    driftPhaseB += juce::MathConstants<double>::twoPi * driftRateB * step;

    if (driftPhaseA > juce::MathConstants<double>::twoPi) driftPhaseA -= juce::MathConstants<double>::twoPi;
    if (driftPhaseB > juce::MathConstants<double>::twoPi) driftPhaseB -= juce::MathConstants<double>::twoPi;

    driftValue = (float) (0.6 * std::sin (driftPhaseA) + 0.4 * std::sin (driftPhaseB));
}

//==============================================================================
void GrainEngine::scheduleNextGrain() noexcept
{
    const auto density = juce::jlimit (0.1f, 200.0f, parameters.densityHz);
    const auto interval = sampleRate / (double) density;
    const auto jitterAmount = juce::jlimit (0.0f, 1.0f, parameters.jitter);
    const auto jittered = interval * (1.0 + 0.9 * (double) jitterAmount * (double) nextBipolarRandom());

    samplesUntilNextGrain = juce::jmax (1.0, jittered);
}

void GrainEngine::triggerGrain() noexcept
{
    // Pool exhausted: drop the grain. Never allocate on the audio thread - a dropped
    // grain is inaudible, a malloc in the render callback is a dropout.
    Grain* slot = nullptr;

    for (auto& grain : grains)
    {
        if (! grain.active)
        {
            slot = &grain;
            break;
        }
    }

    if (slot == nullptr)
        return;

    const auto bufferSize = (double) delay.getNumSamples();

    if (bufferSize <= 0.0)
        return;

    const auto semitones = parameters.pitchSemitones
                           + nextInterval()
                           + nextBipolarRandom() * juce::jmax (0.0f, parameters.pitchSpraySemitones);
    const auto ratio = std::pow (2.0, (double) juce::jlimit (-36.0f, 36.0f, semitones) / 12.0);

    const auto grainMs = juce::jlimit (1.0f, 2000.0f, parameters.grainSizeMs);
    auto length = (int) std::round ((double) grainMs * 0.001 * sampleRate);
    length = juce::jmax (minGrainSamples, length);

    // The grain must not scan further than the buffer holds.
    const auto span = (double) length * ratio;

    if (span >= bufferSize - 4.0)
        length = juce::jmax (minGrainSamples, (int) ((bufferSize - 8.0) / ratio));

    const auto sprayMs = nextBipolarRandom() * juce::jmax (0.0f, parameters.positionSprayMs);
    const auto driftMs = (double) driftValue * (double) juce::jlimit (0.0f, 1.0f, parameters.drift) * maxDriftMs;
    auto offsetSamples = (double) currentDelaySamples + ((double) sprayMs + driftMs) * 0.001 * sampleRate;
    offsetSamples = juce::jlimit (1.0, bufferSize - (double) length * ratio - 4.0, offsetSamples);

    const auto start = delay.wrap ((double) delay.getWritePosition() - offsetSamples);
    const auto playReversed = random.nextFloat() < juce::jlimit (0.0f, 1.0f, parameters.reverseProbability);

    slot->active = true;
    slot->age = 0;
    slot->length = length;
    slot->readPosition = playReversed ? delay.wrap (start + (double) length * ratio) : start;
    slot->increment = playReversed ? -ratio : ratio;

    // Keep perceived loudness roughly constant as density and grain size change:
    // `overlap` is how many grains are sounding on average at any moment.
    const auto overlap = juce::jmax (1.0, (double) parameters.densityHz * (double) grainMs * 0.001);
    slot->gain = (float) (1.0 / std::sqrt (overlap));

    // Constant-power pan, drawn once and then fixed: a grain that moved while it
    // played would smear rather than occupy a place. At width 0 both weights are
    // 1/sqrt(2) and the grain sits dead centre.
    const auto width = juce::jlimit (0.0f, 1.0f, parameters.stereoWidth);
    const auto panPosition = nextBipolarRandom() * width;
    const auto angle = (panPosition * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    slot->panLeft = std::cos (angle);
    slot->panRight = std::sin (angle);

    // Panning alone is not enough: with several grains overlapping, random pan
    // positions average out and both channels converge on the same signal. Giving
    // the two channels different read points makes them genuinely different audio,
    // which is what actually widens the cloud.
    slot->channelOffset = (double) nextBipolarRandom() * (double) width * maxChannelOffsetMs * 0.001 * sampleRate;

    // Hand the editor everything it needs to draw this grain. Non-blocking: if the
    // queue is full the event is dropped rather than the audio thread stalling.
    eventQueue.push ({ panPosition, (float) semitones, (float) length * 1000.0f / (float) sampleRate, playReversed });
}

//==============================================================================
void GrainEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (buffer.getNumChannels(), activeChannels);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Coefficients that need a transcendental function are computed once per block;
    // the smoother then interpolates them per sample, so there is still no zipper noise.
    smoothedDelaySamples.setTargetValue ((float) (juce::jmax (0.0f, parameters.delayTimeMs) * 0.001 * sampleRate));
    smoothedFeedback.setTargetValue (juce::jlimit (0.0f, 1.2f, parameters.feedback));
    smoothedDryWet.setTargetValue (juce::jlimit (0.0f, 1.0f, parameters.dryWet));
    smoothedLowpassCoeff.setTargetValue (OnePoleFilter::coefficientFor (parameters.filterCutoffHz, sampleRate));

    const auto frozen = parameters.freeze;
    const auto diffusionAmount = juce::jlimit (0.0f, 1.0f, parameters.diffusion);

    // sqrt(2): at width 0 both pan weights are 1/sqrt(2), and multiplying them back
    // up keeps a centred grain at exactly the level it had before panning existed.
    constexpr auto panNormalisation = juce::MathConstants<float>::sqrt2;

    advanceDrift (numSamples);

    for (int n = 0; n < numSamples; ++n)
    {
        currentDelaySamples = smoothedDelaySamples.getNextValue();
        const auto feedbackAmount = smoothedFeedback.getNextValue();
        const auto mix = smoothedDryWet.getNextValue();
        const auto lowpassCoeff = smoothedLowpassCoeff.getNextValue();

        // -- scheduler -------------------------------------------------------
        samplesUntilNextGrain -= 1.0;

        if (samplesUntilNextGrain <= 0.0)
        {
            triggerGrain();
            scheduleNextGrain();
        }

        // -- grain pool ------------------------------------------------------
        // Stack array, fixed size, no heap involved.
        float wet[maxChannels] = { 0.0f, 0.0f };

        for (auto& grain : grains)
        {
            if (! grain.active)
                continue;

            const auto envelope = windowValue ((double) grain.age / (double) grain.length) * grain.gain;
            const auto readPoint = delay.makeReadPoint (grain.readPosition);

            // Each channel reads at its own point, so the two sides carry different
            // audio rather than the same signal at two levels. At width 0 the offset
            // and the pan both collapse and this is exactly a plain per-channel read.
            const auto scale = envelope * panNormalisation;

            wet[0] += delay.read (0, readPoint) * scale * grain.panLeft;

            if (numChannels > 1)
            {
                const auto rightPoint = grain.channelOffset != 0.0
                                          ? delay.makeReadPoint (grain.readPosition + grain.channelOffset)
                                          : readPoint;
                wet[1] += delay.read (1, rightPoint) * scale * grain.panRight;
            }

            grain.readPosition = delay.wrap (grain.readPosition + grain.increment);

            if (++grain.age >= grain.length)
                grain.active = false;
        }

        // -- feedback path and output ----------------------------------------
        // Diffusion runs on the wet signal itself, so it is part of what you hear -
        // not only of what gets re-injected. Because the diffused signal is also
        // what feeds back, each repeat smears a little further than the last. The
        // two channels use different allpass lengths, so the wash is wide.
        for (int ch = 0; ch < numChannels; ++ch)
            wet[ch] = diffusers[(size_t) ch].process (wet[ch], diffusionAmount);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            const auto dry = channelData[n];

            auto fed = feedbackLowpass[(size_t) ch].processLowpass (wet[ch], lowpassCoeff);
            fed = feedbackDcBlocker[(size_t) ch].processHighpass (fed, dcBlockerCoeff);
            fed = softClip (fed * feedbackAmount);

            if (! frozen)
                delay.writeFrame (ch, dry + fed);

            channelData[n] = dry * (1.0f - mix) + wet[ch] * mix;
        }

        // Freeze holds the write head still, so the grains keep scanning the same
        // four seconds of audio for ever.
        if (! frozen)
            delay.advanceWrite();
    }
}

} // namespace krain
