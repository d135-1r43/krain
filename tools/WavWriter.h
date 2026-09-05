#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace graindelay::wav
{

//==============================================================================
/** A minimal 32-bit-float WAV writer.

    Deliberately dependency-free: the unit tests link this, and pulling
    juce_audio_formats into the test target just to save a file would slow down the
    fastest feedback loop in the project for no benefit.

    Float samples rather than 16-bit PCM on purpose - the plug-in can legitimately
    push past 0 dBFS, and when you are listening for exactly that you do not want
    the file format to clip it away first.
*/
namespace detail
{
inline void appendBytes (std::vector<uint8_t>& out, const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*> (data);
    out.insert (out.end(), bytes, bytes + size);
}

inline void appendTag (std::vector<uint8_t>& out, const char* tag)
{
    appendBytes (out, tag, 4);
}

inline void appendU32 (std::vector<uint8_t>& out, uint32_t value)
{
    const uint8_t bytes[4] { (uint8_t) (value & 0xffu),
                             (uint8_t) ((value >> 8) & 0xffu),
                             (uint8_t) ((value >> 16) & 0xffu),
                             (uint8_t) ((value >> 24) & 0xffu) };
    appendBytes (out, bytes, sizeof (bytes));
}

inline void appendU16 (std::vector<uint8_t>& out, uint16_t value)
{
    const uint8_t bytes[2] { (uint8_t) (value & 0xffu), (uint8_t) ((value >> 8) & 0xffu) };
    appendBytes (out, bytes, sizeof (bytes));
}
} // namespace detail

/** Writes interleaved-from-planar float data. Returns false if the file could not
    be opened. */
inline bool writeFloat32 (const std::string& path,
                          const float* const* channels,
                          int numChannels,
                          int numSamples,
                          double sampleRate)
{
    if (numChannels <= 0 || numSamples <= 0)
        return false;

    const auto bytesPerSample = (uint32_t) sizeof (float);
    const auto blockAlign = (uint32_t) numChannels * bytesPerSample;
    const auto dataBytes = (uint32_t) numSamples * blockAlign;

    std::vector<uint8_t> file;
    file.reserve (dataBytes + 64);

    detail::appendTag (file, "RIFF");
    detail::appendU32 (file, 4 + 24 + 12 + 8 + dataBytes); // everything after this field
    detail::appendTag (file, "WAVE");

    detail::appendTag (file, "fmt ");
    detail::appendU32 (file, 16);
    detail::appendU16 (file, 3); // WAVE_FORMAT_IEEE_FLOAT
    detail::appendU16 (file, (uint16_t) numChannels);
    detail::appendU32 (file, (uint32_t) sampleRate);
    detail::appendU32 (file, (uint32_t) sampleRate * blockAlign);
    detail::appendU16 (file, (uint16_t) blockAlign);
    detail::appendU16 (file, 32);

    // The spec requires a 'fact' chunk for non-PCM formats. Cheap insurance against
    // a picky decoder.
    detail::appendTag (file, "fact");
    detail::appendU32 (file, 4);
    detail::appendU32 (file, (uint32_t) numSamples);

    detail::appendTag (file, "data");
    detail::appendU32 (file, dataBytes);

    // Both arm64 and x86_64 are little-endian, which is what WAV wants, so the
    // float bytes can go straight out.
    for (int n = 0; n < numSamples; ++n)
        for (int ch = 0; ch < numChannels; ++ch)
            detail::appendBytes (file, &channels[ch][n], sizeof (float));

    std::FILE* handle = std::fopen (path.c_str(), "wb");

    if (handle == nullptr)
        return false;

    const auto written = std::fwrite (file.data(), 1, file.size(), handle);
    std::fclose (handle);

    return written == file.size();
}

/** Writes the little JSON sidecar the compare app reads for titles and parameters.
    One file per render, so parallel ctest runs never collide over a shared index. */
inline bool writeSidecar (const std::string& path, const std::string& json)
{
    std::FILE* handle = std::fopen (path.c_str(), "wb");

    if (handle == nullptr)
        return false;

    const auto written = std::fwrite (json.data(), 1, json.size(), handle);
    std::fclose (handle);

    return written == json.size();
}

} // namespace graindelay::wav
