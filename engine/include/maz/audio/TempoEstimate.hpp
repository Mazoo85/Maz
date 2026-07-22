#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::audio tempo (BPM) estimation — find the beat rate of a piece of music from its samples.
//
// Rhythm games, beat-synced visuals/lighting, auto-cut editors, and adaptive music all want to know the
// tempo. The approach here is the classic one: build an ONSET-STRENGTH signal (how much the short-time
// energy JUMPS up from one frame to the next — a proxy for "a beat just happened"), then AUTOCORRELATE
// it. A steady beat makes the onset signal periodic, so its autocorrelation peaks at the beat period;
// the lag of that peak, refined with parabolic interpolation, converts to BPM. Searches a musical
// range (default 60-200 BPM). Pure CPU, header-only, deterministic — unit-tested by feeding synthetic
// click tracks of known tempo and reading the BPM back.
//
// Honest caveat — the OCTAVE ambiguity: a steady beat autocorrelates just as strongly at half and at
// double its true period, so every onset-autocorrelation tempo detector (this one included) can land on
// a tempo octave (e.g. 75 for a 150 BPM track). A perceptual log-tempo prior (Gaussian centred on
// `priorBpm`) biases the pick toward musically-typical tempos and resolves this near the centre; outside
// the comfortable band the answer is correct to within an octave. This matches how the field evaluates
// trackers ("Accuracy-2" credits octave-equivalent answers).
namespace maz::audio {

struct TempoResult {
    float bpm = 0.0f;        // estimated tempo in beats per minute (0 if none found)
    float confidence = 0.0f; // normalised autocorrelation peak height (0..1)
    bool found = false;
};

// Estimate tempo from `n` mono samples at `sampleRate`. `hop` is the analysis frame size in samples
// (smaller = finer time resolution, more cost). Searches [minBpm, maxBpm]. `priorBpm`/`priorWidth`
// apply a perceptual log-tempo prior (a Gaussian centred on priorBpm, priorWidth in octaves) that
// resolves the classic tempo-OCTAVE ambiguity — a steady beat autocorrelates equally at half and double
// its true period, and the prior biases the pick toward musically-typical tempos (librosa's approach).
inline TempoResult estimateTempo(const float* samples, int n, float sampleRate, int hop = 512,
                                 float minBpm = 60.0f, float maxBpm = 200.0f, float priorBpm = 120.0f,
                                 float priorWidth = 1.0f) {
    TempoResult out;
    if (samples == nullptr || n <= 0 || sampleRate <= 0.0f || hop < 1 || minBpm <= 0.0f ||
        maxBpm <= minBpm) {
        return out;
    }
    const int frames = n / hop;
    if (frames < 4) {
        return out;
    }

    // Per-frame RMS energy.
    std::vector<float> energy(static_cast<std::size_t>(frames), 0.0f);
    for (int f = 0; f < frames; ++f) {
        double sum = 0.0;
        const int base = f * hop;
        for (int i = 0; i < hop; ++i) {
            const float s = samples[base + i];
            sum += static_cast<double>(s) * static_cast<double>(s);
        }
        energy[static_cast<std::size_t>(f)] = static_cast<float>(std::sqrt(sum / static_cast<double>(hop)));
    }

    // Onset strength = positive first difference of energy (half-wave rectified), mean-removed.
    std::vector<float> onset(static_cast<std::size_t>(frames), 0.0f);
    float mean = 0.0f;
    for (int f = 1; f < frames; ++f) {
        const float d = energy[static_cast<std::size_t>(f)] - energy[static_cast<std::size_t>(f - 1)];
        const float o = d > 0.0f ? d : 0.0f;
        onset[static_cast<std::size_t>(f)] = o;
        mean += o;
    }
    mean /= static_cast<float>(frames);
    for (int f = 0; f < frames; ++f) onset[static_cast<std::size_t>(f)] -= mean;

    const float frameRate = sampleRate / static_cast<float>(hop);
    int minLag = static_cast<int>(frameRate * 60.0f / maxBpm);
    int maxLag = static_cast<int>(frameRate * 60.0f / minBpm) + 1;
    if (minLag < 1) minLag = 1;
    if (maxLag >= frames) maxLag = frames - 1;
    if (maxLag <= minLag) {
        return out;
    }

    // Zero-lag energy for normalisation.
    double zero = 0.0;
    for (int f = 0; f < frames; ++f)
        zero += static_cast<double>(onset[static_cast<std::size_t>(f)]) *
                static_cast<double>(onset[static_cast<std::size_t>(f)]);
    if (zero <= 0.0) {
        return out;
    }

    // Autocorrelation across the lag range, weighted by a perceptual log-tempo prior. The prior
    // resolves the half/double-tempo octave ambiguity; the peak is chosen on the WEIGHTED score, while
    // the raw autocorrelation is kept for the confidence report and parabolic refinement.
    int bestLag = minLag;
    double bestWeighted = -1e30;
    std::vector<double> ac(static_cast<std::size_t>(maxLag) + 1, 0.0);
    const bool usePrior = priorBpm > 0.0f && priorWidth > 0.0f;
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double sum = 0.0;
        for (int f = lag; f < frames; ++f) {
            sum += static_cast<double>(onset[static_cast<std::size_t>(f)]) *
                   static_cast<double>(onset[static_cast<std::size_t>(f - lag)]);
        }
        ac[static_cast<std::size_t>(lag)] = sum;
        double weighted = sum;
        if (usePrior && sum > 0.0) {
            const double bpm = 60.0 * static_cast<double>(frameRate) / static_cast<double>(lag);
            const double z = std::log2(bpm / static_cast<double>(priorBpm)) / static_cast<double>(priorWidth);
            weighted = sum * std::exp(-0.5 * z * z);
        }
        if (weighted > bestWeighted) {
            bestWeighted = weighted;
            bestLag = lag;
        }
    }
    const double bestVal = ac[static_cast<std::size_t>(bestLag)];

    // Parabolic interpolation around the peak lag for sub-frame precision.
    float refined = static_cast<float>(bestLag);
    if (bestLag > minLag && bestLag < maxLag) {
        const double a = ac[static_cast<std::size_t>(bestLag - 1)];
        const double b = ac[static_cast<std::size_t>(bestLag)];
        const double c = ac[static_cast<std::size_t>(bestLag + 1)];
        const double denom = a + c - 2.0 * b;
        if (denom != 0.0) {
            refined += static_cast<float>(0.5 * (a - c) / denom);
        }
    }
    if (refined <= 0.0f) {
        return out;
    }

    out.bpm = 60.0f * frameRate / refined;
    out.confidence = static_cast<float>(bestVal / zero);
    out.found = bestVal > 0.0;
    return out;
}

inline TempoResult estimateTempo(const std::vector<float>& samples, float sampleRate, int hop = 512,
                                 float minBpm = 60.0f, float maxBpm = 200.0f) {
    return estimateTempo(samples.data(), static_cast<int>(samples.size()), sampleRate, hop, minBpm, maxBpm);
}

} // namespace maz::audio
