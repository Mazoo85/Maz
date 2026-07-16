#pragma once

#include <string>
#include <vector>

namespace maz::audio {

// Decoded PCM audio from a WAV file: interleaved float samples in [-1, 1].
struct WavData {
    std::vector<float> samples; // interleaved, channels-major per frame
    int channels = 0;
    int sampleRate = 0;

    int frames() const { return channels > 0 ? static_cast<int>(samples.size()) / channels : 0; }

    // Downmix to a single mono channel (average of channels).
    std::vector<float> toMono() const;
};

// Read a canonical 16-bit PCM WAV file. Returns false and sets *err (when non-null) on I/O errors or
// unsupported formats (only 16-bit PCM is handled). Extra/unknown chunks are skipped.
bool readWav16(const std::string& path, WavData& out, std::string* err = nullptr);

} // namespace maz::audio
