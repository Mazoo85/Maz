#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::audio envelope follower + level metering — track the moment-to-moment loudness of a signal.
//
// A huge amount of audio behaviour keys off "how loud is this right now?": compressors and gates decide
// when to clamp, sidechain ducking lowers the music under a voice, a VU/peak meter drives a UI, auto-wah
// and envelope-driven filters sweep with the amplitude, and onset/beat detection watches the envelope
// for jumps. The raw waveform swings +/- many times per cycle, so you cannot read level off it directly;
// an envelope follower smooths the rectified/squared signal with separate ATTACK (how fast it rises to a
// louder level) and RELEASE (how slowly it falls back) time constants — the classic one-pole detector.
// Peak mode follows |x|; RMS mode follows sqrt(mean of x^2), the perceptually-truer "energy" level.
// Plus block helpers rms()/peakLevel() for one-shot metering. Pure CPU, header-only, deterministic —
// unit-tested against the exact one-pole step response (1 - 1/e of the target after one time constant).
namespace maz::audio {

enum class DetectMode { Peak, Rms };

class EnvelopeFollower {
public:
    // Configure attack/release times (milliseconds) at a given sample rate. Time constant tau: after
    // one tau of a step input the envelope reaches 1 - 1/e (~63.2%) of the new target.
    void configure(float attackMs, float releaseMs, float sampleRate, DetectMode mode = DetectMode::Peak) {
        mode_ = mode;
        attackCoef_ = coef(attackMs, sampleRate);
        releaseCoef_ = coef(releaseMs, sampleRate);
    }

    void reset(float value = 0.0f) { state_ = value; }

    // Feed one sample; return the current envelope level (in the same units as the input amplitude).
    float process(float x) {
        const float target = mode_ == DetectMode::Rms ? x * x : std::fabs(x);
        const float c = target > state_ ? attackCoef_ : releaseCoef_;
        state_ = c * state_ + (1.0f - c) * target;
        return mode_ == DetectMode::Rms ? std::sqrt(state_ < 0.0f ? 0.0f : state_) : state_;
    }

    // Current level without advancing.
    float value() const { return mode_ == DetectMode::Rms ? std::sqrt(state_ < 0.0f ? 0.0f : state_) : state_; }

private:
    static float coef(float ms, float sampleRate) {
        const float samples = ms * 0.001f * sampleRate;
        if (samples <= 0.0f) return 0.0f; // instantaneous
        return static_cast<float>(std::exp(-1.0 / static_cast<double>(samples)));
    }

    DetectMode mode_ = DetectMode::Peak;
    float attackCoef_ = 0.0f;
    float releaseCoef_ = 0.0f;
    float state_ = 0.0f; // peak: |x| envelope; rms: mean-square envelope
};

// Root-mean-square level of a block: sqrt(mean(x^2)). The standard "energy" loudness measure.
inline float rms(const float* x, int n) {
    if (x == nullptr || n <= 0) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += static_cast<double>(x[i]) * static_cast<double>(x[i]);
    return static_cast<float>(std::sqrt(sum / static_cast<double>(n)));
}
inline float rms(const std::vector<float>& x) { return rms(x.data(), static_cast<int>(x.size())); }

// Peak (maximum absolute) level of a block.
inline float peakLevel(const float* x, int n) {
    if (x == nullptr || n <= 0) return 0.0f;
    float m = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float a = std::fabs(x[i]);
        if (a > m) m = a;
    }
    return m;
}
inline float peakLevel(const std::vector<float>& x) { return peakLevel(x.data(), static_cast<int>(x.size())); }

} // namespace maz::audio
