#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>
#include <vector>

namespace maz::audio {

// SpectrumAnalyzer — Godot's AudioEffectSpectrumAnalyzer: turn a block of audio samples into a frequency
// spectrum so a game can react to sound (rhythm games, VU meters / equalizer visualizers, beat-reactive
// lights and particles, lip-sync). It runs an in-place radix-2 FFT over a windowed, zero-padded frame and
// exposes the per-bin magnitudes plus `magnitudeForRange(lowHz, highHz)` — the same query Godot's analyzer
// gives (`get_magnitude_for_frequency_range`) — for band energy (bass / mid / treble meters). Pure DSP maths,
// header-only, deterministic — it unit-tests exactly (a pure tone peaks on its bin) and drives a golden
// (a spectrum bar graph).

using Cplx = std::complex<float>;

// In-place iterative radix-2 Cooley-Tukey FFT. `a.size()` must be a power of two. inverse=true computes the
// IFFT (scaled by 1/N), so fft(x,false) then fft(x,true) round-trips.
inline void fft(std::vector<Cplx>& a, bool inverse) {
    const std::size_t n = a.size();
    if (n <= 1) {
        return;
    }
    // Bit-reversal permutation.
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }
    const float pi = 3.14159265358979323846f;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const float ang = (inverse ? 2.0f : -2.0f) * pi / static_cast<float>(len);
        const Cplx wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            Cplx w(1.0f, 0.0f);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const Cplx u = a[i + k];
                const Cplx v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        const float inv = 1.0f / static_cast<float>(n);
        for (Cplx& x : a) {
            x *= inv;
        }
    }
}

// Smallest power of two >= n (>= 1).
inline std::size_t nextPow2(std::size_t n) {
    std::size_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

enum class SpectrumWindow { None, Hann };

class SpectrumAnalyzer {
public:
    SpectrumAnalyzer() = default;
    SpectrumAnalyzer(float sampleRate, std::size_t fftSize, SpectrumWindow win = SpectrumWindow::Hann) {
        configure(sampleRate, fftSize, win);
    }

    void configure(float sampleRate, std::size_t fftSize, SpectrumWindow win = SpectrumWindow::Hann) {
        m_sampleRate = sampleRate;
        m_size = nextPow2(fftSize);
        m_window = win;
        m_mag.assign(m_size / 2 + 1, 0.0f);
    }

    // Analyze up to fftSize real samples: window, zero-pad, FFT, store magnitudes for bins 0..N/2.
    void analyze(const float* samples, std::size_t count) {
        std::vector<Cplx> buf(m_size, Cplx(0.0f, 0.0f));
        const std::size_t n = std::min(count, m_size);
        const float pi = 3.14159265358979323846f;
        for (std::size_t i = 0; i < n; ++i) {
            float w = 1.0f;
            if (m_window == SpectrumWindow::Hann && m_size > 1) {
                w = 0.5f - 0.5f * std::cos(2.0f * pi * static_cast<float>(i) / static_cast<float>(m_size - 1));
            }
            buf[i] = Cplx(samples[i] * w, 0.0f);
        }
        fft(buf, false);
        m_mag.assign(m_size / 2 + 1, 0.0f);
        const float scale = 1.0f / static_cast<float>(m_size);
        for (std::size_t i = 0; i < m_mag.size(); ++i) {
            // Single-sided magnitude: interior bins carry a mirror twin, so double them.
            const float twin = (i == 0 || i == m_size / 2) ? 1.0f : 2.0f;
            m_mag[i] = std::abs(buf[i]) * scale * twin;
        }
    }

    void analyze(const std::vector<float>& samples) { analyze(samples.data(), samples.size()); }

    std::size_t fftSize() const { return m_size; }
    std::size_t binCount() const { return m_mag.size(); }
    float sampleRate() const { return m_sampleRate; }

    float magnitude(std::size_t bin) const { return bin < m_mag.size() ? m_mag[bin] : 0.0f; }
    float binFrequency(std::size_t bin) const {
        return static_cast<float>(bin) * m_sampleRate / static_cast<float>(m_size);
    }

    // Loudest bin (its centre frequency is peakBin() * sampleRate / fftSize).
    std::size_t peakBin() const {
        std::size_t best = 0;
        float bestVal = -1.0f;
        for (std::size_t i = 0; i < m_mag.size(); ++i) {
            if (m_mag[i] > bestVal) {
                bestVal = m_mag[i];
                best = i;
            }
        }
        return best;
    }

    // Godot get_magnitude_for_frequency_range: the peak magnitude across [lowHz, highHz] (inclusive).
    float magnitudeForRange(float lowHz, float highHz) const {
        float peak = 0.0f;
        for (std::size_t i = 0; i < m_mag.size(); ++i) {
            const float f = binFrequency(i);
            if (f >= lowHz && f <= highHz) {
                peak = std::max(peak, m_mag[i]);
            }
        }
        return peak;
    }

private:
    float m_sampleRate = 44100.0f;
    std::size_t m_size = 1024;
    SpectrumWindow m_window = SpectrumWindow::Hann;
    std::vector<float> m_mag;
};

} // namespace maz::audio
