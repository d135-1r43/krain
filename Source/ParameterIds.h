#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace krain::params
{

// `inline constexpr` at namespace scope: one shared, compile-time constant across
// all translation units. The C++ way of saying `public static final String`.
inline constexpr const char* delayTime = "delayTime";
inline constexpr const char* syncEnabled = "syncEnabled";
inline constexpr const char* syncDivision = "syncDivision";
inline constexpr const char* grainSize = "grainSize";
inline constexpr const char* density = "density";
inline constexpr const char* jitter = "jitter";
inline constexpr const char* pitch = "pitch";
inline constexpr const char* pitchSpray = "pitchSpray";
inline constexpr const char* intervals = "intervals";
inline constexpr const char* positionSpray = "positionSpray";
inline constexpr const char* reverseProbability = "reverseProbability";
inline constexpr const char* feedback = "feedback";
inline constexpr const char* filterCutoff = "filterCutoff";
inline constexpr const char* dryWet = "dryWet";
inline constexpr const char* stereoWidth = "stereoWidth";
inline constexpr const char* diffusion = "diffusion";
inline constexpr const char* drift = "drift";
inline constexpr const char* freeze = "freeze";

/** Note divisions for the tempo-synced delay time, as multiples of a quarter note. */
struct Division
{
    const char* name;
    double quarterNotes;
};

inline constexpr std::array<Division, 11> divisions { {
    { "1/32", 0.125 },
    { "1/16T", 1.0 / 6.0 },
    { "1/16", 0.25 },
    { "1/8T", 1.0 / 3.0 },
    { "1/16D", 0.375 },
    { "1/8", 0.5 },
    { "1/4T", 2.0 / 3.0 },
    { "1/8D", 0.75 },
    { "1/4", 1.0 },
    { "1/4D", 1.5 },
    { "1/2", 2.0 },
} };

/** Interval sets a grain can pick its transposition from. Order must match
    krain::IntervalSet. */
inline juce::StringArray intervalNames()
{
    return { "Free", "Octave Up", "Fifth + Octave", "Shimmer", "Octave Down" };
}

inline juce::StringArray divisionNames()
{
    juce::StringArray names;

    for (const auto& division : divisions)
        names.add (division.name);

    return names;
}

} // namespace krain::params
