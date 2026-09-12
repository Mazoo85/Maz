#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::audio — Karplus-Strong plucked-string synthesis. A startlingly simple recipe that produces
// convincing plucked/struck string tones (guitar, harp, koto, a twangy UI blip) with no samples: fill a
// short delay line with a burst of noise (the "pluck"), then repeatedly play it back while averaging each
// pair of adjacent samples. The averaging is a gentle low-pass that shaves the high frequencies a little more
// on each pass, so the bright noisy attack mellows into a decaying harmonic tone whose PITCH is set by the
// delay-line length (frequency = sampleRate / length). It is the classic physical-modelling synthesis method
// — cheap enough to run per-note at runtime for procedural instruments and impact sounds. Godot has
// oscillators/samples but no string model. Header-only, std-only, deterministic (seeded noise burst).
namespace maz::audio {

// Synthesize `sampleCount` samples of a plucked string at `frequency` Hz. `decay` in (0,1] controls sustain
// (closer to 1 rings longer). `seed` chooses the noise burst. Output samples are in roughly [-1, 1].
inline std::vector<float> karplusStrongPluck(float frequency, int sampleRate, int sampleCount, float decay,
                                             std::uint32_t seed) {
    std::vector<float> out;
    if (frequency <= 0.0f || sampleRate <= 0 || sampleCount <= 0) {
        return out;
    }
    int delayLen = static_cast<int>(std::lround(static_cast<double>(sampleRate) / static_cast<double>(frequency)));
    if (delayLen < 2) {
        delayLen = 2;
    }
    if (decay > 1.0f) {
        decay = 1.0f;
    }
    if (decay < 0.0f) {
        decay = 0.0f;
    }
    // Seed the delay line with a white-noise burst in [-1, 1].
    std::vector<float> buf(static_cast<std::size_t>(delayLen));
    std::uint64_t s = seed * 2654435761ull + 0x9E3779B97F4A7C15ull;
    for (int i = 0; i < delayLen; ++i) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        const std::uint32_t r = static_cast<std::uint32_t>(s >> 33);
        buf[static_cast<std::size_t>(i)] = static_cast<float>(r % 20001u) / 10000.0f - 1.0f;
    }
    out.resize(static_cast<std::size_t>(sampleCount));
    int idx = 0;
    for (int n = 0; n < sampleCount; ++n) {
        const float cur = buf[static_cast<std::size_t>(idx)];
        out[static_cast<std::size_t>(n)] = cur;
        const int nextIdx = (idx + 1) % delayLen;
        // Karplus-Strong update: low-pass (average of two adjacent samples) times the decay factor.
        buf[static_cast<std::size_t>(idx)] = decay * 0.5f * (cur + buf[static_cast<std::size_t>(nextIdx)]);
        idx = nextIdx;
    }
    return out;
}

} // namespace maz::audio
