#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::audio Goertzel single-frequency detector — measure how much of ONE specific frequency is present in a
// block of samples, far cheaper than a full FFT when you only care about a handful of target tones. The
// Goertzel algorithm evaluates a single DFT bin with a tiny two-tap recurrence, so it is the classic tool for
// DTMF/touch-tone decoding, detecting a whistle or reference pitch, a cheap guitar-tuner bin, or watching a
// couple of alarm/marker frequencies in a stream without paying for a whole spectrum. Returns the complex bin
// (real/imag) and its magnitude; the magnitude of a pure sine of amplitude A landing on an integer bin is
// A*N/2 (the DFT convention). Godot's only frequency tool is the full AudioEffectSpectrumAnalyzer. Uses
// double internally for accuracy. Header-only, std-only, deterministic.
namespace maz::audio {

struct GoertzelBin {
    float re = 0.0f;
    float im = 0.0f;
    float magnitude() const { return std::sqrt(re * re + im * im); }
};

// Evaluate the DFT bin `k` (a normalized frequency in bins, k = frequencyHz * N / sampleRate; may be
// fractional) of the samples `x`. Its MAGNITUDE equals the DFT bin's magnitude |X[k]| exactly (the standard
// Goertzel guarantee); the real/imag phase is the resonator's, relative to the block, not the DFT reference.
inline GoertzelBin goertzelBin(const std::vector<float>& x, float k) {
    GoertzelBin out;
    const std::size_t N = x.size();
    if (N == 0) {
        return out;
    }
    const double w = 2.0 * 3.14159265358979324 * static_cast<double>(k) / static_cast<double>(N);
    const double cw = std::cos(w), sw = std::sin(w);
    const double coeff = 2.0 * cw;
    double s1 = 0.0, s2 = 0.0;
    for (std::size_t n = 0; n < N; ++n) {
        const double s0 = static_cast<double>(x[n]) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    out.re = static_cast<float>(s1 - s2 * cw);
    out.im = static_cast<float>(-s2 * sw); // e^{-i...} convention
    return out;
}

// Magnitude of a single DFT bin `k`.
inline float goertzelMagnitude(const std::vector<float>& x, float k) { return goertzelBin(x, k).magnitude(); }

// Convenience: the magnitude at a physical frequency (Hz) given the sample rate. Maps to bin
// k = targetHz * N / sampleRate.
inline float goertzelMagnitudeHz(const std::vector<float>& x, float targetHz, float sampleRate) {
    if (sampleRate <= 0.0f || x.empty()) {
        return 0.0f;
    }
    const float k = targetHz * static_cast<float>(x.size()) / sampleRate;
    return goertzelMagnitude(x, k);
}

} // namespace maz::audio
