#include "maz/audio/Effects.hpp"

#include <algorithm>
#include <cmath>

namespace maz::audio {

namespace {
float dbToLin(float db) {
    return std::pow(10.0f, db / 20.0f);
}
float linToDb(float lin) {
    return 20.0f * std::log10(std::max(lin, 1e-9f));
}
} // namespace

// ---- Delay ------------------------------------------------------------------

void Delay::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    dampL_ = 0.0f;
    dampR_ = 0.0f;
}

namespace {
// Each sync division as a multiple of a quarter note (quarter = 60000/bpm ms).
constexpr float kDivMul[Delay::kSyncDivisions] = {
    4.0f,       // 1/1
    2.0f,       // 1/2
    1.0f,       // 1/4
    1.5f,       // 1/4.
    0.5f,       // 1/8
    0.75f,      // 1/8.
    1.0f / 3.0f, // 1/8T
    0.25f,      // 1/16
};
constexpr const char* kDivName[Delay::kSyncDivisions] = {
    "1/1", "1/2", "1/4", "1/4.", "1/8", "1/8.", "1/8T", "1/16",
};
} // namespace

const char* Delay::syncDivisionName(int div) {
    if (div < 0 || div >= kSyncDivisions) {
        return "?";
    }
    return kDivName[div];
}

void Delay::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    const double quarterMs = 60000.0 / bpm;
    timeMs_ = static_cast<float>(quarterMs * static_cast<double>(kDivMul[syncDiv_]));
}

void Delay::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // Size the delay line for up to 2 s; the tap moves within it as timeMs_ changes.
    const int maxSize = sampleRate * 2;
    if (size_ != maxSize) {
        size_ = maxSize;
        bufL_.assign(static_cast<size_t>(size_), 0.0f);
        bufR_.assign(static_cast<size_t>(size_), 0.0f);
        write_ = 0;
    }
    int tap = static_cast<int>(timeMs_ * 0.001f * static_cast<float>(sampleRate));
    tap = std::clamp(tap, 1, size_ - 1);
    const float fb = std::clamp(feedback_, 0.0f, 0.95f);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    // Damping: a one-pole high-cut on the feedback (damping 0 = off/bright, 1 = heavy darkening).
    const float dampCoef = std::clamp(damping_, 0.0f, 1.0f);

    for (int i = 0; i < frames; ++i) {
        const int r = (write_ - tap + size_) % size_;
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        const float wetL = bufL_[static_cast<size_t>(r)];
        const float wetR = bufR_[static_cast<size_t>(r)];
        // Low-pass the fed-back signal so successive repeats lose their highs.
        dampL_ += (1.0f - dampCoef) * (wetL - dampL_);
        dampR_ += (1.0f - dampCoef) * (wetR - dampR_);
        const float fbL = dampL_ * fb;
        const float fbR = dampR_ * fb;
        if (pingPong_) {
            // Cross-feed: each channel's echo re-enters the *other* channel's line, so repeats
            // alternate L→R→L across the stereo field.
            bufL_[static_cast<size_t>(write_)] = dryL + fbR;
            bufR_[static_cast<size_t>(write_)] = dryR + fbL;
        } else {
            bufL_[static_cast<size_t>(write_)] = dryL + fbL;
            bufR_[static_cast<size_t>(write_)] = dryR + fbR;
        }
        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;
        write_ = (write_ + 1) % size_;
    }
}

// ---- RingMod ----------------------------------------------------------------

void RingMod::reset() {
    phase_ = 0.0;
}

void RingMod::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr double kTwoPi = 6.283185307179586;
    const double inc = static_cast<double>(freqHz_) / static_cast<double>(sampleRate);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    for (int i = 0; i < frames; ++i) {
        const float carrier = static_cast<float>(std::sin(phase_ * kTwoPi));
        stereo[2 * i] = stereo[2 * i] * (1.0f - mix) + stereo[2 * i] * carrier * mix;
        stereo[2 * i + 1] = stereo[2 * i + 1] * (1.0f - mix) + stereo[2 * i + 1] * carrier * mix;
        phase_ += inc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- Distortion -------------------------------------------------------------

void Distortion::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float drive = std::max(drive_, 1.0f);
    const float tanhNorm = 1.0f / std::tanh(drive); // keep unity-ish level across drive
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    constexpr float kPi = 3.14159265f;
    const int n = frames * 2;
    for (int i = 0; i < n; ++i) {
        const float dry = stereo[i];
        const float x = dry * drive;
        float wet = 0.0f;
        switch (curve_) {
        case Curve::Soft:
            wet = std::tanh(x) * tanhNorm;
            break;
        case Curve::Hard:
            wet = std::clamp(x, -1.0f, 1.0f);
            break;
        case Curve::Fold: {
            // Triangle wavefolder: reflect the signal back whenever it exceeds ±1.
            float f = x;
            for (int k = 0; k < 4; ++k) {
                if (f > 1.0f) {
                    f = 2.0f - f;
                } else if (f < -1.0f) {
                    f = -2.0f - f;
                } else {
                    break;
                }
            }
            wet = f;
            break;
        }
        case Curve::SineFold:
            wet = std::sin(x * kPi * 0.5f);
            break;
        }
        stereo[i] = dry * (1.0f - mix) + wet * mix;
    }
}

// ---- Chorus -----------------------------------------------------------------

void Chorus::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0;
}

void Chorus::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const int maxSize = sampleRate / 20; // up to 50 ms of delay line
    if (size_ != maxSize) {
        size_ = maxSize;
        bufL_.assign(static_cast<size_t>(size_), 0.0f);
        bufR_.assign(static_cast<size_t>(size_), 0.0f);
        write_ = 0;
    }
    constexpr double kTwoPi = 6.283185307179586;
    const double phaseInc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    const float baseMs = 12.0f;
    const float baseSamp = baseMs * 0.001f * static_cast<float>(sampleRate);
    const float depthSamp = std::clamp(depthMs_, 0.0f, 20.0f) * 0.001f * static_cast<float>(sampleRate);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);

    auto readAt = [&](const std::vector<float>& buf, float delay) {
        float rp = static_cast<float>(write_) - delay;
        while (rp < 0.0f) {
            rp += static_cast<float>(size_);
        }
        const int i0 = static_cast<int>(rp) % size_;
        const int i1 = (i0 + 1) % size_;
        const float frac = rp - std::floor(rp);
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    };

    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        bufL_[static_cast<size_t>(write_)] = dryL;
        bufR_[static_cast<size_t>(write_)] = dryR;

        const float modL = static_cast<float>(std::sin(phase_ * kTwoPi));
        const float modR = static_cast<float>(std::sin((phase_ + 0.25) * kTwoPi)); // quadrature
        const float wetL = readAt(bufL_, baseSamp + depthSamp * modL);
        const float wetR = readAt(bufR_, baseSamp + depthSamp * modR);

        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;

        write_ = (write_ + 1) % size_;
        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- ParametricEQ -----------------------------------------------------------

void ParametricEQ::setLowGain(float db) {
    lowDb_ = db;
    dirty_ = true;
}
void ParametricEQ::setMid(float freq, float q, float db) {
    midFreq_ = freq;
    midQ_ = q;
    midDb_ = db;
    dirty_ = true;
}
void ParametricEQ::setHighGain(float db) {
    highDb_ = db;
    dirty_ = true;
}

void ParametricEQ::reset() {
    lowL_.reset();
    midL_.reset();
    highL_.reset();
    lowR_.reset();
    midR_.reset();
    highR_.reset();
}

void ParametricEQ::recompute(int sampleRate) {
    lowL_.setShelf(120.0f, lowDb_, sampleRate, false);
    lowR_.setShelf(120.0f, lowDb_, sampleRate, false);
    midL_.setPeaking(midFreq_, midQ_, midDb_, sampleRate);
    midR_.setPeaking(midFreq_, midQ_, midDb_, sampleRate);
    highL_.setShelf(6000.0f, highDb_, sampleRate, true);
    highR_.setShelf(6000.0f, highDb_, sampleRate, true);
    sr_ = sampleRate;
    dirty_ = false;
}

void ParametricEQ::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    if (dirty_ || sr_ != sampleRate) {
        recompute(sampleRate);
    }
    for (int i = 0; i < frames; ++i) {
        stereo[2 * i] = highL_.process(midL_.process(lowL_.process(stereo[2 * i])));
        stereo[2 * i + 1] = highR_.process(midR_.process(lowR_.process(stereo[2 * i + 1])));
    }
}

// ---- Exciter ----------------------------------------------------------------

void Exciter::reset() {
    lpL_ = 0.0f;
    lpR_ = 0.0f;
}

void Exciter::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    const float a = 1.0f - std::exp(-kTwoPi * crossover_ / static_cast<float>(sampleRate));
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        lpL_ += a * (l - lpL_);
        lpR_ += a * (r - lpR_);
        // Harmonics from the high band only, added back on top of the full signal.
        const float excL = std::tanh((l - lpL_) * 3.0f) * amount_;
        const float excR = std::tanh((r - lpR_) * 3.0f) * amount_;
        stereo[2 * i] = l + excL;
        stereo[2 * i + 1] = r + excR;
    }
}

// ---- TransientShaper --------------------------------------------------------

void TransientShaper::reset() {
    envAttFast_ = 0.0f;
    envAttSlow_ = 0.0f;
    envRelFast_ = 0.0f;
    envRelSlow_ = 0.0f;
}

void TransientShaper::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // One-pole smoothing coefficient for a given time constant in ms.
    auto coef = [sr](float ms) { return 1.0f - std::exp(-1.0f / (0.001f * ms * sr)); };
    // Attack detector: a very fast follower vs a slower one — the fast one leads on an onset.
    const float aFast = coef(0.5f);
    const float aSlow = coef(15.0f);
    // Sustain detector: same fast attack, but fast vs slow release so the slow one lags on the tail.
    const float rFast = coef(40.0f);
    const float rSlow = coef(300.0f);
    const float attackAtk = coef(1.0f); // shared quick attack for the release-difference pair
    constexpr float kEps = 1e-6f;
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float mag = std::fabs(l) > std::fabs(r) ? std::fabs(l) : std::fabs(r);

        // Attack pair: both rise on an onset, the fast one faster → (fast − slow) marks the attack.
        envAttFast_ += aFast * (mag - envAttFast_);
        envAttSlow_ += aSlow * (mag - envAttSlow_);
        const float attTrans = envAttFast_ - envAttSlow_; // >0 during an onset
        const float attRatio = attTrans > 0.0f ? attTrans / (envAttSlow_ + kEps) : 0.0f;

        // Sustain pair: quick attack, then fast vs slow release → (slow − fast) marks the body/tail.
        const float cUpF = mag > envRelFast_ ? attackAtk : rFast;
        const float cUpS = mag > envRelSlow_ ? attackAtk : rSlow;
        envRelFast_ += cUpF * (mag - envRelFast_);
        envRelSlow_ += cUpS * (mag - envRelSlow_);
        const float susTrans = envRelSlow_ - envRelFast_; // >0 during the decay/body
        const float susRatio = susTrans > 0.0f ? susTrans / (envRelSlow_ + kEps) : 0.0f;

        // Combine into a single gain. attack_/sustain_ in [-1,1]; ratios are ~[0,1]. At 0/0 → gain 1.
        float gain = 1.0f + attack_ * attRatio + sustain_ * susRatio;
        if (gain < 0.05f) {
            gain = 0.05f; // never invert or fully mute
        } else if (gain > 8.0f) {
            gain = 8.0f;
        }
        stereo[2 * i] = l * gain;
        stereo[2 * i + 1] = r * gain;
    }
}

// ---- TiltEQ -----------------------------------------------------------------

void TiltEQ::reset() {
    lowL_.reset();
    highL_.reset();
    lowR_.reset();
    highR_.reset();
}

void TiltEQ::recompute(int sampleRate) {
    // Pivot around ~650 Hz: low shelf and high shelf move by ±tilt/2 in opposite directions.
    const float half = tilt_ * 0.5f;
    lowL_.setShelf(650.0f, -half, sampleRate, false);
    lowR_.setShelf(650.0f, -half, sampleRate, false);
    highL_.setShelf(650.0f, half, sampleRate, true);
    highR_.setShelf(650.0f, half, sampleRate, true);
    sr_ = sampleRate;
    dirty_ = false;
}

void TiltEQ::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    if (dirty_ || sr_ != sampleRate) {
        recompute(sampleRate);
    }
    for (int i = 0; i < frames; ++i) {
        stereo[2 * i] = highL_.process(lowL_.process(stereo[2 * i]));
        stereo[2 * i + 1] = highR_.process(lowR_.process(stereo[2 * i + 1]));
    }
}

// ---- Flanger ----------------------------------------------------------------

void Flanger::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0;
}

void Flanger::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const int maxSize = sampleRate / 50; // up to 20 ms of delay line
    if (size_ != maxSize) {
        size_ = maxSize;
        bufL_.assign(static_cast<size_t>(size_), 0.0f);
        bufR_.assign(static_cast<size_t>(size_), 0.0f);
        write_ = 0;
    }
    constexpr double kTwoPi = 6.283185307179586;
    const double phaseInc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    const float baseSamp = 1.0f * 0.001f * static_cast<float>(sampleRate); // ~1 ms floor
    const float depthSamp = depthMs_ * 0.001f * static_cast<float>(sampleRate);
    const float fb = feedback_;
    const float mix = std::clamp(mix_, 0.0f, 1.0f);

    auto readAt = [&](const std::vector<float>& buf, float delay) {
        float rp = static_cast<float>(write_) - delay;
        while (rp < 0.0f) {
            rp += static_cast<float>(size_);
        }
        const int i0 = static_cast<int>(rp) % size_;
        const int i1 = (i0 + 1) % size_;
        const float frac = rp - std::floor(rp);
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    };

    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];

        const float modL = 0.5f * (1.0f + static_cast<float>(std::sin(phase_ * kTwoPi)));
        const float modR = 0.5f * (1.0f + static_cast<float>(std::sin((phase_ + 0.25) * kTwoPi)));
        const float wetL = readAt(bufL_, baseSamp + depthSamp * modL);
        const float wetR = readAt(bufR_, baseSamp + depthSamp * modR);

        // Feed the delayed signal back into the line for the resonant comb.
        bufL_[static_cast<size_t>(write_)] = dryL + wetL * fb;
        bufR_[static_cast<size_t>(write_)] = dryR + wetR * fb;

        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;

        write_ = (write_ + 1) % size_;
        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- Bitcrusher -------------------------------------------------------------

void Bitcrusher::reset() {
    holdL_ = 0.0f;
    holdR_ = 0.0f;
    counter_ = 0;
}

void Bitcrusher::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float bits = std::clamp(bits_, 1.0f, 16.0f);
    const int step = std::max(1, static_cast<int>(downsample_));
    const float levels = std::pow(2.0f, bits);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    auto crush = [levels](float x) {
        return std::round(x * levels) / levels; // quantize to `bits` bits
    };
    for (int i = 0; i < frames; ++i) {
        if (counter_ <= 0) {
            holdL_ = crush(stereo[2 * i]);
            holdR_ = crush(stereo[2 * i + 1]);
            counter_ = step;
        }
        --counter_;
        stereo[2 * i] = stereo[2 * i] * (1.0f - mix) + holdL_ * mix;
        stereo[2 * i + 1] = stereo[2 * i + 1] * (1.0f - mix) + holdR_ * mix;
    }
}

// ---- Phaser -----------------------------------------------------------------

void Phaser::reset() {
    fbL_ = 0.0f;
    fbR_ = 0.0f;
    phase_ = 0.0;
    for (Allpass1& a : apL_) {
        a.z = 0.0f;
    }
    for (Allpass1& a : apR_) {
        a.z = 0.0f;
    }
}

void Phaser::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr double kTwoPi = 6.283185307179586;
    const double phaseInc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    const float depth = std::clamp(depth_, 0.0f, 1.0f);
    const float fb = std::clamp(feedback_, 0.0f, 0.9f);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);

    for (int i = 0; i < frames; ++i) {
        const float lfo = static_cast<float>(std::sin(phase_ * kTwoPi));
        // Sweep the all-pass coefficient across the audio band.
        const float a = 0.5f + 0.45f * depth * lfo;

        float xL = stereo[2 * i] + fbL_ * fb;
        for (Allpass1& stage : apL_) {
            xL = stage.process(xL, a);
        }
        fbL_ = xL;

        float xR = stereo[2 * i + 1] + fbR_ * fb;
        for (Allpass1& stage : apR_) {
            xR = stage.process(xR, a);
        }
        fbR_ = xR;

        stereo[2 * i] = stereo[2 * i] * (1.0f - mix) + xL * mix;
        stereo[2 * i + 1] = stereo[2 * i + 1] * (1.0f - mix) + xR * mix;

        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- LowPass ----------------------------------------------------------------

void LowPass::reset() {
    yL_ = 0.0f;
    yR_ = 0.0f;
}

void LowPass::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    const float a =
        1.0f - std::exp(-kTwoPi * std::clamp(cutoff_, 20.0f, static_cast<float>(sampleRate) * 0.49f) /
                        static_cast<float>(sampleRate));
    for (int i = 0; i < frames; ++i) {
        yL_ += a * (stereo[2 * i] - yL_);
        yR_ += a * (stereo[2 * i + 1] - yR_);
        stereo[2 * i] = yL_;
        stereo[2 * i + 1] = yR_;
    }
}

// ---- HighPass ---------------------------------------------------------------

void HighPass::reset() {
    xL_ = yL_ = 0.0f;
    xR_ = yR_ = 0.0f;
}

void HighPass::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    // One-pole high-pass: y[n] = a*(y[n-1] + x[n] - x[n-1]), a = 1/(1 + 2π·fc/sr).
    const float fc = std::clamp(cutoff_, 5.0f, static_cast<float>(sampleRate) * 0.49f);
    const float a = 1.0f / (1.0f + kTwoPi * fc / static_cast<float>(sampleRate));
    for (int i = 0; i < frames; ++i) {
        const float xl = stereo[2 * i];
        const float xr = stereo[2 * i + 1];
        yL_ = a * (yL_ + xl - xL_);
        yR_ = a * (yR_ + xr - xR_);
        xL_ = xl;
        xR_ = xr;
        stereo[2 * i] = yL_;
        stereo[2 * i + 1] = yR_;
    }
}

// ---- Compressor -------------------------------------------------------------

void Compressor::reset() {
    env_ = 0.0f;
}

void Compressor::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float atkCoef = std::exp(-1.0f / (std::max(attackMs_, 0.01f) * 0.001f * sr));
    const float relCoef = std::exp(-1.0f / (std::max(releaseMs_, 0.01f) * 0.001f * sr));
    const float makeup = dbToLin(makeupDb_);
    const float ratio = std::max(ratio_, 1.0f);

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float peak = std::max(std::fabs(l), std::fabs(r));

        // Peak-following envelope (fast attack, slow release).
        const float coef = peak > env_ ? atkCoef : relCoef;
        env_ = coef * env_ + (1.0f - coef) * peak;

        // Static gain computer in dB, with an optional soft knee around the threshold.
        const float envDb = linToDb(env_);
        const float over = envDb - thresholdDb_;
        float reductionDb = 0.0f; // output − input, in dB (≤ 0)
        if (kneeDb_ > 0.0f && 2.0f * std::fabs(over) <= kneeDb_) {
            // Within the knee: quadratic interpolation into full-ratio compression.
            const float x = over + kneeDb_ * 0.5f;
            reductionDb = (1.0f / ratio - 1.0f) * x * x / (2.0f * kneeDb_);
        } else if (over > 0.0f) {
            reductionDb = (1.0f / ratio - 1.0f) * over; // = targetDb − envDb
        }
        const float gain = dbToLin(reductionDb) * makeup;
        // Parallel/NY compression: blend the compressed signal back with the dry (mix 1 = fully
        // compressed, the classic behaviour; lower mixes keep more of the untouched transients).
        const float dry = 1.0f - mix_;
        stereo[2 * i] = l * dry + l * gain * mix_;
        stereo[2 * i + 1] = r * dry + r * gain * mix_;
    }
}

// ---- De-Esser ---------------------------------------------------------------

void DeEsser::reset() {
    lpL_ = 0.0f;
    lpR_ = 0.0f;
    env_ = 0.0f;
}

void DeEsser::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // One-pole low-pass split at the crossover; the high band is (input − low band).
    const float a = std::exp(-2.0f * 3.14159265358979f * frequency_ / sr);
    const float atkCoef = std::exp(-1.0f / (0.001f * sr));            // ~1 ms attack
    const float relCoef = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));
    const float thr = dbToLin(thresholdDb_);

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        lpL_ = a * lpL_ + (1.0f - a) * l;
        lpR_ = a * lpR_ + (1.0f - a) * r;
        const float hfL = l - lpL_;
        const float hfR = r - lpR_;

        // Follow the high band's peak (stereo-linked) and duck it when it exceeds the threshold.
        const float peak = std::max(std::fabs(hfL), std::fabs(hfR));
        const float coef = peak > env_ ? atkCoef : relCoef;
        env_ = coef * env_ + (1.0f - coef) * peak;
        float gHF = 1.0f;
        if (env_ > thr && env_ > 0.0f) {
            // Blend between no reduction and a brickwall at the threshold, scaled by `amount`.
            gHF = 1.0f - amount_ * (1.0f - thr / env_);
        }
        stereo[2 * i] = lpL_ + hfL * gHF;
        stereo[2 * i + 1] = lpR_ + hfR * gHF;
    }
}

// ---- Gate -------------------------------------------------------------------

void Gate::reset() {
    env_ = 0.0f;
    gain_ = 1.0f;
    holdCounter_ = 0;
}

void Gate::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // Fast detector attack, moderate detector release, plus gain-smoothing coefs for the gate.
    const float detCoef = std::exp(-1.0f / (0.001f * sr));                                // ~1 ms
    const float openCoef = std::exp(-1.0f / (std::max(attackMs_, 0.01f) * 0.001f * sr));
    const float closeCoef = std::exp(-1.0f / (std::max(releaseMs_, 0.01f) * 0.001f * sr));
    const float ratio = std::max(ratio_, 1.0f);
    const float floorLin = dbToLin(rangeDb_);
    const int holdSamples = static_cast<int>(holdMs_ * 0.001f * sr);

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float peak = std::max(std::fabs(l), std::fabs(r));

        // Peak-following detector (fast).
        env_ = detCoef * env_ + (1.0f - detCoef) * peak;
        const float envDb = linToDb(env_);

        // Above threshold → unity + re-arm the hold; below → hold open, then expand to the floor.
        float target = 1.0f;
        if (envDb >= thresholdDb_) {
            holdCounter_ = holdSamples;
        } else if (holdCounter_ > 0) {
            --holdCounter_; // still within the hold window: keep the gate open
        } else {
            const float reductionDb = (thresholdDb_ - envDb) * (ratio - 1.0f);
            target = dbToLin(-reductionDb);
            if (target < floorLin) {
                target = floorLin;
            }
        }

        // Smooth the gate gain (open faster, close slower).
        const float coef = target > gain_ ? openCoef : closeCoef;
        gain_ = coef * gain_ + (1.0f - coef) * target;

        stereo[2 * i] = l * gain_;
        stereo[2 * i + 1] = r * gain_;
    }
}

// ---- TapeSaturation ---------------------------------------------------------

void TapeSaturation::reset() {
    lpL_ = 0.0f;
    lpR_ = 0.0f;
}

void TapeSaturation::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // Compensate for the tanh gain so the wet level stays close to the input.
    const float norm = 1.0f / std::tanh(drive_);
    // One-pole high-cut coefficient: warmth 0 → open (~18 kHz), warmth 1 → ~3 kHz.
    const float cutHz = 18000.0f - warmth_ * 15000.0f;
    const float x = std::exp(-2.0f * 3.14159265f * cutHz / static_cast<float>(sampleRate));
    const float bias = 0.05f * warmth_;
    const float biasOut = std::tanh(bias) * norm; // steady-state DC from the asymmetry, removed below
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Asymmetric drive (small bias → even harmonics), tanh soft-knee, gain-compensated, DC-free.
        float wl = std::tanh(drive_ * l + bias) * norm - biasOut;
        float wr = std::tanh(drive_ * r + bias) * norm - biasOut;
        // Gentle high-frequency roll-off (tape top-end loss).
        lpL_ = (1.0f - x) * wl + x * lpL_;
        lpR_ = (1.0f - x) * wr + x * lpR_;
        wl = lpL_;
        wr = lpR_;
        stereo[2 * i] = l + (wl - l) * mix_;
        stereo[2 * i + 1] = r + (wr - r) * mix_;
    }
}

// ---- MonoBass ---------------------------------------------------------------

void MonoBass::reset() {
    lpL_ = 0.0f;
    lpR_ = 0.0f;
}

void MonoBass::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    const float a = 1.0f - std::exp(-kTwoPi * crossover_ / static_cast<float>(sampleRate));
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // One-pole low bands.
        lpL_ += a * (l - lpL_);
        lpR_ += a * (r - lpR_);
        const float monoLow = 0.5f * (lpL_ + lpR_);
        // Recombine mono low + stereo high (high = input − its own low band).
        stereo[2 * i] = monoLow + (l - lpL_);
        stereo[2 * i + 1] = monoLow + (r - lpR_);
    }
}

// ---- AutoPan ----------------------------------------------------------------

void AutoPan::reset() {
    phase_ = 0.0;
}

void AutoPan::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr double kTwoPi = 6.283185307179586;
    constexpr float kQuarterPi = 0.78539816f;
    const double inc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    for (int i = 0; i < frames; ++i) {
        // Pan position in [-1, 1] from the LFO, scaled by depth.
        const float pos = depth_ * static_cast<float>(std::sin(phase_ * kTwoPi));
        const float theta = (pos + 1.0f) * kQuarterPi; // 0..pi/2
        stereo[2 * i] *= std::cos(theta);
        stereo[2 * i + 1] *= std::sin(theta);
        phase_ += inc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- AutoWah ----------------------------------------------------------------

void AutoWah::reset() {
    env_ = 0.0f;
    lpL_.reset();
    lpR_.reset();
}

void AutoWah::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // Envelope-follower coefficients from the attack/release times.
    const float aAtk = 1.0f - std::exp(-1.0f / (0.001f * attackMs_ * sr));
    const float aRel = 1.0f - std::exp(-1.0f / (0.001f * releaseMs_ * sr));
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Track the peak of the stereo-summed magnitude (fast up, slow down).
        const float mag = std::fabs(l) > std::fabs(r) ? std::fabs(l) : std::fabs(r);
        const float coef = mag > env_ ? aAtk : aRel;
        env_ += coef * (mag - env_);
        // Map the envelope (clamped to unity) to a cutoff between base and base+range.
        const float e = env_ > 1.0f ? 1.0f : env_;
        const float cutoff = baseHz_ + sensitivity_ * e * rangeHz_;
        stereo[2 * i] =
            lpL_.process(l, cutoff, resonance_, sampleRate, StateVariableFilter::Mode::LowPass);
        stereo[2 * i + 1] =
            lpR_.process(r, cutoff, resonance_, sampleRate, StateVariableFilter::Mode::LowPass);
    }
}

// ---- CombResonator ----------------------------------------------------------

void CombResonator::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    writePos_ = 0;
}

void CombResonator::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // Delay-line length covers the lowest tunable pitch (20 Hz); (re)allocate on rate change.
    const int maxD = sampleRate / 20 + 4;
    if (static_cast<int>(bufL_.size()) != maxD) {
        bufL_.assign(static_cast<size_t>(maxD), 0.0f);
        bufR_.assign(static_cast<size_t>(maxD), 0.0f);
        writePos_ = 0;
    }
    int d = static_cast<int>(static_cast<float>(sampleRate) / freq_ + 0.5f);
    if (d < 1) {
        d = 1;
    }
    if (d >= maxD) {
        d = maxD - 1;
    }
    for (int i = 0; i < frames; ++i) {
        int readPos = writePos_ - d;
        if (readPos < 0) {
            readPos += maxD;
        }
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Feedback comb: output = input + feedback · (delayed output), stored back into the line.
        const float wl = l + feedback_ * bufL_[static_cast<size_t>(readPos)];
        const float wr = r + feedback_ * bufR_[static_cast<size_t>(readPos)];
        bufL_[static_cast<size_t>(writePos_)] = wl;
        bufR_[static_cast<size_t>(writePos_)] = wr;
        stereo[2 * i] = l * (1.0f - mix_) + wl * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wr * mix_;
        if (++writePos_ >= maxD) {
            writePos_ = 0;
        }
    }
}

// ---- Tremolo ----------------------------------------------------------------

void Tremolo::reset() {
    phase_ = 0.0;
}

namespace {
// Each tremolo sync division as LFO cycles per beat (a quarter note = 1 cycle per beat).
constexpr float kTremCyclesPerBeat[Tremolo::kSyncDivisions] = {
    0.25f, // 1/1
    0.5f,  // 1/2
    1.0f,  // 1/4
    2.0f,  // 1/8
    3.0f,  // 1/8T
    4.0f,  // 1/16
};
constexpr const char* kTremDivName[Tremolo::kSyncDivisions] = {
    "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16",
};
} // namespace

const char* Tremolo::syncDivisionName(int div) {
    if (div < 0 || div >= kSyncDivisions) {
        return "?";
    }
    return kTremDivName[div];
}

void Tremolo::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    // rate (Hz) = beats/second × cycles-per-beat.
    rateHz_ = static_cast<float>(bpm / 60.0 * static_cast<double>(kTremCyclesPerBeat[syncDiv_]));
}

void Tremolo::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr double kTwoPi = 6.283185307179586;
    const double inc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    for (int i = 0; i < frames; ++i) {
        // Unipolar LFO in [0,1]: 1 = full level, 0 = fully dipped.
        const float lfo = shape_ == Shape::Square
                              ? (phase_ < 0.5 ? 1.0f : 0.0f)
                              : 0.5f + 0.5f * static_cast<float>(std::sin(phase_ * kTwoPi));
        const float gain = (1.0f - depth_) + depth_ * lfo; // depth 0 → unity (transparent)
        stereo[2 * i] *= gain;
        stereo[2 * i + 1] *= gain;
        phase_ += inc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- StereoDelay ------------------------------------------------------------

void StereoDelay::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    writePos_ = 0;
}

void StereoDelay::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const int maxD = sampleRate * 2 + 4; // up to 2 s per channel
    if (static_cast<int>(bufL_.size()) != maxD) {
        bufL_.assign(static_cast<size_t>(maxD), 0.0f);
        bufR_.assign(static_cast<size_t>(maxD), 0.0f);
        writePos_ = 0;
    }
    auto delaySamples = [&](float ms) {
        int d = static_cast<int>(ms * 0.001f * static_cast<float>(sampleRate) + 0.5f);
        if (d < 1) {
            d = 1;
        }
        if (d >= maxD) {
            d = maxD - 1;
        }
        return d;
    };
    const int dl = delaySamples(leftMs_);
    const int dr = delaySamples(rightMs_);
    for (int i = 0; i < frames; ++i) {
        int rl = writePos_ - dl;
        if (rl < 0) {
            rl += maxD;
        }
        int rr = writePos_ - dr;
        if (rr < 0) {
            rr += maxD;
        }
        const float inL = stereo[2 * i];
        const float inR = stereo[2 * i + 1];
        const float echoL = bufL_[static_cast<size_t>(rl)];
        const float echoR = bufR_[static_cast<size_t>(rr)];
        // Each channel feeds its own echo back into its own line at its own time.
        bufL_[static_cast<size_t>(writePos_)] = inL + echoL * feedback_;
        bufR_[static_cast<size_t>(writePos_)] = inR + echoR * feedback_;
        stereo[2 * i] = inL * (1.0f - mix_) + echoL * mix_;
        stereo[2 * i + 1] = inR * (1.0f - mix_) + echoR * mix_;
        if (++writePos_ >= maxD) {
            writePos_ = 0;
        }
    }
}

// ---- FormantFilter ----------------------------------------------------------

void FormantFilter::reset() {
    f1L_.reset();
    f2L_.reset();
    f1R_.reset();
    f2R_.reset();
}

void FormantFilter::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // First two formant frequencies (Hz) for each vowel.
    float f1 = 800.0f, f2 = 1150.0f;
    switch (vowel_) {
    case Vowel::A: f1 = 800.0f;  f2 = 1150.0f; break;
    case Vowel::E: f1 = 400.0f;  f2 = 1700.0f; break;
    case Vowel::I: f1 = 300.0f;  f2 = 2300.0f; break;
    case Vowel::O: f1 = 450.0f;  f2 = 800.0f;  break;
    case Vowel::U: f1 = 325.0f;  f2 = 700.0f;  break;
    }
    constexpr float kQ = 5.0f; // resonant enough to make the formants sing
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float wetL = f1L_.process(l, f1, kQ, sampleRate, StateVariableFilter::Mode::BandPass) +
                           0.7f * f2L_.process(l, f2, kQ, sampleRate, StateVariableFilter::Mode::BandPass);
        const float wetR = f1R_.process(r, f1, kQ, sampleRate, StateVariableFilter::Mode::BandPass) +
                           0.7f * f2R_.process(r, f2, kQ, sampleRate, StateVariableFilter::Mode::BandPass);
        stereo[2 * i] = l * (1.0f - mix_) + wetL * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wetR * mix_;
    }
}

// ---- StereoWidener ----------------------------------------------------------

void StereoWidener::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * width_;
        stereo[2 * i] = mid + side;
        stereo[2 * i + 1] = mid - side;
    }
}

// ---- Utility ----------------------------------------------------------------

void Utility::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float g = dbToLin(gainDb_);
    const float ls = invertL_ ? -1.0f : 1.0f;
    const float rs = invertR_ ? -1.0f : 1.0f;
    for (int i = 0; i < frames; ++i) {
        float l = stereo[2 * i] * ls;
        float r = stereo[2 * i + 1] * rs;
        if (mono_) {
            const float m = 0.5f * (l + r);
            l = m;
            r = m;
        }
        stereo[2 * i] = l * g;
        stereo[2 * i + 1] = r * g;
    }
}

// ---- Limiter ----------------------------------------------------------------

void Limiter::reset() {
    std::fill(dL_.begin(), dL_.end(), 0.0f);
    std::fill(dR_.begin(), dR_.end(), 0.0f);
    std::fill(dPeak_.begin(), dPeak_.end(), 0.0f);
    widx_ = 0;
    gain_ = 1.0f;
}

void Limiter::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // (Re)size the look-ahead delay lines if the window changed.
    const int want = std::max(1, static_cast<int>(lookaheadMs_ * 0.001f * sr));
    if (want != bufLen_) {
        bufLen_ = want;
        dL_.assign(static_cast<size_t>(bufLen_), 0.0f);
        dR_.assign(static_cast<size_t>(bufLen_), 0.0f);
        dPeak_.assign(static_cast<size_t>(bufLen_), 0.0f);
        widx_ = 0;
        gain_ = 1.0f;
    }
    const float inGain = dbToLin(inputGainDb_);
    const float ceiling = dbToLin(ceilingDb_);
    const float relCoef = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i] * inGain;
        const float r = stereo[2 * i + 1] * inGain;
        const float peak = std::max(std::fabs(l), std::fabs(r));

        // The sample about to leave the delay line (delayed by the look-ahead window). Its own peak is
        // still in the buffer, so the window-max target below always accounts for it.
        const size_t oi = static_cast<size_t>(widx_);
        const float outL = dL_[oi];
        const float outR = dR_[oi];

        // Loudest peak now in flight: the outgoing sample (still in the buffer) plus every sample
        // between it and now (the buffer) plus the incoming one. Computed BEFORE overwriting the
        // oldest slot, so the outgoing sample's own peak is included — that is what makes the ceiling
        // a guarantee.
        float wmax = peak;
        for (float p : dPeak_) {
            if (p > wmax) {
                wmax = p;
            }
        }
        // Overwrite the oldest slot with the incoming sample and advance the ring.
        dL_[oi] = l;
        dR_[oi] = r;
        dPeak_[oi] = peak;
        widx_ = (widx_ + 1) % bufLen_;

        // Target gain that keeps the loudest buffered sample at or under the ceiling. Attack is
        // instant (the min snaps down); between transients the gain releases smoothly back toward the
        // target as wmax falls. gain_ never exceeds the target, so the output cannot exceed the
        // ceiling: |outL| ≤ wmax and gain_ ≤ ceiling / wmax.
        const float target = wmax > ceiling ? ceiling / wmax : 1.0f;
        const float released = gain_ + (1.0f - gain_) * relCoef;
        gain_ = std::min(released, target);

        stereo[2 * i] = outL * gain_;
        stereo[2 * i + 1] = outR * gain_;
    }
}

// ---- Stereo Enhancer --------------------------------------------------------

void StereoEnhancer::reset() {
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    widx_ = 0;
}

void StereoEnhancer::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const int maxDelay = static_cast<int>(0.040f * static_cast<float>(sampleRate)) + 2;
    if (static_cast<int>(buf_.size()) != maxDelay) {
        buf_.assign(static_cast<size_t>(maxDelay), 0.0f);
        widx_ = 0;
    }
    int delay = static_cast<int>(delayMs_ * 0.001f * static_cast<float>(sampleRate));
    if (delay < 1) {
        delay = 1;
    }
    if (delay >= maxDelay) {
        delay = maxDelay - 1;
    }
    for (int i = 0; i < frames; ++i) {
        const float r = stereo[2 * i + 1];
        // Read the right channel delayed by `delay` samples, then advance the ring.
        int ridx = widx_ - delay;
        if (ridx < 0) {
            ridx += maxDelay;
        }
        const float delayed = buf_[static_cast<size_t>(ridx)];
        buf_[static_cast<size_t>(widx_)] = r;
        widx_ = (widx_ + 1) % maxDelay;
        // Blend the delayed copy into the right channel to decorrelate it from the left (width).
        stereo[2 * i + 1] = (1.0f - amount_) * r + amount_ * delayed;
    }
}

// ---- Reverb -----------------------------------------------------------------

void Reverb::Comb::setSize(int n) {
    buf.assign(static_cast<size_t>(std::max(1, n)), 0.0f);
    idx = 0;
    store = 0.0f;
}

float Reverb::Comb::process(float in, float feedback, float damp) {
    const float y = buf[static_cast<size_t>(idx)];
    store = y * (1.0f - damp) + store * damp;
    buf[static_cast<size_t>(idx)] = in + store * feedback;
    if (++idx >= static_cast<int>(buf.size())) {
        idx = 0;
    }
    return y;
}

void Reverb::Allpass::setSize(int n) {
    buf.assign(static_cast<size_t>(std::max(1, n)), 0.0f);
    idx = 0;
}

float Reverb::Allpass::process(float in, float feedback) {
    const float b = buf[static_cast<size_t>(idx)];
    const float y = -in + b;
    buf[static_cast<size_t>(idx)] = in + b * feedback;
    if (++idx >= static_cast<int>(buf.size())) {
        idx = 0;
    }
    return y;
}

void Reverb::ensureSized(int sampleRate) {
    if (sizedFor_ == sampleRate) {
        return;
    }
    sizedFor_ = sampleRate;
    // Freeverb tunings (samples @ 44.1 kHz), scaled to the actual rate. The right channel is offset
    // by a "stereo spread" so the two sides decorrelate.
    const int combTune[kCombs] = {1116, 1188, 1277, 1356};
    const int apTune[kAllpass] = {556, 441};
    const int spread = 23;
    const double scale = static_cast<double>(sampleRate) / 44100.0;
    for (int i = 0; i < kCombs; ++i) {
        combsL_[static_cast<size_t>(i)].setSize(static_cast<int>(combTune[i] * scale));
        combsR_[static_cast<size_t>(i)].setSize(static_cast<int>((combTune[i] + spread) * scale));
    }
    for (int i = 0; i < kAllpass; ++i) {
        apsL_[static_cast<size_t>(i)].setSize(static_cast<int>(apTune[i] * scale));
        apsR_[static_cast<size_t>(i)].setSize(static_cast<int>((apTune[i] + spread) * scale));
    }
    // Pre-delay line: up to 250 ms.
    preBuf_.assign(static_cast<size_t>(sampleRate) / 4 + 1, 0.0f);
    preWrite_ = 0;
}

void Reverb::reset() {
    for (Comb& c : combsL_) {
        std::fill(c.buf.begin(), c.buf.end(), 0.0f);
        c.store = 0.0f;
    }
    for (Comb& c : combsR_) {
        std::fill(c.buf.begin(), c.buf.end(), 0.0f);
        c.store = 0.0f;
    }
    for (Allpass& a : apsL_) {
        std::fill(a.buf.begin(), a.buf.end(), 0.0f);
    }
    for (Allpass& a : apsR_) {
        std::fill(a.buf.begin(), a.buf.end(), 0.0f);
    }
    std::fill(preBuf_.begin(), preBuf_.end(), 0.0f);
    preWrite_ = 0;
}

void Reverb::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    ensureSized(sampleRate);
    // Freeze: hold the tail forever — lossless feedback, no damping, and no new input enters.
    const float feedback = freeze_ ? 1.0f : (0.7f + 0.28f * std::clamp(roomSize_, 0.0f, 1.0f));
    const float damp = freeze_ ? 0.0f : (std::clamp(damping_, 0.0f, 1.0f) * 0.4f);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    constexpr float kInputGain = 0.15f;

    // Pre-delay tap: how far back in the pre-delay line the reverb network reads its input.
    const int preSize = static_cast<int>(preBuf_.size());
    int preTap = static_cast<int>(preDelayMs_ * 0.001f * static_cast<float>(sampleRate));
    if (preTap > preSize - 1) {
        preTap = preSize - 1;
    }
    if (preTap < 0) {
        preTap = 0;
    }

    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        const float rawIn = (dryL + dryR) * kInputGain;

        // Feed the reverb from the pre-delayed input so the tail starts `preDelayMs` after the hit.
        float in = rawIn;
        if (preSize > 0) {
            preBuf_[static_cast<size_t>(preWrite_)] = rawIn;
            const int rd = (preWrite_ - preTap + preSize) % preSize;
            in = preBuf_[static_cast<size_t>(rd)];
            preWrite_ = (preWrite_ + 1) % preSize;
        }
        if (freeze_) {
            in = 0.0f; // no new signal enters while frozen; the existing tail circulates
        }

        float wetL = 0.0f;
        float wetR = 0.0f;
        for (int c = 0; c < kCombs; ++c) {
            wetL += combsL_[static_cast<size_t>(c)].process(in, feedback, damp);
            wetR += combsR_[static_cast<size_t>(c)].process(in, feedback, damp);
        }
        for (int a = 0; a < kAllpass; ++a) {
            wetL = apsL_[static_cast<size_t>(a)].process(wetL, 0.5f);
            wetR = apsR_[static_cast<size_t>(a)].process(wetR, 0.5f);
        }

        // Mid/side width on the wet tail (width 1 = unchanged).
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * width_;
        wetL = mid + side;
        wetR = mid - side;

        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;
    }
}

} // namespace maz::audio
