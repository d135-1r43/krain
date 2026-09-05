#include "dsp/DelayBuffer.h"

#include <cmath>

namespace graindelay
{

namespace
{
constexpr double minCapacitySeconds = 4.0;
constexpr double referenceSampleRate = 96000.0;
} // namespace

void DelayBuffer::prepare (int numChannels, double sampleRate)
{
    const auto seconds = minCapacitySeconds;
    const auto samplesAtCurrentRate = seconds * juce::jmax (8000.0, sampleRate);
    const auto samplesAtReferenceRate = seconds * referenceSampleRate;

    const auto size = (int) std::ceil (juce::jmax (samplesAtCurrentRate, samplesAtReferenceRate));

    // setSize() is the allocating call - hence "prepare() allocates".
    buffer.setSize (juce::jmax (1, numChannels), size, false, true, true);
    reset();
}

void DelayBuffer::reset() noexcept
{
    buffer.clear();
    writePosition = 0;
}

void DelayBuffer::writeFrame (int channel, float value) noexcept
{
    jassert (juce::isPositiveAndBelow (channel, buffer.getNumChannels()));
    buffer.getWritePointer (channel)[writePosition] = value;
}

void DelayBuffer::advanceWrite() noexcept
{
    if (++writePosition >= buffer.getNumSamples())
        writePosition = 0;
}

double DelayBuffer::wrap (double index) const noexcept
{
    const auto size = (double) buffer.getNumSamples();

    if (size <= 0.0)
        return 0.0;

    // A grain moves by at most a couple of samples per step, so a branch beats fmod.
    // The while-loops are the belt-and-braces path for a freshly seeded position.
    while (index >= size)
        index -= size;

    while (index < 0.0)
        index += size;

    return index;
}

DelayBuffer::ReadPoint DelayBuffer::makeReadPoint (double index) const noexcept
{
    const auto size = buffer.getNumSamples();

    if (size <= 0)
        return {};

    const auto wrapped = wrap (index);
    const auto i0 = (int) wrapped;

    ReadPoint point;
    point.index0 = juce::jlimit (0, size - 1, i0);
    point.index1 = point.index0 + 1 >= size ? 0 : point.index0 + 1;
    point.fraction = (float) (wrapped - (double) i0);

    return point;
}

float DelayBuffer::read (int channel, ReadPoint point) const noexcept
{
    jassert (juce::isPositiveAndBelow (channel, buffer.getNumChannels()));

    const auto* data = buffer.getReadPointer (channel);
    const auto a = data[point.index0];
    const auto b = data[point.index1];

    return a + point.fraction * (b - a);
}

float DelayBuffer::readInterpolated (int channel, double index) const noexcept
{
    return read (channel, makeReadPoint (index));
}

} // namespace graindelay
