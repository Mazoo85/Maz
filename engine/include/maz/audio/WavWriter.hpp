#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace maz::audio {

// Downmix an interleaved multi-channel buffer to a single mono channel (the average of the channels),
// returning `frames` samples. Used for mono export. Returns an empty vector on invalid arguments.
inline std::vector<float> downmixToMono(const float* interleaved, int frames, int channels) {
    std::vector<float> mono;
    if (interleaved == nullptr || frames <= 0 || channels <= 0) {
        return mono;
    }
    mono.resize(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += interleaved[static_cast<size_t>(i) * static_cast<size_t>(channels) +
                               static_cast<size_t>(c)];
        }
        mono[static_cast<size_t>(i)] = sum / static_cast<float>(channels);
    }
    return mono;
}

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

// Remove any DC offset from a rendered mix by subtracting each channel's mean (FL Studio's export
// "remove DC offset"). A constant bias wastes headroom, can click on start/stop, and stresses woofers;
// centring each channel at zero fixes it exactly. Best applied BEFORE normalization so the level stage
// sees the centred signal. No-op on a null/empty buffer. `interleaved` is `frames*channels` samples.
inline void removeDcOffset(float* interleaved, int frames, int channels) {
    if (interleaved == nullptr || frames <= 0 || channels <= 0) {
        return;
    }
    for (int c = 0; c < channels; ++c) {
        double sum = 0.0;
        for (int i = 0; i < frames; ++i) {
            sum += static_cast<double>(interleaved[i * channels + c]);
        }
        const float mean = static_cast<float>(sum / static_cast<double>(frames));
        if (mean != 0.0f) {
            for (int i = 0; i < frames; ++i) {
                interleaved[i * channels + c] -= mean;
            }
        }
    }
}

// Loudness (RMS) normalization: scale the mix so its average level (RMS) reaches `targetRms` (linear),
// which tracks perceived loudness far better than peak normalization — quiet masters are lifted to a
// consistent loudness rather than merely to full scale. To guarantee no clipping, the gain is capped
// so the loudest sample stays at or under `peakCeiling`; a very peaky (transient-heavy) mix therefore
// lands a little below the RMS target rather than clipping. Like peakNormalize this applies ONE
// constant gain — no compression/limiting — so it never alters dynamics. Returns the gain applied; a
// no-op (1.0, buffer untouched) on silence, a null/empty buffer, or a non-positive target. Apply to a
// final mix bounce, not to individual stems (their RMS differ, so per-stem normalizing skews balance).
inline float rmsNormalize(float* interleaved, int count, float targetRms = 0.125f,
                          float peakCeiling = 0.966f) {
    if (interleaved == nullptr || count <= 0 || targetRms <= 0.0f) {
        return 1.0f;
    }
    double sq = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float s = interleaved[i];
        sq += static_cast<double>(s) * static_cast<double>(s);
        const float a = std::fabs(s);
        if (a > peak) {
            peak = a;
        }
    }
    const float rms = static_cast<float>(std::sqrt(sq / static_cast<double>(count)));
    if (rms <= 0.0f) {
        return 1.0f; // silence — nothing to scale
    }
    float gain = targetRms / rms;
    if (peakCeiling > 0.0f && peak * gain > peakCeiling) {
        gain = peakCeiling / peak; // cap so the peak never clips (peaky mixes fall short of target)
    }
    for (int i = 0; i < count; ++i) {
        interleaved[i] *= gain;
    }
    return gain;
}

// Write `frames` of interleaved float32 samples (values expected in [-1, 1]) to a canonical 16-bit
// PCM WAV file. Samples outside the range are clamped. Returns false and sets *err (when non-null)
// on any I/O failure.
//
// `dither`: when true, add triangular-PDF (TPDF) dither of ±1 LSB before quantizing float→PCM.
// Dithering decorrelates the quantization error from the signal, trading audible quantization
// distortion on quiet/fading tails for a benign, constant, inaudible noise floor — the standard
// mastering step when reducing bit depth (FL Studio's export "Dithering" option). The dither uses a
// fixed-seed PRNG so a given input renders to a byte-identical WAV every time. false (default) keeps
// the exact previous behaviour (plain rounding, no added noise).
//
// `bits`: output bit depth — 16 (default) or 24 PCM, or 32 for IEEE float (a lossless, no-clip
// export for stems/further processing; dither is ignored for float). Any other value falls back to
// 16. 24-bit's noise floor is low enough that dithering is rarely needed there.
bool writeWav16(const std::string& path, const float* interleaved, int frames, int channels,
                int sampleRate, std::string* err = nullptr, bool dither = false, int bits = 16);

} // namespace maz::audio
