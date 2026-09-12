#pragma once

#include <cstddef>
#include <vector>

// maz::audio monophonic pitch detection — the YIN algorithm (de Cheveigné & Kawahara, 2002).
//
// Given a block of mono audio samples, estimate the fundamental frequency (the perceived pitch). This
// is what a guitar/vocal TUNER, a rhythm game that scores sung or played notes, auto-harmony, and
// voice-driven mechanics all need. Naive autocorrelation famously "octave-errors" — it locks onto a
// harmonic instead of the fundamental. YIN fixes that with a cumulative-mean-normalised difference
// function plus an absolute threshold, then refines the estimate to sub-sample precision with parabolic
// interpolation. It is monophonic (one note at a time), CPU-only, and deterministic — so it unit-tests
// headlessly against synthesised tones of known frequency, including harmonic-rich ones that trip up
// autocorrelation. Header-only.
namespace maz::audio {

struct PitchResult {
    float frequency = 0.0f;  // estimated fundamental in Hz (0 if none found)
    float confidence = 0.0f; // 1 - normalised difference at the chosen lag (0..1); higher = surer
    bool found = false;      // true when a clear pitch was detected below the threshold
};

// Estimate the pitch of `n` mono samples at `sampleRate` Hz. `threshold` is YIN's aperiodicity cutoff
// (0.1-0.2 typical; lower = stricter). Only lags corresponding to [minFreq, maxFreq] are searched.
inline PitchResult detectPitchYin(const float* samples, int n, float sampleRate, float threshold = 0.15f,
                                  float minFreq = 50.0f, float maxFreq = 2000.0f) {
    PitchResult out;
    if (samples == nullptr || n < 4 || sampleRate <= 0.0f || minFreq <= 0.0f || maxFreq <= minFreq) {
        return out;
    }
    int tauMax = static_cast<int>(sampleRate / minFreq);
    if (tauMax > n / 2) tauMax = n / 2;
    int tauMin = static_cast<int>(sampleRate / maxFreq);
    if (tauMin < 2) tauMin = 2;
    if (tauMax <= tauMin) {
        return out;
    }
    const int W = n - tauMax; // number of terms in the difference sum
    if (W < 2) {
        return out;
    }

    // Step 1: difference function d(tau).
    std::vector<float> d(static_cast<std::size_t>(tauMax) + 1, 0.0f);
    for (int tau = 0; tau <= tauMax; ++tau) {
        float sum = 0.0f;
        for (int j = 0; j < W; ++j) {
            const float delta = samples[j] - samples[j + tau];
            sum += delta * delta;
        }
        d[static_cast<std::size_t>(tau)] = sum;
    }

    // Step 2: cumulative mean normalised difference d'(tau).
    std::vector<float> dp(static_cast<std::size_t>(tauMax) + 1, 1.0f);
    float running = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau) {
        running += d[static_cast<std::size_t>(tau)];
        if (running <= 0.0f) {
            dp[static_cast<std::size_t>(tau)] = 1.0f; // flat/silent input — no periodicity
        } else {
            dp[static_cast<std::size_t>(tau)] =
                d[static_cast<std::size_t>(tau)] * static_cast<float>(tau) / running;
        }
    }

    // Step 3: absolute threshold — first lag (>= tauMin) dipping below `threshold`, taken to its local min.
    int tauEst = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau) {
        if (dp[static_cast<std::size_t>(tau)] < threshold) {
            while (tau + 1 <= tauMax &&
                   dp[static_cast<std::size_t>(tau + 1)] < dp[static_cast<std::size_t>(tau)]) {
                ++tau;
            }
            tauEst = tau;
            break;
        }
    }
    if (tauEst < 0) {
        // Nothing crossed the threshold: report the best (lowest-dp) lag but mark it not-found.
        int best = tauMin;
        for (int tau = tauMin; tau <= tauMax; ++tau) {
            if (dp[static_cast<std::size_t>(tau)] < dp[static_cast<std::size_t>(best)]) best = tau;
        }
        out.frequency = sampleRate / static_cast<float>(best);
        out.confidence = 1.0f - dp[static_cast<std::size_t>(best)];
        out.found = false;
        return out;
    }

    // Step 4: parabolic interpolation around tauEst for sub-sample precision.
    float betterTau = static_cast<float>(tauEst);
    if (tauEst > tauMin && tauEst < tauMax) {
        const float a = dp[static_cast<std::size_t>(tauEst - 1)];
        const float b = dp[static_cast<std::size_t>(tauEst)];
        const float c = dp[static_cast<std::size_t>(tauEst + 1)];
        const float denom = a + c - 2.0f * b;
        if (denom != 0.0f) {
            betterTau += 0.5f * (a - c) / denom;
        }
    }

    out.frequency = sampleRate / betterTau;
    out.confidence = 1.0f - dp[static_cast<std::size_t>(tauEst)];
    out.found = true;
    return out;
}

inline PitchResult detectPitchYin(const std::vector<float>& samples, float sampleRate,
                                  float threshold = 0.15f, float minFreq = 50.0f,
                                  float maxFreq = 2000.0f) {
    return detectPitchYin(samples.data(), static_cast<int>(samples.size()), sampleRate, threshold,
                          minFreq, maxFreq);
}

} // namespace maz::audio
