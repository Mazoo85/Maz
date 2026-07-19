#pragma once

#include <cmath>
#include <string>

namespace maz::audio {

// Scale `interleaved` (a run of `count` samples) in place so its largest absolute sample equals
// `targetPeak` (linear; the default 0.966 ≈ -0.3 dBFS, leaving a hair of inter-sample headroom). This
// is static peak normalization — FL Studio's "Normalize" export option — and is deliberately NOT a
// compressor or limiter: it applies one constant gain and never alters the signal's shape or dynamics.
// Returns the gain applied. It is a no-op (returns 1.0, buffer untouched) on silence, on a null/empty
// buffer, or when `targetPeak <= 0`, and it is idempotent: normalizing an already-normalized buffer
// leaves it unchanged. Apply it to a final mix bounce, not to individual stems (per-stem peaks differ,
// so normalizing each independently would destroy their relative balance).
inline float peakNormalize(float* interleaved, int count, float targetPeak = 0.966f) {
    if (interleaved == nullptr || count <= 0 || targetPeak <= 0.0f) {
        return 1.0f;
    }
    float peak = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float a = std::fabs(interleaved[i]);
        if (a > peak) {
            peak = a;
        }
    }
    if (peak <= 0.0f) {
        return 1.0f; // silence — nothing to scale
    }
    const float gain = targetPeak / peak;
    for (int i = 0; i < count; ++i) {
        interleaved[i] *= gain;
    }
    return gain;
}

// Write `frames` of interleaved float32 samples (values expected in [-1, 1]) to a canonical 16-bit
// PCM WAV file. Samples outside the range are clamped. Returns false and sets *err (when non-null)
// on any I/O failure.
//
// `dither`: when true, add triangular-PDF (TPDF) dither of ±1 LSB before quantizing float→16-bit.
// Dithering decorrelates the quantization error from the signal, trading audible quantization
// distortion on quiet/fading tails for a benign, constant, inaudible noise floor — the standard
// mastering step when reducing to 16-bit (FL Studio's export "Dithering" option). The dither uses a
// fixed-seed PRNG so a given input renders to a byte-identical WAV every time. false (default) keeps
// the exact previous behaviour (plain rounding, no added noise).
bool writeWav16(const std::string& path, const float* interleaved, int frames, int channels,
                int sampleRate, std::string* err = nullptr, bool dither = false);

} // namespace maz::audio
