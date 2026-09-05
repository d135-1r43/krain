//==============================================================================
// krain-render
//
// Renders test signals through the grain engine and drops source/processed WAV
// pairs into the renders directory, where the compare app picks them up.
//
// This is the "run it and listen" half of the test story. The Catch2 suite proves
// the engine stays finite; this proves nothing at all, and is far more useful for
// deciding whether a change actually improved the sound.
//==============================================================================

#include "dsp/GrainEngine.h"
#include "WavWriter.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
using Parameters = krain::GrainEngine::Parameters;

//==============================================================================
// A granular delay is normally used on an aux send, not as an insert - so every
// preset renders 100 % wet. What comes out of these files is the return signal on
// its own; the compare app does the mixing, the way a console would.
struct Preset
{
    std::string name;
    std::string description;
    Parameters parameters;
    double freezeAfterSeconds = -1.0;
};

std::vector<Preset> makePresets()
{
    std::vector<Preset> presets;

    {
        Parameters p;
        p.delayTimeMs = 350.0f;
        p.grainSizeMs = 120.0f;
        p.densityHz = 20.0f;
        p.feedback = 0.4f;
        p.dryWet = 1.0f;
        presets.push_back ({ "default", "The shipping defaults", p, -1.0 });
    }
    {
        Parameters p;
        p.delayTimeMs = 400.0f;
        p.grainSizeMs = 150.0f;
        p.densityHz = 40.0f;
        p.jitter = 0.3f;
        p.pitchSemitones = 12.0f;
        p.pitchSpraySemitones = 0.15f;
        p.feedback = 0.7f;
        p.filterCutoffHz = 6000.0f;
        p.dryWet = 1.0f;
        presets.push_back ({ "shimmer", "Octave-up feedback, the classic shimmer", p, -1.0 });
    }
    {
        Parameters p;
        p.delayTimeMs = 600.0f;
        p.grainSizeMs = 400.0f;
        p.densityHz = 12.0f;
        p.jitter = 0.8f;
        p.pitchSpraySemitones = 5.0f;
        p.positionSprayMs = 300.0f;
        p.feedback = 0.5f;
        p.filterCutoffHz = 4000.0f;
        p.dryWet = 1.0f;
        presets.push_back ({ "cloud", "Long, sprayed grains - smeared texture", p, -1.0 });
    }
    {
        Parameters p;
        p.delayTimeMs = 500.0f;
        p.grainSizeMs = 250.0f;
        p.densityHz = 18.0f;
        p.pitchSemitones = -5.0f;
        p.reverseProbability = 1.0f;
        p.feedback = 0.55f;
        p.dryWet = 1.0f;
        presets.push_back ({ "reverse", "Every grain plays backwards, pitched down", p, -1.0 });
    }
    {
        Parameters p;
        p.delayTimeMs = 120.0f;
        p.grainSizeMs = 25.0f;
        p.densityHz = 60.0f;
        p.jitter = 0.05f;
        p.feedback = 0.6f;
        p.filterCutoffHz = 9000.0f;
        p.dryWet = 1.0f;
        presets.push_back ({ "stutter", "Tiny dense grains - buffer-stutter effect", p, -1.0 });
    }
    {
        Parameters p;
        p.delayTimeMs = 500.0f;
        p.grainSizeMs = 300.0f;
        p.densityHz = 25.0f;
        p.jitter = 0.5f;
        p.pitchSpraySemitones = 0.1f;
        p.positionSprayMs = 200.0f;
        p.feedback = 0.0f;
        p.dryWet = 1.0f;
        presets.push_back ({ "freeze", "Freeze engages after 2 s - input then stops", p, 2.0 });
    }

    return presets;
}

//==============================================================================
struct SourceSignal
{
    std::string name;
    std::string description;
    double seconds;
};

const std::vector<SourceSignal>& builtInSources()
{
    static const std::vector<SourceSignal> sources {
        { "impulse", "One click, then silence - shows the grain structure bare", 6.0 },
        { "clicks", "A click every 500 ms - makes grain timing audible", 8.0 },
        { "chord", "Detuned triad for 2 s, then silence - reveals pitch artefacts", 9.0 },
        { "noise", "One second of noise - worst case for the feedback path", 10.0 },
        { "sweep", "Log sine sweep 40 Hz to 12 kHz", 8.0 },
    };

    return sources;
}

float generateSample (const std::string& source, int index, double sampleRate, double totalSeconds)
{
    const auto t = (double) index / sampleRate;

    if (source == "impulse")
        return index == 0 ? 1.0f : 0.0f;

    if (source == "clicks")
    {
        const auto period = (int) (0.5 * sampleRate);
        const auto phase = index % period;
        // A couple of samples wide, so it is a click rather than a DC step.
        return phase < 3 ? (phase == 0 ? 0.9f : -0.45f) : 0.0f;
    }

    if (source == "chord")
    {
        if (t > 2.0)
            return 0.0f;

        const auto envelope = std::min (1.0, t * 40.0) * std::min (1.0, (2.0 - t) * 8.0);
        auto value = 0.0;

        for (const auto frequency : { 220.0, 277.18, 329.63 })
            value += std::sin (2.0 * M_PI * frequency * t);

        return (float) (value * envelope * 0.22);
    }

    if (source == "noise")
    {
        if (t > 1.0)
            return 0.0f;

        // Deterministic LCG: the same command always renders the same file, which is
        // what makes an A/B against yesterday's render meaningful.
        static uint32_t state = 22695477u;
        state = state * 1103515245u + 12345u;

        return ((float) (state >> 8) / (float) 0x7fffff - 1.0f) * 0.5f;
    }

    if (source == "sweep")
    {
        const auto fraction = t / totalSeconds;
        const auto startHz = 40.0;
        const auto endHz = 12000.0;
        const auto k = std::log (endHz / startHz);
        const auto phase = 2.0 * M_PI * startHz * totalSeconds / k * (std::exp (fraction * k) - 1.0);
        const auto fade = std::min (1.0, std::min (t * 20.0, (totalSeconds - t) * 20.0));

        return (float) (std::sin (phase) * 0.4 * fade);
    }

    return 0.0f;
}

//==============================================================================
std::string toJson (const Preset& preset, const std::string& sourceName, const std::string& sourceDescription)
{
    const auto& p = preset.parameters;

    const auto number = [] (float value)
    {
        char text[32];
        std::snprintf (text, sizeof (text), "%.4g", (double) value);
        return std::string (text);
    };

    std::string json = "{\n";
    json += "  \"preset\": \"" + preset.name + "\",\n";
    json += "  \"presetDescription\": \"" + preset.description + "\",\n";
    json += "  \"source\": \"" + sourceName + "\",\n";
    json += "  \"sourceDescription\": \"" + sourceDescription + "\",\n";
    json += "  \"origin\": \"render\",\n";
    json += "  \"params\": {\n";
    json += "    \"delayTimeMs\": " + number (p.delayTimeMs) + ",\n";
    json += "    \"grainSizeMs\": " + number (p.grainSizeMs) + ",\n";
    json += "    \"densityHz\": " + number (p.densityHz) + ",\n";
    json += "    \"jitter\": " + number (p.jitter) + ",\n";
    json += "    \"pitchSemitones\": " + number (p.pitchSemitones) + ",\n";
    json += "    \"pitchSpraySemitones\": " + number (p.pitchSpraySemitones) + ",\n";
    json += "    \"positionSprayMs\": " + number (p.positionSprayMs) + ",\n";
    json += "    \"reverseProbability\": " + number (p.reverseProbability) + ",\n";
    json += "    \"feedback\": " + number (p.feedback) + ",\n";
    json += "    \"filterCutoffHz\": " + number (p.filterCutoffHz) + ",\n";
    json += "    \"dryWet\": " + number (p.dryWet) + ",\n";
    json += "    \"freeze\": " + std::string (preset.freezeAfterSeconds >= 0.0 ? "\"after 2 s\"" : "false") + "\n";
    json += "  }\n";
    json += "}\n";

    return json;
}

//==============================================================================
/** Loads an arbitrary audio file as the source. This is the one place worth
    pulling juce_audio_formats in for - hand-rolling a decoder for whatever the user
    drops in would be silly. */
bool loadInputFile (const std::string& path, std::vector<std::vector<float>>& channels, double& sampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));

    if (reader == nullptr)
        return false;

    const auto numSamples = (int) juce::jmin ((juce::int64) (60 * 192000), reader->lengthInSamples);
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();
    reader->read (&buffer, 0, numSamples, 0, true, reader->numChannels > 1);

    sampleRate = reader->sampleRate;
    channels.assign (2, std::vector<float> ((size_t) numSamples, 0.0f));

    for (int ch = 0; ch < 2; ++ch)
        std::copy (buffer.getReadPointer (ch), buffer.getReadPointer (ch) + numSamples, channels[(size_t) ch].begin());

    return true;
}

//==============================================================================
void printUsage()
{
    std::printf ("krain-render - render test signals through the grain engine\n\n");
    std::printf ("Usage: krain-render [options]\n\n");
    std::printf ("  --out-dir DIR     where to write (default: the build renders directory)\n");
    std::printf ("  --source NAME     built-in test signal (default: chord)\n");
    std::printf ("  --input FILE      use an audio file as the source instead\n");
    std::printf ("  --preset NAME     render one preset only (default: all of them)\n");
    std::printf ("  --sample-rate N   default 48000\n");
    std::printf ("  --list            show the built-in sources and presets\n\n");

    std::printf ("Sources:\n");
    for (const auto& source : builtInSources())
        std::printf ("  %-10s %s\n", source.name.c_str(), source.description.c_str());

    std::printf ("\nPresets:\n");
    for (const auto& preset : makePresets())
        std::printf ("  %-10s %s\n", preset.name.c_str(), preset.description.c_str());
}
} // namespace

//==============================================================================
int main (int argc, char* argv[])
{
    std::string outDir = KRAIN_RENDER_DIR;
    std::string sourceName = "chord";
    std::string inputFile;
    std::string onlyPreset;
    double sampleRate = 48000.0;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        const auto next = [&] () -> std::string { return i + 1 < argc ? argv[++i] : std::string {}; };

        if (argument == "--out-dir") outDir = next();
        else if (argument == "--source") sourceName = next();
        else if (argument == "--input") inputFile = next();
        else if (argument == "--preset") onlyPreset = next();
        else if (argument == "--sample-rate") sampleRate = std::stod (next());
        else if (argument == "--list" || argument == "--help" || argument == "-h") { printUsage(); return 0; }
        else { std::printf ("Unknown option: %s\n\n", argument.c_str()); printUsage(); return 1; }
    }

    std::error_code errorCode;
    std::filesystem::create_directories (outDir, errorCode);

    // ---------------------------------------------------------------- source
    std::vector<std::vector<float>> source;
    std::string sourceDescription;

    if (! inputFile.empty())
    {
        if (! loadInputFile (inputFile, source, sampleRate))
        {
            std::printf ("Could not read '%s'\n", inputFile.c_str());
            return 1;
        }

        sourceName = std::filesystem::path (inputFile).stem().string();
        sourceDescription = "From " + inputFile;
    }
    else
    {
        const auto match = std::find_if (builtInSources().begin(), builtInSources().end(),
                                         [&] (const SourceSignal& s) { return s.name == sourceName; });

        if (match == builtInSources().end())
        {
            std::printf ("Unknown source '%s'\n\n", sourceName.c_str());
            printUsage();
            return 1;
        }

        sourceDescription = match->description;
        const auto numSamples = (size_t) (match->seconds * sampleRate);
        source.assign (2, std::vector<float> (numSamples, 0.0f));

        for (size_t n = 0; n < numSamples; ++n)
        {
            const auto value = generateSample (sourceName, (int) n, sampleRate, match->seconds);
            source[0][n] = value;
            source[1][n] = value;
        }
    }

    const auto numSamples = (int) source[0].size();

    // --------------------------------------------------------------- render
    auto rendered = 0;

    for (const auto& preset : makePresets())
    {
        if (! onlyPreset.empty() && preset.name != onlyPreset)
            continue;

        krain::GrainEngine engine;
        engine.prepare (sampleRate, 2);

        auto parameters = preset.parameters;
        engine.setParameters (parameters);

        constexpr int blockSize = 512;
        juce::AudioBuffer<float> block (2, blockSize);
        std::vector<std::vector<float>> processed (2, std::vector<float> ((size_t) numSamples, 0.0f));

        auto frozen = false;

        for (int position = 0; position < numSamples; position += blockSize)
        {
            const auto count = juce::jmin (blockSize, numSamples - position);

            if (! frozen && preset.freezeAfterSeconds >= 0.0
                && (double) position / sampleRate >= preset.freezeAfterSeconds)
            {
                parameters.freeze = true;
                engine.setParameters (parameters);
                frozen = true;
            }

            block.clear();

            for (int ch = 0; ch < 2; ++ch)
                std::copy (source[(size_t) ch].begin() + position,
                           source[(size_t) ch].begin() + position + count,
                           block.getWritePointer (ch));

            juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), 2, count);
            engine.process (view);

            for (int ch = 0; ch < 2; ++ch)
                std::copy (block.getReadPointer (ch), block.getReadPointer (ch) + count,
                           processed[(size_t) ch].begin() + position);
        }

        const auto id = sourceName + "--" + preset.name;
        const float* sourcePointers[2] { source[0].data(), source[1].data() };
        const float* processedPointers[2] { processed[0].data(), processed[1].data() };

        krain::wav::writeFloat32 (outDir + "/" + id + "--source.wav", sourcePointers, 2, numSamples, sampleRate);
        krain::wav::writeFloat32 (outDir + "/" + id + "--processed.wav", processedPointers, 2, numSamples, sampleRate);
        krain::wav::writeSidecar (outDir + "/" + id + ".json", toJson (preset, sourceName, sourceDescription));

        std::printf ("  %-28s %6.2f s\n", id.c_str(), (double) numSamples / sampleRate);
        ++rendered;
    }

    if (rendered == 0)
    {
        std::printf ("No preset matched '%s'\n", onlyPreset.c_str());
        return 1;
    }

    std::printf ("\n%d render(s) written to %s\n", rendered, outDir.c_str());
    std::printf ("Compare them with: tools/compare/serve.sh\n");

    return 0;
}
