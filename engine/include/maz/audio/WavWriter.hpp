#pragma once

#include <string>

namespace maz::audio {

// Write `frames` of interleaved float32 samples (values expected in [-1, 1]) to a canonical 16-bit
// PCM WAV file. Samples outside the range are clamped. Returns false and sets *err (when non-null)
// on any I/O failure.
bool writeWav16(const std::string& path, const float* interleaved, int frames, int channels,
                int sampleRate, std::string* err = nullptr);

} // namespace maz::audio
