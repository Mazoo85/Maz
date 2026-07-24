#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::audio window functions — the tapering envelopes you multiply a block of samples by BEFORE an FFT (or
// a filter design) so the block's abrupt start/end don't smear energy across the whole spectrum ("spectral
// leakage"). Multiplying by a window that fades smoothly to zero at both ends turns a rough chunk of audio
// into a clean, analysable frame — the standard front-end for the engine's SpectrumAnalyzer / FFT
// (Spectrum.hpp only had an inline Hann), for building FIR filters, and for smooth grain/crossfade
// envelopes. Provides the classic family — rectangular (none), Hann, Hamming, Blackman, Blackman-Harris,
// and Bartlett (triangular) — each a symmetric taper peaking at the centre, plus helpers to apply one to a
// buffer and to report its coherent gain (mean value, the amplitude-scaling a window imposes). Godot exposes
// no window functions. Header-only, std-only, deterministic.
namespace maz::audio {

enum class WindowType { Rectangular, Hann, Hamming, Blackman, BlackmanHarris, Bartlett };

// The window value at sample index `n` (0..N-1) for a window of length `N`. Symmetric; peaks at the centre.
inline float windowValue(WindowType type, std::size_t n, std::size_t N) {
    if (N <= 1) {
        return 1.0f;
    }
    const float twoPi = 6.28318530717958648f;
    const float d = static_cast<float>(N - 1);
    const float x = static_cast<float>(n) / d; // 0..1
    const float w = twoPi * x;
    switch (type) {
        case WindowType::Rectangular:
            return 1.0f;
        case WindowType::Hann:
            return 0.5f - 0.5f * std::cos(w);
        case WindowType::Hamming:
            return 0.54f - 0.46f * std::cos(w);
        case WindowType::Blackman:
            return 0.42f - 0.5f * std::cos(w) + 0.08f * std::cos(2.0f * w);
        case WindowType::BlackmanHarris:
            return 0.35875f - 0.48829f * std::cos(w) + 0.14128f * std::cos(2.0f * w) - 0.01168f * std::cos(3.0f * w);
        case WindowType::Bartlett: {
            const float half = d * 0.5f;
            return 1.0f - std::fabs((static_cast<float>(n) - half) / half);
        }
    }
    return 1.0f;
}

// Multiply a buffer in place by the given window (buf.size() sets N).
inline void applyWindow(std::vector<float>& buf, WindowType type) {
    const std::size_t N = buf.size();
    for (std::size_t n = 0; n < N; ++n) {
        buf[n] *= windowValue(type, n, N);
    }
}

// The coherent gain of a window: the mean of its samples. When you window a pure tone before an FFT, its
// measured amplitude is scaled by this factor — divide by it to recover the true amplitude. (Rectangular = 1,
// Hann ~= 0.5, Hamming ~= 0.54.)
inline float coherentGain(WindowType type, std::size_t N) {
    if (N == 0) {
        return 0.0f;
    }
    double sum = 0.0;
    for (std::size_t n = 0; n < N; ++n) {
        sum += static_cast<double>(windowValue(type, n, N));
    }
    return static_cast<float>(sum / static_cast<double>(N));
}

} // namespace maz::audio
