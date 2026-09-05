#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/DelayBuffer.h"
#include "dsp/GrainEngine.h"
#include "WavWriter.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <string>
#include <vector>

//==============================================================================
// Allocation counter
//
// The single hardest requirement on an audio plug-in is that the render callback
// never allocates: malloc can take a lock and block for milliseconds, and a blocked
// audio thread is an audible dropout. Overriding the global operator new lets the
// test *prove* the rule holds instead of just asserting it in a comment.
//
// C++ note (Java dev): `operator new` is a plain, replaceable global function. If a
// program defines its own, the linker uses that one everywhere. There is no
// equivalent hook in the JVM.
namespace
{
std::atomic<int> allocationCount { 0 };
std::atomic<bool> countingAllocations { false };

inline void noteAllocation() noexcept
{
    if (countingAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);
}

/** RAII guard: counts allocations for as long as it is in scope. */
struct AllocationGuard
{
    AllocationGuard() noexcept
    {
        allocationCount.store (0, std::memory_order_relaxed);
        countingAllocations.store (true, std::memory_order_relaxed);
    }

    ~AllocationGuard() noexcept { countingAllocations.store (false, std::memory_order_relaxed); }

    int count() const noexcept { return allocationCount.load (std::memory_order_relaxed); }
};
} // namespace

void* operator new (std::size_t size)
{
    noteAllocation();

    if (auto* pointer = std::malloc (size == 0 ? 1 : size))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    noteAllocation();

    if (auto* pointer = std::aligned_alloc ((std::size_t) alignment,
                                            ((size + (std::size_t) alignment - 1) / (std::size_t) alignment)
                                                * (std::size_t) alignment))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void operator delete (void* pointer) noexcept { std::free (pointer); }
void operator delete[] (void* pointer) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::align_val_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::align_val_t) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t, std::align_val_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t, std::align_val_t) noexcept { std::free (pointer); }

//==============================================================================
namespace
{
constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 512;

/** Renders `seconds` of audio and reports what came out. */
struct RenderResult
{
    bool allFinite = true;
    float peak = 0.0f;
    double rms = 0.0;
    int allocations = 0;
};

/** Writes a source/processed pair plus a JSON sidecar into the renders directory.

    This is the bit that turns a green test into something you can actually listen
    to. A passing assertion tells you the output is finite; only your ears tell you
    the grains are not clicking.
*/
void capture (const std::string& id,
              const std::string& title,
              const std::vector<std::vector<float>>& source,
              const std::vector<std::vector<float>>& processed)
{
    const std::string directory = GRAINDELAY_RENDER_DIR;

    std::error_code errorCode;
    std::filesystem::create_directories (directory, errorCode);

    const auto numSamples = (int) source[0].size();
    const float* sourcePointers[2] { source[0].data(), source[1].data() };
    const float* processedPointers[2] { processed[0].data(), processed[1].data() };

    graindelay::wav::writeFloat32 (directory + "/" + id + "--source.wav", sourcePointers, 2, numSamples, testSampleRate);
    graindelay::wav::writeFloat32 (directory + "/" + id + "--processed.wav", processedPointers, 2, numSamples, testSampleRate);

    graindelay::wav::writeSidecar (directory + "/" + id + ".json",
                                   "{\n  \"preset\": \"" + id + "\",\n"
                                   "  \"presetDescription\": \"" + title + "\",\n"
                                   "  \"source\": \"test\",\n"
                                   "  \"sourceDescription\": \"Written by the Catch2 suite\",\n"
                                   "  \"origin\": \"test\"\n}\n");
}

/** Feeds `input` (a callback producing one sample) through the engine.

    The impulse / noise-burst / silence cases below are all just different callbacks.
*/
template <typename SampleProvider>
RenderResult render (graindelay::GrainEngine& engine,
                     double seconds,
                     SampleProvider&& provider,
                     const std::string& captureId = {},
                     const std::string& captureTitle = {})
{
    const auto totalSamples = (int) (seconds * testSampleRate);
    const auto numBlocks = juce::jmax (1, totalSamples / testBlockSize);

    juce::AudioBuffer<float> buffer (2, testBlockSize);

    RenderResult result;
    double sumOfSquares = 0.0;
    int sampleIndex = 0;

    const auto capturing = ! captureId.empty();
    std::vector<std::vector<float>> capturedSource, capturedProcessed;

    if (capturing)
    {
        capturedSource.assign (2, {});
        capturedProcessed.assign (2, {});

        for (int ch = 0; ch < 2; ++ch)
        {
            capturedSource[(size_t) ch].reserve ((size_t) (numBlocks * testBlockSize));
            capturedProcessed[(size_t) ch].reserve ((size_t) (numBlocks * testBlockSize));
        }
    }

    for (int block = 0; block < numBlocks; ++block)
    {
        for (int n = 0; n < testBlockSize; ++n, ++sampleIndex)
        {
            const auto value = provider (sampleIndex);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, n, value);
        }

        if (capturing)
            for (int ch = 0; ch < 2; ++ch)
                capturedSource[(size_t) ch].insert (capturedSource[(size_t) ch].end(),
                                                    buffer.getReadPointer (ch),
                                                    buffer.getReadPointer (ch) + testBlockSize);

        {
            // Everything inside this scope runs on what would be the audio thread.
            const AllocationGuard guard;
            engine.process (buffer);
            result.allocations += guard.count();
        }

        if (capturing)
            for (int ch = 0; ch < 2; ++ch)
                capturedProcessed[(size_t) ch].insert (capturedProcessed[(size_t) ch].end(),
                                                       buffer.getReadPointer (ch),
                                                       buffer.getReadPointer (ch) + testBlockSize);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            for (int n = 0; n < testBlockSize; ++n)
            {
                const auto value = buffer.getSample (ch, n);

                if (! std::isfinite (value))
                    result.allFinite = false;

                result.peak = juce::jmax (result.peak, std::abs (value));
                sumOfSquares += (double) value * (double) value;
            }
        }
    }

    result.rms = std::sqrt (sumOfSquares / juce::jmax (1.0, (double) numBlocks * testBlockSize * 2.0));

    if (capturing)
        capture (captureId, captureTitle, capturedSource, capturedProcessed);

    return result;
}

graindelay::GrainEngine::Parameters defaultParameters()
{
    graindelay::GrainEngine::Parameters p;
    p.delayTimeMs = 250.0f;
    p.grainSizeMs = 100.0f;
    p.densityHz = 25.0f;
    p.jitter = 0.3f;
    p.feedback = 0.4f;
    p.dryWet = 1.0f;
    p.filterCutoffHz = 8000.0f;

    return p;
}
} // namespace

//==============================================================================
TEST_CASE ("DelayBuffer holds at least four seconds", "[delaybuffer]")
{
    graindelay::DelayBuffer buffer;

    SECTION ("at 44.1 kHz it is still sized for 4 s at 96 kHz")
    {
        buffer.prepare (2, 44100.0);
        REQUIRE (buffer.getNumSamples() >= (int) (4.0 * 96000.0));
    }

    SECTION ("at 192 kHz it grows with the sample rate")
    {
        buffer.prepare (2, 192000.0);
        REQUIRE (buffer.getNumSamples() >= (int) (4.0 * 192000.0));
    }
}

TEST_CASE ("DelayBuffer reads back what was written", "[delaybuffer]")
{
    graindelay::DelayBuffer buffer;
    buffer.prepare (2, testSampleRate);

    for (int n = 0; n < 1000; ++n)
    {
        buffer.writeFrame (0, (float) n);
        buffer.writeFrame (1, (float) -n);
        buffer.advanceWrite();
    }

    REQUIRE (buffer.getWritePosition() == 1000);

    SECTION ("integer positions are exact")
    {
        REQUIRE (buffer.readInterpolated (0, 500.0) == 500.0f);
        REQUIRE (buffer.readInterpolated (1, 500.0) == -500.0f);
    }

    SECTION ("fractional positions interpolate linearly")
    {
        REQUIRE_THAT (buffer.readInterpolated (0, 500.25),
                      Catch::Matchers::WithinAbs (500.25, 1.0e-4));
    }

    SECTION ("negative and oversized indices wrap into range")
    {
        const auto size = buffer.getNumSamples();
        REQUIRE (buffer.readInterpolated (0, -1.0) == buffer.readInterpolated (0, size - 1.0));
        REQUIRE (buffer.readInterpolated (0, size + 42.0) == buffer.readInterpolated (0, 42.0));
    }
}

//==============================================================================
TEST_CASE ("GrainEngine renders an impulse without NaN or Inf", "[grainengine]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2);
    engine.setParameters (defaultParameters());

    const auto result = render (engine, 5.0, [] (int index) { return index == 0 ? 1.0f : 0.0f; },
                                "test-impulse", "Single impulse, default parameters");

    REQUIRE (result.allFinite);
    REQUIRE (result.peak > 0.0f);        // the impulse actually came back out
    REQUIRE (result.peak < 10.0f);       // ...and did not explode
}

TEST_CASE ("GrainEngine stays silent for silent input", "[grainengine]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2);
    engine.setParameters (defaultParameters());

    const auto result = render (engine, 2.0, [] (int) { return 0.0f; });

    REQUIRE (result.allFinite);
    REQUIRE (result.peak == 0.0f);
}

TEST_CASE ("GrainEngine survives 30 s at feedback 0.95", "[grainengine][stability]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2);

    auto parameters = defaultParameters();
    parameters.feedback = 0.95f;
    parameters.densityHz = 40.0f;
    parameters.pitchSpraySemitones = 5.0f;
    parameters.positionSprayMs = 120.0f;
    parameters.reverseProbability = 0.35f;
    engine.setParameters (parameters);

    juce::Random random (0x51ED);

    // A one-second burst of noise, then silence: whatever is still ringing after
    // that is pure feedback, which is exactly what we want to watch.
    const auto result = render (engine, 30.0, [&random] (int index)
    {
        return index < (int) testSampleRate ? random.nextFloat() * 2.0f - 1.0f : 0.0f;
    },
    "test-feedback-095", "Noise burst then 29 s of tail at feedback 0.95");

    REQUIRE (result.allFinite);

    // The tanh soft clip is the thing that guarantees this bound. Without it, a
    // feedback of 0.95 combined with overlapping grains would keep growing.
    REQUIRE (result.peak < 4.0f);
}

TEST_CASE ("GrainEngine decays once the input stops", "[grainengine][stability]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2);

    auto parameters = defaultParameters();
    parameters.feedback = 0.5f;
    engine.setParameters (parameters);

    const auto burst = [] (int index) { return index < 24000 ? 0.5f : 0.0f; };

    // Render the burst and the first few seconds of tail...
    render (engine, 4.0, burst);

    // ...then keep going with silence and check the tail has actually died away.
    const auto tail = render (engine, 20.0, [] (int) { return 0.0f; },
                              "test-decay", "Tail after the input stops, feedback 0.5");

    REQUIRE (tail.allFinite);
    REQUIRE (tail.peak < 0.05f);
}

TEST_CASE ("Freeze keeps the texture going after the input stops", "[grainengine][freeze]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2);

    auto parameters = defaultParameters();
    parameters.feedback = 0.0f;
    engine.setParameters (parameters);

    juce::Random random (0xF00D);
    render (engine, 2.0, [&random] (int) { return random.nextFloat() * 2.0f - 1.0f; });

    parameters.freeze = true;
    engine.setParameters (parameters);

    const auto frozen = render (engine, 5.0, [] (int) { return 0.0f; },
                                "test-freeze", "Freeze engaged, input silent");

    REQUIRE (frozen.allFinite);
    REQUIRE (frozen.rms > 0.001);
}

TEST_CASE ("Extreme parameter settings stay finite", "[grainengine]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2);

    graindelay::GrainEngine::Parameters parameters;
    parameters.delayTimeMs = 4000.0f;
    parameters.grainSizeMs = 1000.0f;
    parameters.densityHz = 100.0f;
    parameters.jitter = 1.0f;
    parameters.pitchSemitones = 24.0f;
    parameters.pitchSpraySemitones = 12.0f;
    parameters.positionSprayMs = 500.0f;
    parameters.reverseProbability = 1.0f;
    parameters.feedback = 1.2f;
    parameters.filterCutoffHz = 20000.0f;
    parameters.dryWet = 1.0f;
    engine.setParameters (parameters);

    juce::Random random (0xBEEF);
    const auto result = render (engine, 10.0, [&random] (int) { return random.nextFloat() * 2.0f - 1.0f; },
                                "test-extremes-max", "Every parameter at its maximum");

    REQUIRE (result.allFinite);
    REQUIRE (result.peak < 10.0f);

    SECTION ("and so does the opposite extreme")
    {
        engine.reset();

        parameters.delayTimeMs = 1.0f;
        parameters.grainSizeMs = 5.0f;
        parameters.densityHz = 0.5f;
        parameters.pitchSemitones = -24.0f;
        parameters.filterCutoffHz = 20.0f;
        engine.setParameters (parameters);

        const auto extreme = render (engine, 10.0, [&random] (int) { return random.nextFloat() * 2.0f - 1.0f; });

        REQUIRE (extreme.allFinite);
        REQUIRE (extreme.peak < 10.0f);
    }
}

TEST_CASE ("The grain pool never overflows", "[grainengine]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2);

    auto parameters = defaultParameters();
    parameters.densityHz = 100.0f;   // maximum
    parameters.grainSizeMs = 1000.0f; // maximum -> ~100 grains would want to overlap
    parameters.jitter = 0.0f;
    engine.setParameters (parameters);

    juce::Random random (0xC0FFEE);
    juce::AudioBuffer<float> buffer (2, testBlockSize);

    int highWaterMark = 0;

    for (int block = 0; block < 400; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < testBlockSize; ++n)
                buffer.setSample (ch, n, random.nextFloat() * 2.0f - 1.0f);

        engine.process (buffer);
        highWaterMark = juce::jmax (highWaterMark, engine.getNumActiveGrains());
    }

    // Requesting more overlap than the pool holds must drop grains, not allocate.
    REQUIRE (highWaterMark <= graindelay::GrainEngine::maxGrains);
    REQUIRE (highWaterMark > 0);
}

//==============================================================================
TEST_CASE ("the allocation counter actually counts", "[realtime][meta]")
{
    // Positive control. Without this, "process() never allocates" below would also
    // pass if the operator new replacement were silently not linked in.
    graindelay::GrainEngine engine;

    const AllocationGuard guard;
    engine.prepare (testSampleRate, 2);

    REQUIRE (guard.count() > 0);
}

TEST_CASE ("process() never allocates", "[grainengine][realtime]")
{
    graindelay::GrainEngine engine;
    engine.prepare (testSampleRate, 2); // this one is *allowed* to allocate

    auto parameters = defaultParameters();
    parameters.feedback = 0.8f;
    parameters.reverseProbability = 0.5f;
    parameters.pitchSpraySemitones = 7.0f;
    parameters.positionSprayMs = 200.0f;
    engine.setParameters (parameters);

    juce::Random random (0xA11C);
    juce::AudioBuffer<float> buffer (2, testBlockSize);

    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < testBlockSize; ++n)
            buffer.setSample (ch, n, random.nextFloat() * 2.0f - 1.0f);

    // Warm up outside the guard so that any one-off lazy initialisation elsewhere
    // does not get blamed on the render loop.
    engine.process (buffer);

    int allocations = 0;

    for (int block = 0; block < 200; ++block)
    {
        const AllocationGuard guard;
        engine.setParameters (parameters);
        engine.process (buffer);
        allocations += guard.count();
    }

    REQUIRE (allocations == 0);
}
