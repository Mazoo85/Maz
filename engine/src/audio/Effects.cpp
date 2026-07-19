#include "maz/audio/Effects.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace maz::audio {

namespace {
float dbToLin(float db) {
    return std::pow(10.0f, db / 20.0f);
}
float linToDb(float lin) {
    return 20.0f * std::log10(std::max(lin, 1e-9f));
}
// Shared modulation-LFO sync divisions (chorus / flanger) as cycles per beat, with display names.
constexpr float kModCyclesPerBeat[kModSyncDivisions] = {
    0.25f, // 1/1
    0.5f,  // 1/2
    1.0f,  // 1/4
    2.0f,  // 1/8
    3.0f,  // 1/8T
    4.0f,  // 1/16
};
constexpr const char* kModDivName[kModSyncDivisions] = {"1/1", "1/2", "1/4", "1/8", "1/8T", "1/16"};
} // namespace

const char* modSyncDivisionName(int div) {
    if (div < 0 || div >= kModSyncDivisions) {
        return "?";
    }
    return kModDivName[div];
}

float modSyncRateHz(int div, double bpm) {
    if (div < 0 || div >= kModSyncDivisions || bpm <= 0.0) {
        return 1.0f;
    }
    return static_cast<float>(bpm / 60.0 * static_cast<double>(kModCyclesPerBeat[div]));
}

// ---- Delay ------------------------------------------------------------------

void Delay::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    dampL_ = 0.0f;
    dampR_ = 0.0f;
    lcL_ = 0.0f;
    lcR_ = 0.0f;
    modPhase_ = 0.0;
    duckEnv_ = 0.0f;
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

float Delay::syncTimeMs(int div, double bpm) {
    if (div < 0 || div >= kSyncDivisions || bpm <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(60000.0 / bpm * static_cast<double>(kDivMul[div]));
}

void Delay::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    timeMs_ = syncTimeMs(syncDiv_, bpm);
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
    // Feedback low-cut: a one-pole high-pass on the feedback (0 Hz = off).
    const bool doLowCut = fbLowCutHz_ > 0.0f;
    const float aLow =
        doLowCut ? 1.0f - std::exp(-2.0f * 3.14159265358979f * fbLowCutHz_ /
                                       static_cast<float>(sampleRate))
                 : 0.0f;
    // Delay-time modulation: sweep the read tap ±modSamples with a slow LFO (analog/tape wobble).
    const bool doMod = modDepthMs_ > 0.0f;
    const float modSamples = modDepthMs_ * 0.001f * static_cast<float>(sampleRate);
    const double modInc = static_cast<double>(modRateHz_) / static_cast<double>(sampleRate);
    // Ducking: follow the dry input's peak (fast attack, slower release) and pull the wet down while
    // the dry is loud. duck_ = 0 leaves the wet untouched.
    const float duckAtk = std::exp(-1.0f / (0.005f * static_cast<float>(sampleRate))); // ~5 ms
    const float duckRel = std::exp(-1.0f / (0.150f * static_cast<float>(sampleRate))); // ~150 ms

    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        float wetL;
        float wetR;
        if (doMod) {
            // Fractional, interpolated read at a tap that wobbles with the LFO.
            const float m =
                modSamples * static_cast<float>(std::sin(modPhase_ * 6.283185307179586));
            float tapF = static_cast<float>(tap) + m;
            const float maxTap = static_cast<float>(size_ - 2);
            tapF = tapF < 1.0f ? 1.0f : (tapF > maxTap ? maxTap : tapF);
            double pos = std::fmod(static_cast<double>(write_) - static_cast<double>(tapF),
                                   static_cast<double>(size_));
            if (pos < 0.0) {
                pos += static_cast<double>(size_);
            }
            const int i0 = static_cast<int>(pos);
            const float frac = static_cast<float>(pos - static_cast<double>(i0));
            const int i1 = (i0 + 1) % size_;
            wetL = bufL_[static_cast<size_t>(i0)] * (1.0f - frac) + bufL_[static_cast<size_t>(i1)] * frac;
            wetR = bufR_[static_cast<size_t>(i0)] * (1.0f - frac) + bufR_[static_cast<size_t>(i1)] * frac;
            modPhase_ += modInc;
            if (modPhase_ >= 1.0) {
                modPhase_ -= 1.0;
            }
        } else {
            const int r = (write_ - tap + size_) % size_;
            wetL = bufL_[static_cast<size_t>(r)];
            wetR = bufR_[static_cast<size_t>(r)];
        }
        // Low-pass the fed-back signal so successive repeats lose their highs.
        dampL_ += (1.0f - dampCoef) * (wetL - dampL_);
        dampR_ += (1.0f - dampCoef) * (wetR - dampR_);
        float fbSigL = dampL_;
        float fbSigR = dampR_;
        // Low-cut (high-pass = signal − low-passed) on the feedback so repeats lose their low end.
        if (doLowCut) {
            lcL_ += aLow * (fbSigL - lcL_);
            lcR_ += aLow * (fbSigR - lcR_);
            fbSigL -= lcL_;
            fbSigR -= lcR_;
        }
        float fbL = fbSigL * fb;
        float fbR = fbSigR * fb;
        // Feedback drive: warm and self-limit the repeats with a tanh in the loop (tape/analog echo).
        // Normalised so a nominal-level signal stays ~unity; skipped at 0 so feedback is bit-identical.
        if (fbDrive_ > 0.0f) {
            const float k = 1.0f + fbDrive_ * 4.0f;
            const float kn = std::tanh(k);
            fbL = std::tanh(fbL * k) / kn;
            fbR = std::tanh(fbR * k) / kn;
        }
        if (pingPong_) {
            // Cross-feed: each channel's echo re-enters the *other* channel's line, so repeats
            // alternate L→R→L across the stereo field.
            bufL_[static_cast<size_t>(write_)] = dryL + fbR;
            bufR_[static_cast<size_t>(write_)] = dryR + fbL;
        } else {
            bufL_[static_cast<size_t>(write_)] = dryL + fbL;
            bufR_[static_cast<size_t>(write_)] = dryR + fbR;
        }
        // Ducking: scale the wet by the (inverted) dry-level envelope before the mix.
        float duckGain = 1.0f;
        if (duck_ > 0.0f) {
            const float peak = std::max(std::fabs(dryL), std::fabs(dryR));
            const float coef = peak > duckEnv_ ? duckAtk : duckRel;
            duckEnv_ = coef * duckEnv_ + (1.0f - coef) * peak;
            duckGain = 1.0f - duck_ * std::min(1.0f, duckEnv_);
            if (duckGain < 0.0f) {
                duckGain = 0.0f;
            }
        }
        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix * duckGain;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix * duckGain;
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
        // Carrier waveform: a sine is the classic two-sideband ring mod; the other shapes carry many
        // harmonics, each of which mirrors the input into a fresh sideband pair for a brighter clang.
        float carrier;
        switch (carrier_) {
        case Carrier::Square:
            carrier = phase_ < 0.5 ? 1.0f : -1.0f;
            break;
        case Carrier::Saw:
            carrier = static_cast<float>(2.0 * phase_ - 1.0);
            break;
        case Carrier::Triangle:
            carrier = static_cast<float>(4.0 * std::fabs(phase_ - 0.5) - 1.0);
            break;
        case Carrier::Sine:
        default:
            carrier = static_cast<float>(std::sin(phase_ * kTwoPi));
            break;
        }
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
    const float outGain = dbToLin(outputDb_); // post-shaper output trim (1.0 at 0 dB)
    constexpr float kPi = 3.14159265f;
    // Post tone: a one-pole low-pass on the wet signal (off at 20 kHz).
    const bool doTone = toneHz_ < 19000.0f;
    const float toneA =
        doTone ? 1.0f - std::exp(-2.0f * kPi * toneHz_ / static_cast<float>(sampleRate)) : 0.0f;
    // Bias/asymmetry: offset the shaper input, then remove the resulting DC. Engaged only when set,
    // so bias 0 stays bit-for-bit identical to the classic symmetric behaviour.
    const bool doBias = bias_ != 0.0f;
    const float biasOffset = bias_ * 2.0f; // shift in the (post-drive) shaper-input domain
    const float dcR = std::exp(-2.0f * kPi * 20.0f / static_cast<float>(sampleRate)); // ~20 Hz blocker
    const int n = frames * 2;
    for (int i = 0; i < n; ++i) {
        const float dry = stereo[i];
        const float x = dry * drive + (doBias ? biasOffset : 0.0f);
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
        case Curve::Tube: {
            // Asymmetric soft clip: the positive half saturates fully, the negative half is scaled
            // down, so the two halves differ — that asymmetry adds even harmonics (warmth) and a
            // small DC offset, the tube/valve character.
            const float t = std::tanh(x) * tanhNorm;
            wet = x >= 0.0f ? t : 0.6f * t;
            break;
        }
        }
        // Bias DC blocker: strip the offset the asymmetry introduced so the output stays centred.
        if (doBias) {
            if ((i & 1) == 0) {
                dcHpL_ = wet - dcPrevL_ + dcR * dcHpL_;
                dcPrevL_ = wet;
                wet = dcHpL_;
            } else {
                dcHpR_ = wet - dcPrevR_ + dcR * dcHpR_;
                dcPrevR_ = wet;
                wet = dcHpR_;
            }
        }
        // Post tone: low-pass the wet before mixing (per channel: even i = L, odd i = R).
        if (doTone) {
            if ((i & 1) == 0) {
                toneL_ += toneA * (wet - toneL_);
                wet = toneL_;
            } else {
                toneR_ += toneA * (wet - toneR_);
                wet = toneR_;
            }
        }
        stereo[i] = (dry * (1.0f - mix) + wet * mix) * outGain;
    }
}

void Distortion::reset() {
    toneL_ = 0.0f;
    toneR_ = 0.0f;
    dcPrevL_ = dcPrevR_ = 0.0f;
    dcHpL_ = dcHpR_ = 0.0f;
}

// ---- AmpCab -----------------------------------------------------------------

void AmpCab::reset() {
    lcL_ = lcR_ = toneLpL_ = toneLpR_ = 0.0f;
    presL_.reset();
    presR_.reset();
}

void AmpCab::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float preGain = 1.0f + drive_ * 24.0f;      // preamp gain into the tanh
    const float norm = 1.0f / std::tanh(preGain);     // keep the driven level roughly unity
    const float aLc = 1.0f - std::exp(-2.0f * 3.14159265358979f * 90.0f / sr);   // low-cut ~90 Hz
    const float aTone = 1.0f - std::exp(-2.0f * 3.14159265358979f * toneHz_ / sr); // cabinet high-cut
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        auto amp = [&](float x, float& lc, float& toneLp, StateVariableFilter& pres) {
            // Preamp overdrive.
            float d = std::tanh(x * preGain) * norm;
            // Cabinet low-cut (shed flub): high-pass = x − low-passed.
            lc += aLc * (d - lc);
            d -= lc;
            // Presence: a mid bite bump around 2.5 kHz added back in.
            const float bp = pres.process(d, 2500.0f, 1.2f, sampleRate,
                                          StateVariableFilter::Mode::BandPass);
            d += presence_ * bp;
            // Cabinet high-cut (speaker top-end rolloff).
            toneLp += aTone * (d - toneLp);
            return toneLp;
        };
        const float wetL = amp(l, lcL_, toneLpL_, presL_);
        const float wetR = amp(r, lcR_, toneLpR_, presR_);
        stereo[2 * i] = l * (1.0f - mix_) + wetL * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wetR * mix_;
    }
}

// ---- PitchShifter -----------------------------------------------------------

void PitchShifter::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    writePos_ = 0;
    phase_ = 0.0;
    fbL_ = 0.0f;
    fbR_ = 0.0f;
}

void PitchShifter::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const int bufLen = sampleRate / 20; // ~50 ms window
    if (size_ != bufLen) {
        size_ = bufLen;
        bufL_.assign(static_cast<size_t>(size_), 0.0f);
        bufR_.assign(static_cast<size_t>(size_), 0.0f);
        writePos_ = 0;
        phase_ = 0.0;
    }
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    const float ratio = std::pow(2.0f, semitones_ / 12.0f);
    // The tap delay sweeps so the read advances at `ratio` samples per output sample.
    const double inc = (1.0 - static_cast<double>(ratio)) / static_cast<double>(size_);
    constexpr double kPi = 3.14159265358979;

    auto readTap = [&](const std::vector<float>& buf, double p) {
        const double delay = p * static_cast<double>(size_ - 1);
        double rp = static_cast<double>(writePos_) - delay;
        while (rp < 0.0) {
            rp += static_cast<double>(size_);
        }
        const int i0 = static_cast<int>(rp) % size_;
        const int i1 = (i0 + 1) % size_;
        const float frac = static_cast<float>(rp - std::floor(rp));
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    };

    const float fb = feedback_; // route the shifted output back in for cascading shifts
    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        // Feed the previous shifted output back into the delay line so each pass shifts again.
        bufL_[static_cast<size_t>(writePos_)] = dryL + fbL_ * fb;
        bufR_[static_cast<size_t>(writePos_)] = dryR + fbR_ * fb;

        // Two taps half the buffer apart, each windowed by sin(pi·p) so one fades in as the other
        // wraps — hiding the discontinuity when a tap laps the buffer.
        double p0 = phase_ - std::floor(phase_);
        double p1 = phase_ + 0.5;
        p1 -= std::floor(p1);
        const float w0 = static_cast<float>(std::sin(kPi * p0));
        const float w1 = static_cast<float>(std::sin(kPi * p1));
        const float wetL = readTap(bufL_, p0) * w0 + readTap(bufL_, p1) * w1;
        const float wetR = readTap(bufR_, p0) * w0 + readTap(bufR_, p1) * w1;
        fbL_ = wetL;
        fbR_ = wetR;

        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;

        if (++writePos_ >= size_) {
            writePos_ = 0;
        }
        phase_ += inc;
        phase_ -= std::floor(phase_);
    }
}

// ---- FrequencyShifter -------------------------------------------------------

void FrequencyShifter::reset() {
    dlL_.fill(0.0f);
    dlR_.fill(0.0f);
    wp_ = 0;
    phase_ = 0.0;
}

void FrequencyShifter::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr double kTwoPi = 6.283185307179586;
    constexpr double kPi = 3.141592653589793;
    constexpr int N = kTaps;
    constexpr int C = (N - 1) / 2;
    // Blackman-windowed Hilbert FIR (90° phase shift; odd-symmetric, even taps zero). Built once.
    static const std::array<float, N> h = [] {
        std::array<float, N> t{};
        for (int n = 0; n < N; ++n) {
            const int k = n - C;
            if (k % 2 == 0) {
                t[static_cast<size_t>(n)] = 0.0f;
            } else {
                const double s = 2.0 / (kPi * k);
                const double w = 0.42 - 0.5 * std::cos(kTwoPi * n / (N - 1)) +
                                 0.08 * std::cos(2.0 * kTwoPi * n / (N - 1));
                t[static_cast<size_t>(n)] = static_cast<float>(s * w);
            }
        }
        return t;
    }();

    const double inc = kTwoPi * static_cast<double>(shiftHz_) / static_cast<double>(sampleRate);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        dlL_[static_cast<size_t>(wp_)] = dryL;
        dlR_[static_cast<size_t>(wp_)] = dryR;
        // Quadrature (90°) component via the FIR; in-phase component is the matching group delay.
        float qL = 0.0f, qR = 0.0f;
        for (int t = 0; t < N; ++t) {
            const int idx = (wp_ - t + N) % N;
            qL += h[static_cast<size_t>(t)] * dlL_[static_cast<size_t>(idx)];
            qR += h[static_cast<size_t>(t)] * dlR_[static_cast<size_t>(idx)];
        }
        const float iL = dlL_[static_cast<size_t>((wp_ - C + N) % N)];
        const float iR = dlR_[static_cast<size_t>((wp_ - C + N) % N)];
        const float cs = static_cast<float>(std::cos(phase_));
        const float sn = static_cast<float>(std::sin(phase_));
        // Heterodyne: (I + jQ)·e^{jωt} keeps only the shifted sideband → single-sideband shift.
        const float wetL = iL * cs - qL * sn;
        const float wetR = iR * cs - qR * sn;
        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;
        wp_ = (wp_ + 1) % N;
        phase_ += inc;
        if (phase_ >= kTwoPi) {
            phase_ -= kTwoPi;
        } else if (phase_ < 0.0) {
            phase_ += kTwoPi;
        }
    }
}

// ---- Chorus -----------------------------------------------------------------

void Chorus::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0;
}

void Chorus::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    rateHz_ = modSyncRateHz(syncDiv_, bpm);
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
    const float fb = std::clamp(feedback_, 0.0f, 0.9f);

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

        // Sum `voices_` modulated taps spread evenly across the LFO cycle (an ensemble chorus). With
        // one voice this reduces to a single quadrature tap — bit-for-bit the original behaviour.
        float wetL;
        float wetR;
        if (voices_ <= 1) {
            const float modL = static_cast<float>(std::sin(phase_ * kTwoPi));
            const float modR = static_cast<float>(std::sin((phase_ + 0.25) * kTwoPi)); // quadrature
            wetL = readAt(bufL_, baseSamp + depthSamp * modL);
            wetR = readAt(bufR_, baseSamp + depthSamp * modR);
        } else {
            float sumL = 0.0f, sumR = 0.0f;
            for (int v = 0; v < voices_; ++v) {
                const double ph = phase_ + static_cast<double>(v) / static_cast<double>(voices_);
                const float modL = static_cast<float>(std::sin(ph * kTwoPi));
                const float modR = static_cast<float>(std::sin((ph + 0.25) * kTwoPi));
                sumL += readAt(bufL_, baseSamp + depthSamp * modL);
                sumR += readAt(bufR_, baseSamp + depthSamp * modR);
            }
            const float invV = 1.0f / static_cast<float>(voices_);
            wetL = sumL * invV;
            wetR = sumR * invV;
        }

        // Feedback: mix the wet output back into the delay lines (0 = clean chorus).
        bufL_[static_cast<size_t>(write_)] = dryL + wetL * fb;
        bufR_[static_cast<size_t>(write_)] = dryR + wetR * fb;

        // Stereo width: mid/side-scale the wet (0 = mono wet, 1 = natural, 2 = extra-wide).
        const float wMid = 0.5f * (wetL + wetR);
        const float wSide = 0.5f * (wetL - wetR) * width_;
        const float outWetL = wMid + wSide;
        const float outWetR = wMid - wSide;
        stereo[2 * i] = dryL * (1.0f - mix) + outWetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + outWetR * mix;

        write_ = (write_ + 1) % size_;
        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- Vibrato ----------------------------------------------------------------

void Vibrato::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0;
}

void Vibrato::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    rateHz_ = modSyncRateHz(syncDiv_, bpm);
}

void Vibrato::process(float* stereo, int frames, int sampleRate) {
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
    const float depthSamp =
        std::clamp(depthMs_, 0.0f, 20.0f) * 0.001f * static_cast<float>(sampleRate);
    // Centre the swept read one depth + 1 ms back, so the modulated delay never reads past the write
    // head (rp stays positive) and swings symmetrically around the centre.
    const float baseSamp = depthSamp + 0.001f * static_cast<float>(sampleRate);

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
        bufL_[static_cast<size_t>(write_)] = stereo[2 * i];
        bufR_[static_cast<size_t>(write_)] = stereo[2 * i + 1];

        const float mod = static_cast<float>(std::sin(phase_ * kTwoPi));
        // Fully wet: the output is the pitch-modulated (swept-delay) copy only, no dry blend.
        stereo[2 * i] = readAt(bufL_, baseSamp + depthSamp * mod);
        stereo[2 * i + 1] = readAt(bufR_, baseSamp + depthSamp * mod);

        write_ = (write_ + 1) % size_;
        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- Rotary (Leslie) --------------------------------------------------------

void Rotary::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0;
}

void Rotary::process(float* stereo, int frames, int sampleRate) {
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
    // Doppler sweep depth (up to ~3 ms) and the amplitude-modulation depth both scale with `depth`.
    const float dopSamp = std::clamp(depth_, 0.0f, 1.0f) * 3.0f * 0.001f * static_cast<float>(sampleRate);
    const float baseSamp = dopSamp + 0.001f * static_cast<float>(sampleRate);
    const float amDepth = std::clamp(depth_, 0.0f, 1.0f);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    // Tube-preamp drive on the wet path; skipped at 0 so the rotary is bit-for-bit unchanged.
    const bool doDrive = drive_ > 0.0f;
    const float driveK = 1.0f + drive_ * 8.0f;
    const float driveNorm = doDrive ? 1.0f / std::tanh(driveK) : 1.0f;

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

        const double a = phase_ * kTwoPi;
        const float modL = static_cast<float>(std::sin(a));
        const float modR = -modL; // the two mics sit ~180° apart around the rotor
        // Doppler: sweep each channel's read delay with the rotation (pitch wobble).
        float wetL = readAt(bufL_, baseSamp + dopSamp * modL);
        float wetR = readAt(bufR_, baseSamp + dopSamp * modR);
        // Amplitude modulation: the horn is loudest facing each mic (opposite phases per side).
        const float cosA = static_cast<float>(std::cos(a));
        const float amL = 1.0f - amDepth * 0.5f * (1.0f - cosA);
        const float amR = 1.0f - amDepth * 0.5f * (1.0f + cosA);
        wetL *= amL;
        wetR *= amR;
        // Leslie tube-preamp overdrive on the rotating signal.
        if (doDrive) {
            wetL = std::tanh(wetL * driveK) * driveNorm;
            wetR = std::tanh(wetR * driveK) * driveNorm;
        }
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
void ParametricEQ::setMid2(float freq, float q, float db) {
    mid2Freq_ = freq;
    mid2Q_ = q;
    mid2Db_ = db;
    dirty_ = true;
}
void ParametricEQ::setMid3(float freq, float q, float db) {
    mid3Freq_ = freq;
    mid3Q_ = q;
    mid3Db_ = db;
    dirty_ = true;
}
void ParametricEQ::setHighGain(float db) {
    highDb_ = db;
    dirty_ = true;
}

void ParametricEQ::reset() {
    lowL_.reset();
    midL_.reset();
    mid2L_.reset();
    mid3L_.reset();
    highL_.reset();
    lowR_.reset();
    midR_.reset();
    mid2R_.reset();
    mid3R_.reset();
    highR_.reset();
}

void ParametricEQ::recompute(int sampleRate) {
    lowL_.setShelf(120.0f, lowDb_, sampleRate, false);
    lowR_.setShelf(120.0f, lowDb_, sampleRate, false);
    midL_.setPeaking(midFreq_, midQ_, midDb_, sampleRate);
    midR_.setPeaking(midFreq_, midQ_, midDb_, sampleRate);
    mid2L_.setPeaking(mid2Freq_, mid2Q_, mid2Db_, sampleRate);
    mid2R_.setPeaking(mid2Freq_, mid2Q_, mid2Db_, sampleRate);
    mid3L_.setPeaking(mid3Freq_, mid3Q_, mid3Db_, sampleRate);
    mid3R_.setPeaking(mid3Freq_, mid3Q_, mid3Db_, sampleRate);
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
        stereo[2 * i] =
            highL_.process(mid3L_.process(mid2L_.process(midL_.process(lowL_.process(stereo[2 * i])))));
        stereo[2 * i + 1] = highR_.process(
            mid3R_.process(mid2R_.process(midR_.process(lowR_.process(stereo[2 * i + 1])))));
    }
}

// ---- Exciter ----------------------------------------------------------------

void Exciter::reset() {
    lpL_ = 0.0f;
    lpR_ = 0.0f;
    dcPrevL_ = dcPrevR_ = 0.0f;
    dcHpL_ = dcHpR_ = 0.0f;
}

void Exciter::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    const float a = 1.0f - std::exp(-kTwoPi * crossover_ / static_cast<float>(sampleRate));
    // Even mode uses a squaring cell (→ a 2nd-harmonic octave), DC-blocked so it stays centred.
    const float dcR = std::exp(-kTwoPi * 20.0f / static_cast<float>(sampleRate));
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        lpL_ += a * (l - lpL_);
        lpR_ += a * (r - lpR_);
        const float hiL = l - lpL_;
        const float hiR = r - lpR_;
        float excL, excR;
        if (evenMode_) {
            // Squaring the high band makes a 2nd harmonic (+ DC); the blocker removes the DC.
            const float sqL = hiL * hiL * 6.0f;
            const float sqR = hiR * hiR * 6.0f;
            dcHpL_ = sqL - dcPrevL_ + dcR * dcHpL_;
            dcPrevL_ = sqL;
            dcHpR_ = sqR - dcPrevR_ + dcR * dcHpR_;
            dcPrevR_ = sqR;
            excL = dcHpL_ * amount_;
            excR = dcHpR_ * amount_;
        } else {
            // Odd harmonics from the high band via a tanh saturator.
            excL = std::tanh(hiL * 3.0f) * amount_;
            excR = std::tanh(hiR * 3.0f) * amount_;
        }
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

// ---- Multiband Transient Shaper ---------------------------------------------

void MultibandTransientShaper::reset() {
    lp1L_ = lp1R_ = lp2L_ = lp2R_ = 0.0f;
    for (int b = 0; b < kBands; ++b) {
        attF_[b] = attS_[b] = relF_[b] = relS_[b] = 0.0f;
    }
}

void MultibandTransientShaper::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    auto coef = [sr](float ms) { return 1.0f - std::exp(-1.0f / (0.001f * ms * sr)); };
    const float aFast = coef(0.5f), aSlow = coef(15.0f);   // attack detector pair
    const float rFast = coef(40.0f), rSlow = coef(300.0f); // sustain detector release pair
    const float relAtk = coef(1.0f);                       // shared quick attack for the release pair
    constexpr float kEps = 1e-6f;
    const float lo = std::min(crossLow_, crossHigh_);
    const float hi = std::max(crossLow_, crossHigh_);
    const float a1 = std::exp(-2.0f * 3.14159265358979f * lo / sr);
    const float a2 = std::exp(-2.0f * 3.14159265358979f * hi / sr);
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Exact-reconstruction one-pole split (low + mid + high == input).
        lp1L_ = a1 * lp1L_ + (1.0f - a1) * l;
        lp1R_ = a1 * lp1R_ + (1.0f - a1) * r;
        lp2L_ = a2 * lp2L_ + (1.0f - a2) * l;
        lp2R_ = a2 * lp2R_ + (1.0f - a2) * r;
        const float bandsL[kBands] = {lp1L_, lp2L_ - lp1L_, l - lp2L_};
        const float bandsR[kBands] = {lp1R_, lp2R_ - lp1R_, r - lp2R_};
        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < kBands; ++b) {
            if (attack_[b] == 0.0f && sustain_[b] == 0.0f) {
                outL += bandsL[b];
                outR += bandsR[b];
                continue;
            }
            const float mag = std::max(std::fabs(bandsL[b]), std::fabs(bandsR[b]));
            // Attack: fast follower leads the slow one on an onset.
            attF_[b] += aFast * (mag - attF_[b]);
            attS_[b] += aSlow * (mag - attS_[b]);
            const float attTrans = attF_[b] - attS_[b];
            const float attRatio = attTrans > 0.0f ? attTrans / (attS_[b] + kEps) : 0.0f;
            // Sustain: quick attack, then fast vs slow release → slow lags on the body/tail.
            const float cUpF = mag > relF_[b] ? relAtk : rFast;
            const float cUpS = mag > relS_[b] ? relAtk : rSlow;
            relF_[b] += cUpF * (mag - relF_[b]);
            relS_[b] += cUpS * (mag - relS_[b]);
            const float susTrans = relS_[b] - relF_[b];
            const float susRatio = susTrans > 0.0f ? susTrans / (relS_[b] + kEps) : 0.0f;
            float gain = 1.0f + attack_[b] * attRatio + sustain_[b] * susRatio;
            gain = gain < 0.05f ? 0.05f : (gain > 8.0f ? 8.0f : gain);
            outL += bandsL[b] * gain;
            outR += bandsR[b] * gain;
        }
        stereo[2 * i] = outL;
        stereo[2 * i + 1] = outR;
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
    // Pivot around pivot_ Hz: low shelf and high shelf move by ±tilt/2 in opposite directions.
    const float half = tilt_ * 0.5f;
    lowL_.setShelf(pivot_, -half, sampleRate, false);
    lowR_.setShelf(pivot_, -half, sampleRate, false);
    highL_.setShelf(pivot_, half, sampleRate, true);
    highR_.setShelf(pivot_, half, sampleRate, true);
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

void Flanger::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    rateHz_ = modSyncRateHz(syncDiv_, bpm);
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

        // Invert flips the wet polarity before the mix, so the comb's peaks become notches — a
        // hollow, through-zero flange that deeply cancels when the delay is short.
        const float wetGain = invert_ ? -mix : mix;
        stereo[2 * i] = dryL * (1.0f - mix) + wetL * wetGain;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * wetGain;

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
    toneL_ = 0.0f;
    toneR_ = 0.0f;
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
    // Post tone: a one-pole low-pass on the crushed (wet) signal (off at 20 kHz).
    const bool doTone = toneHz_ < 19000.0f;
    const float toneA =
        doTone ? 1.0f - std::exp(-2.0f * 3.14159265f * toneHz_ / static_cast<float>(sampleRate)) : 0.0f;
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
        float wetL = holdL_;
        float wetR = holdR_;
        if (doTone) {
            toneL_ += toneA * (wetL - toneL_);
            wetL = toneL_;
            toneR_ += toneA * (wetR - toneR_);
            wetR = toneR_;
        }
        stereo[2 * i] = stereo[2 * i] * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = stereo[2 * i + 1] * (1.0f - mix) + wetR * mix;
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

void Phaser::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    rateHz_ = modSyncRateHz(syncDiv_, bpm);
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
        // Sweep the all-pass coefficient across the audio band. In stereo mode the right channel's
        // LFO leads by 90° (a quarter cycle) so the two channels sweep out of step.
        const float aL = 0.5f + 0.45f * depth * lfo;
        const float aR =
            stereo_ ? 0.5f + 0.45f * depth * static_cast<float>(std::sin((phase_ + 0.25) * kTwoPi))
                    : aL;

        float xL = stereo[2 * i] + fbL_ * fb;
        for (int s = 0; s < stages_; ++s) {
            xL = apL_[static_cast<size_t>(s)].process(xL, aL);
        }
        fbL_ = xL;

        float xR = stereo[2 * i + 1] + fbR_ * fb;
        for (int s = 0; s < stages_; ++s) {
            xR = apR_[static_cast<size_t>(s)].process(xR, aR);
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

// ---- MasterFilter (resonant DJ filter) --------------------------------------

void MasterFilter::reset() {
    fL_.reset();
    fR_.reset();
}

void MasterFilter::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // Drive: overdrive into the filter with a tanh saturation, normalised so full-scale stays ~unity.
    // Skipped at 0 so the clean filter is bit-for-bit unchanged.
    const bool doDrive = drive_ > 0.0f;
    const float k = 1.0f + drive_ * 8.0f;
    const float kNorm = doDrive ? 1.0f / std::tanh(k) : 1.0f;
    for (int i = 0; i < frames; ++i) {
        float l = stereo[2 * i];
        float r = stereo[2 * i + 1];
        if (doDrive) {
            l = std::tanh(l * k) * kNorm;
            r = std::tanh(r * k) * kNorm;
        }
        stereo[2 * i] = fL_.process(l, cutoff_, reso_, sampleRate, mode_);
        stereo[2 * i + 1] = fR_.process(r, cutoff_, reso_, sampleRate, mode_);
    }
}

// ---- Compressor -------------------------------------------------------------

void Compressor::reset() {
    env_ = 0.0f;
    rmsEnv_ = 0.0f;
    envL_ = envR_ = 0.0f;
    rmsEnvL_ = rmsEnvR_ = 0.0f;
    scLpL_ = 0.0f;
    scLpR_ = 0.0f;
    grDb_ = 0.0f;
    std::fill(laBufL_.begin(), laBufL_.end(), 0.0f);
    std::fill(laBufR_.begin(), laBufR_.end(), 0.0f);
    laPos_ = 0;
}

void Compressor::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float atkCoef = std::exp(-1.0f / (std::max(attackMs_, 0.01f) * 0.001f * sr));
    const float relCoef = std::exp(-1.0f / (std::max(releaseMs_, 0.01f) * 0.001f * sr));
    const float rmsCoef = std::exp(-1.0f / (0.010f * sr)); // ~10 ms RMS averaging window
    const float makeup = dbToLin(effectiveMakeupDb());
    const float ratio = std::max(ratio_, 1.0f);
    // Sidechain high-pass on the detection signal only (0 = off): removes lows from what drives the
    // gain reduction, so bass/kick don't pump the compressor.
    const bool scHpf = scHpfHz_ > 0.0f;
    const float scA = scHpf ? 1.0f - std::exp(-6.283185307179586f * scHpfHz_ / sr) : 0.0f;
    float grPeak = 0.0f; // most negative reduction this block (for the GR meter)

    // Lookahead: size the audio delay lines so the detector reads `lookaheadMs` ahead of the output.
    const int laLen = static_cast<int>(lookaheadMs_ * 0.001f * sr);
    if (laLen != laLen_) {
        laLen_ = laLen;
        laBufL_.assign(static_cast<size_t>(laLen > 0 ? laLen : 0), 0.0f);
        laBufR_.assign(static_cast<size_t>(laLen > 0 ? laLen : 0), 0.0f);
        laPos_ = 0;
    }
    const bool useLA = laLen_ > 0;

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Lookahead: the gain (computed from the current, un-delayed detection below) is applied to an
        // earlier sample, so by the time a transient reaches the output the reduction is engaged.
        float outL = l, outR = r; // the (possibly delayed) audio the gain is applied to
        if (useLA) {
            outL = laBufL_[static_cast<size_t>(laPos_)];
            outR = laBufR_[static_cast<size_t>(laPos_)];
            laBufL_[static_cast<size_t>(laPos_)] = l;
            laBufR_[static_cast<size_t>(laPos_)] = r;
            laPos_ = (laPos_ + 1) % laLen_;
        }
        float dl = l, dr = r; // detection signal
        if (scHpf) {
            scLpL_ += scA * (l - scLpL_);
            scLpR_ += scA * (r - scLpR_);
            dl = l - scLpL_; // high-passed
            dr = r - scLpR_;
        }
        // The gain computer: from a detection level and its own envelope state, advance the follower
        // and return the makeup-scaled linear gain (also tracking the deepest reduction for the meter).
        auto computeGain = [&](float level, float& env) {
            const float coef = level > env ? atkCoef : relCoef;
            env = coef * env + (1.0f - coef) * level;
            const float envDb = linToDb(env);
            const float over = envDb - thresholdDb_;
            float reductionDb = 0.0f; // output − input, in dB (≤ 0)
            if (kneeDb_ > 0.0f && 2.0f * std::fabs(over) <= kneeDb_) {
                // Within the knee: quadratic interpolation into full-ratio compression.
                const float x = over + kneeDb_ * 0.5f;
                reductionDb = (1.0f / ratio - 1.0f) * x * x / (2.0f * kneeDb_);
            } else if (over > 0.0f) {
                reductionDb = (1.0f / ratio - 1.0f) * over; // = targetDb − envDb
            }
            if (reductionDb < grPeak) {
                grPeak = reductionDb; // track the deepest reduction for the GR meter
            }
            return dbToLin(reductionDb) * makeup;
        };

        float gainL, gainR;
        if (stereoLink_) {
            // Linked: one shared detector from the max (peak) or summed power (RMS) of both channels.
            float level;
            if (rmsMode_) {
                const float ms = 0.5f * (dl * dl + dr * dr);
                rmsEnv_ = rmsCoef * rmsEnv_ + (1.0f - rmsCoef) * ms;
                level = std::sqrt(rmsEnv_);
            } else {
                level = std::max(std::fabs(dl), std::fabs(dr));
            }
            gainL = gainR = computeGain(level, env_);
        } else {
            // Unlinked: each channel detects and reduces on its own.
            float levelL, levelR;
            if (rmsMode_) {
                rmsEnvL_ = rmsCoef * rmsEnvL_ + (1.0f - rmsCoef) * dl * dl;
                rmsEnvR_ = rmsCoef * rmsEnvR_ + (1.0f - rmsCoef) * dr * dr;
                levelL = std::sqrt(rmsEnvL_);
                levelR = std::sqrt(rmsEnvR_);
            } else {
                levelL = std::fabs(dl);
                levelR = std::fabs(dr);
            }
            gainL = computeGain(levelL, envL_);
            gainR = computeGain(levelR, envR_);
        }
        // Parallel/NY compression: blend the compressed signal back with the dry (mix 1 = fully
        // compressed, the classic behaviour; lower mixes keep more of the untouched transients). The
        // dry is the *delayed* audio too, so dry and wet stay time-aligned under lookahead.
        const float dry = 1.0f - mix_;
        stereo[2 * i] = outL * dry + outL * gainL * mix_;
        stereo[2 * i + 1] = outR * dry + outR * gainR * mix_;
    }
    grDb_ = grPeak;
}

// ---- Multiband Compressor ---------------------------------------------------

void MultibandCompressor::reset() {
    lp1L_ = lp1R_ = lp2L_ = lp2R_ = 0.0f;
    for (int b = 0; b < kBands; ++b) {
        env_[b] = 0.0f;
    }
}

void MultibandCompressor::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // Ensure lo <= hi so the mid band is well-formed regardless of setter order.
    const float lo = std::min(crossLow_, crossHigh_);
    const float hi = std::max(crossLow_, crossHigh_);
    const float a1 = std::exp(-2.0f * 3.14159265358979f * lo / sr);
    const float a2 = std::exp(-2.0f * 3.14159265358979f * hi / sr);
    const float atkCoef = std::exp(-1.0f / (std::max(attackMs_, 0.01f) * 0.001f * sr));
    const float relCoef = std::exp(-1.0f / (std::max(releaseMs_, 0.01f) * 0.001f * sr));

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // One-pole low-pass splits; bands reconstruct the input exactly (low + mid + high == input).
        lp1L_ = a1 * lp1L_ + (1.0f - a1) * l;
        lp1R_ = a1 * lp1R_ + (1.0f - a1) * r;
        lp2L_ = a2 * lp2L_ + (1.0f - a2) * l;
        lp2R_ = a2 * lp2R_ + (1.0f - a2) * r;
        const float lowL = lp1L_, lowR = lp1R_;
        const float midL = lp2L_ - lp1L_, midR = lp2R_ - lp1R_;
        const float hiL = l - lp2L_, hiR = r - lp2R_;
        const float bandsL[kBands] = {lowL, midL, hiL};
        const float bandsR[kBands] = {lowR, midR, hiR};

        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < kBands; ++b) {
            const float peak = std::max(std::fabs(bandsL[b]), std::fabs(bandsR[b]));
            const float coef = peak > env_[b] ? atkCoef : relCoef;
            env_[b] = coef * env_[b] + (1.0f - coef) * peak;
            const float over = linToDb(env_[b]) - thr_[b];
            const float redDb = over > 0.0f ? (1.0f / std::max(ratio_[b], 1.0f) - 1.0f) * over : 0.0f;
            const float g = dbToLin(redDb); // ≤ 1 (or exactly 1 when not over / ratio 1)
            outL += bandsL[b] * g;
            outR += bandsR[b] * g;
        }
        stereo[2 * i] = outL;
        stereo[2 * i + 1] = outR;
    }
}

// ---- StereoImager -----------------------------------------------------------

void StereoImager::reset() {
    lp1L_ = lp1R_ = lp2L_ = lp2R_ = 0.0f;
}

void StereoImager::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float lo = std::min(crossLow_, crossHigh_);
    const float hi = std::max(crossLow_, crossHigh_);
    const float a1 = std::exp(-2.0f * 3.14159265358979f * lo / sr);
    const float a2 = std::exp(-2.0f * 3.14159265358979f * hi / sr);
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Same one-pole split as the multiband compressor: low + mid + high == input.
        lp1L_ = a1 * lp1L_ + (1.0f - a1) * l;
        lp1R_ = a1 * lp1R_ + (1.0f - a1) * r;
        lp2L_ = a2 * lp2L_ + (1.0f - a2) * l;
        lp2R_ = a2 * lp2R_ + (1.0f - a2) * r;
        const float bandsL[kBands] = {lp1L_, lp2L_ - lp1L_, l - lp2L_};
        const float bandsR[kBands] = {lp1R_, lp2R_ - lp1R_, r - lp2R_};
        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < kBands; ++b) {
            const float mid = 0.5f * (bandsL[b] + bandsR[b]);
            const float side = 0.5f * (bandsL[b] - bandsR[b]) * width_[b];
            outL += mid + side;
            outR += mid - side;
        }
        stereo[2 * i] = outL;
        stereo[2 * i + 1] = outR;
    }
}

// ---- Multiband Saturator ----------------------------------------------------

void MultibandSaturator::reset() {
    lp1L_ = lp1R_ = lp2L_ = lp2R_ = 0.0f;
}

void MultibandSaturator::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float lo = std::min(crossLow_, crossHigh_);
    const float hi = std::max(crossLow_, crossHigh_);
    const float a1 = std::exp(-2.0f * 3.14159265358979f * lo / sr);
    const float a2 = std::exp(-2.0f * 3.14159265358979f * hi / sr);
    // Per-band saturation: dry/wet-blend a tanh-shaped copy by the band's drive so drive 0 is a bit-
    // exact bypass. The drive scales the pre-gain into the tanh (harder → more harmonics), and the
    // shaped signal is normalised by tanh(gain) so the band's level stays roughly constant.
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Same one-pole split as the multiband compressor / imager: low + mid + high == input.
        lp1L_ = a1 * lp1L_ + (1.0f - a1) * l;
        lp1R_ = a1 * lp1R_ + (1.0f - a1) * r;
        lp2L_ = a2 * lp2L_ + (1.0f - a2) * l;
        lp2R_ = a2 * lp2R_ + (1.0f - a2) * r;
        const float bandsL[kBands] = {lp1L_, lp2L_ - lp1L_, l - lp2L_};
        const float bandsR[kBands] = {lp1R_, lp2R_ - lp1R_, r - lp2R_};
        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < kBands; ++b) {
            const float d = drive_[b];
            if (d <= 0.0f) {
                outL += bandsL[b];
                outR += bandsR[b];
                continue;
            }
            const float gain = 1.0f + d * 6.0f; // pre-gain into the shaper; more drive → more grit
            const float norm = 1.0f / std::tanh(gain);
            const float shapedL = std::tanh(bandsL[b] * gain) * norm;
            const float shapedR = std::tanh(bandsR[b] * gain) * norm;
            outL += bandsL[b] * (1.0f - d) + shapedL * d;
            outR += bandsR[b] * (1.0f - d) + shapedR * d;
        }
        stereo[2 * i] = outL;
        stereo[2 * i + 1] = outR;
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

// ---- Dynamic EQ -------------------------------------------------------------

void DynamicEq::reset() {
    bpL_.reset();
    bpR_.reset();
    env_ = 0.0f;
}

void DynamicEq::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0 || rangeDb_ == 0.0f) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float atkCoef = std::exp(-1.0f / (attackMs_ * 0.001f * sr));
    const float relCoef = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));
    const float thr = dbToLin(thresholdDb_);
    constexpr float kKneeDb = 12.0f; // dB above threshold over which the band moves to full `range`
    // This SVF's band-pass peaks at ~Q at resonance; normalise it to unity peak so that adding
    // g * bandpass to the dry signal gives a peak bell gain of exactly (1 + g) — otherwise a large
    // cut over-subtracts (and can flip sign). The band level then also reads ~= the in-band input.
    const float bpNorm = 1.0f / std::max(q_, 0.5f);
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Band-pass extracts the band; adding a scaled copy to the dry signal is a peaking bell
        // (g > 0 boosts, g < 0 cuts). The same band-pass keys the envelope follower.
        const float bpL = bpL_.process(l, frequency_, q_, sampleRate, StateVariableFilter::Mode::BandPass) * bpNorm;
        const float bpR = bpR_.process(r, frequency_, q_, sampleRate, StateVariableFilter::Mode::BandPass) * bpNorm;
        const float peak = std::max(std::fabs(bpL), std::fabs(bpR));
        const float coef = peak > env_ ? atkCoef : relCoef;
        env_ = coef * env_ + (1.0f - coef) * peak;
        float g = 0.0f; // extra band gain factor (0 = flat); out = dry + g * bandpass
        if (env_ > thr && env_ > 0.0f) {
            // How far over threshold, in dB, mapped 0..1 across the knee → engagement.
            const float overDb = linToDb(env_) - thresholdDb_;
            float engage = overDb / kKneeDb;
            engage = engage < 0.0f ? 0.0f : (engage > 1.0f ? 1.0f : engage);
            g = dbToLin(rangeDb_ * engage) - 1.0f; // peak bell gain of (range*engage) dB
        }
        stereo[2 * i] = l + g * bpL;
        stereo[2 * i + 1] = r + g * bpR;
    }
}

// ---- Gate -------------------------------------------------------------------

void Gate::reset() {
    env_ = 0.0f;
    gain_ = 1.0f;
    holdCounter_ = 0;
    scLpL_ = 0.0f;
    scLpR_ = 0.0f;
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
    // Sidechain high-pass on the detection signal (one-pole LP; HP = x − LP). 0 = off (detect full).
    const bool scHpf = scHpfHz_ > 0.0f;
    const float scA = scHpf ? 1.0f - std::exp(-6.283185307179586f * scHpfHz_ / sr) : 0.0f;

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Detection signal: the full signal, or its high-passed version when the key filter is on.
        float detL = l, detR = r;
        if (scHpf) {
            scLpL_ += scA * (l - scLpL_);
            scLpR_ += scA * (r - scLpR_);
            detL = l - scLpL_;
            detR = r - scLpR_;
        }
        const float peak = std::max(std::fabs(detL), std::fabs(detR));

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
    std::fill(wfL_.begin(), wfL_.end(), 0.0f);
    std::fill(wfR_.begin(), wfR_.end(), 0.0f);
    wfWrite_ = 0;
    wowPhase_ = 0.0;
    flutPhase_ = 0.0;
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
    // Wow & flutter: a modulated delay wobbles the pitch (a slow ~0.6 Hz wow + faster ~7 Hz flutter).
    const bool doWf = wowFlutter_ > 0.0f;
    const float sr = static_cast<float>(sampleRate);
    if (doWf) {
        const int want = sampleRate / 20 + 4; // ~50 ms delay line
        if (wfSize_ != want) {
            wfSize_ = want;
            wfL_.assign(static_cast<size_t>(wfSize_), 0.0f);
            wfR_.assign(static_cast<size_t>(wfSize_), 0.0f);
            wfWrite_ = 0;
        }
    }
    const double wowInc = 0.6 / static_cast<double>(sampleRate);
    const double flutInc = 7.0 / static_cast<double>(sampleRate);
    const float baseDelay = 0.015f * sr;                 // ~15 ms center tap
    const float wfDepth = wowFlutter_ * 0.004f * sr;     // up to ±4 ms
    constexpr double kTwoPi = 6.283185307179586;
    for (int i = 0; i < frames; ++i) {
        float l = stereo[2 * i];
        float r = stereo[2 * i + 1];
        if (doWf) {
            wfL_[static_cast<size_t>(wfWrite_)] = l;
            wfR_[static_cast<size_t>(wfWrite_)] = r;
            const float mod = 0.7f * static_cast<float>(std::sin(wowPhase_ * kTwoPi)) +
                              0.3f * static_cast<float>(std::sin(flutPhase_ * kTwoPi));
            float rp = static_cast<float>(wfWrite_) - (baseDelay + wfDepth * mod);
            while (rp < 0.0f) {
                rp += static_cast<float>(wfSize_);
            }
            const int i0 = static_cast<int>(rp) % wfSize_;
            const int i1 = (i0 + 1) % wfSize_;
            const float frac = rp - std::floor(rp);
            l = wfL_[static_cast<size_t>(i0)] * (1.0f - frac) + wfL_[static_cast<size_t>(i1)] * frac;
            r = wfR_[static_cast<size_t>(i0)] * (1.0f - frac) + wfR_[static_cast<size_t>(i1)] * frac;
            wfWrite_ = (wfWrite_ + 1) % wfSize_;
            wowPhase_ += wowInc;
            if (wowPhase_ >= 1.0) {
                wowPhase_ -= 1.0;
            }
            flutPhase_ += flutInc;
            if (flutPhase_ >= 1.0) {
                flutPhase_ -= 1.0;
            }
        }
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

// ---- SubBass ----------------------------------------------------------------

void SubBass::reset() {
    lp_ = 0.0f;
    env_ = 0.0f;
    sq_ = 1.0f;
    sub_ = 0.0f;
    prevLp_ = 0.0f;
}

void SubBass::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    const float aTrack = 1.0f - std::exp(-kTwoPi * cutoff_ / static_cast<float>(sampleRate));
    const float aTone = 1.0f - std::exp(-kTwoPi * tone_ / static_cast<float>(sampleRate));
    // Envelope follower time constant (~30 ms) so the sub tracks the bass level without chattering.
    const float aEnv = 1.0f - std::exp(-1.0f / (0.03f * static_cast<float>(sampleRate)));
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float mid = 0.5f * (l + r);
        // Isolate the bass fundamental we track.
        lp_ += aTrack * (mid - lp_);
        // Flip the square once per input cycle (a rising zero-crossing) → an octave-down square.
        if (prevLp_ <= 0.0f && lp_ > 0.0f) {
            sq_ = -sq_;
        }
        prevLp_ = lp_;
        // Follow the tracked bass amplitude so the sub only sounds when bass is present.
        const float mag = lp_ < 0.0f ? -lp_ : lp_;
        env_ += aEnv * (mag - env_);
        // Round the amplitude-tracked square toward a sine with the tone low-pass.
        const float target = sq_ * env_;
        sub_ += aTone * (target - sub_);
        const float add = amount_ * sub_;
        stereo[2 * i] = l + add;
        stereo[2 * i + 1] = r + add;
    }
}

// ---- Octaver ----------------------------------------------------------------

void Octaver::reset() {
    prevL_ = prevR_ = 0.0f;
    hpL_ = hpR_ = 0.0f;
    lpL_ = lpR_ = 0.0f;
}

void Octaver::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || amount_ <= 0.0f || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    // DC-blocker (~20 Hz high-pass) to strip the rectifier's DC offset, and a tone low-pass.
    const float dcR = std::exp(-kTwoPi * 20.0f / static_cast<float>(sampleRate));
    const float aTone = 1.0f - std::exp(-kTwoPi * tone_ / static_cast<float>(sampleRate));
    // 1.6 roughly compensates the rectified octave partial's amplitude so `amount` reads unity-ish.
    const float g = amount_ * 1.6f;
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Full-wave rectify → DC-block → tone low-pass, per channel.
        const float rl = l < 0.0f ? -l : l;
        hpL_ = rl - prevL_ + dcR * hpL_;
        prevL_ = rl;
        lpL_ += aTone * (hpL_ - lpL_);
        const float rr = r < 0.0f ? -r : r;
        hpR_ = rr - prevR_ + dcR * hpR_;
        prevR_ = rr;
        lpR_ += aTone * (hpR_ - lpR_);
        stereo[2 * i] = l + g * lpL_;
        stereo[2 * i + 1] = r + g * lpR_;
    }
}

// ---- AutoPan ----------------------------------------------------------------

void AutoPan::reset() {
    phase_ = 0.0;
}

void AutoPan::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    rateHz_ = modSyncRateHz(syncDiv_, bpm);
}

void AutoPan::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr double kTwoPi = 6.283185307179586;
    constexpr float kQuarterPi = 0.78539816f;
    const double inc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    for (int i = 0; i < frames; ++i) {
        // Pan position in [-1, 1] from the LFO (by shape), scaled by depth.
        float lfo;
        switch (shape_) {
        case Shape::Square:
            lfo = phase_ < 0.5 ? 1.0f : -1.0f;
            break;
        case Shape::Triangle:
            lfo = 1.0f - 4.0f * static_cast<float>(std::fabs(phase_ - 0.5)); // -1→+1→-1
            break;
        case Shape::Saw:
            lfo = 2.0f * static_cast<float>(phase_) - 1.0f; // -1→+1 ramp, then reset
            break;
        default: // Sine
            lfo = static_cast<float>(std::sin(phase_ * kTwoPi));
            break;
        }
        const float pos = depth_ * lfo;
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
        // Map the envelope (clamped to unity) to a cutoff between base and base+range. Downward mode
        // starts open (base+range) and closes toward base as the input gets louder.
        const float e = env_ > 1.0f ? 1.0f : env_;
        const float sweep = sensitivity_ * e * rangeHz_;
        const float cutoff = downward_ ? (baseHz_ + rangeHz_ - sweep) : (baseHz_ + sweep);
        const float wetL =
            lpL_.process(l, cutoff, resonance_, sampleRate, StateVariableFilter::Mode::LowPass);
        const float wetR =
            lpR_.process(r, cutoff, resonance_, sampleRate, StateVariableFilter::Mode::LowPass);
        // Dry/wet blend (parallel wah); mix 1 = fully wet, as before.
        stereo[2 * i] = l * (1.0f - mix_) + wetL * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wetR * mix_;
    }
}

// ---- CombResonator ----------------------------------------------------------

void CombResonator::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    writePos_ = 0;
    dampL_ = 0.0f;
    dampR_ = 0.0f;
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
    const float damp = std::clamp(damping_, 0.0f, 1.0f);
    for (int i = 0; i < frames; ++i) {
        int readPos = writePos_ - d;
        if (readPos < 0) {
            readPos += maxD;
        }
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Damping: one-pole low-pass the delayed (fed-back) signal so the tail loses highs over time.
        dampL_ = bufL_[static_cast<size_t>(readPos)] * (1.0f - damp) + dampL_ * damp;
        dampR_ = bufR_[static_cast<size_t>(readPos)] * (1.0f - damp) + dampR_ * damp;
        // Feedback comb: output = input + feedback · (damped delayed output), stored back into the line.
        const float wl = l + feedback_ * dampL_;
        const float wr = r + feedback_ * dampR_;
        bufL_[static_cast<size_t>(writePos_)] = wl;
        bufR_[static_cast<size_t>(writePos_)] = wr;
        stereo[2 * i] = l * (1.0f - mix_) + wl * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wr * mix_;
        if (++writePos_ >= maxD) {
            writePos_ = 0;
        }
    }
}

// ---- Chord Resonator --------------------------------------------------------

const int* ChordResonator::chordIntervals(Chord c) {
    // Semitone offsets from the root, kVoices each (padded with octaves for a full 6-voice stack).
    static const int major[kVoices] = {0, 4, 7, 12, 16, 19};
    static const int minor[kVoices] = {0, 3, 7, 12, 15, 19};
    static const int dom7[kVoices] = {0, 4, 7, 10, 12, 16};
    static const int min7[kVoices] = {0, 3, 7, 10, 12, 15};
    static const int sus4[kVoices] = {0, 5, 7, 12, 17, 19};
    static const int octaves[kVoices] = {0, 12, 24, 0, 12, 24};
    switch (c) {
    case Chord::Major: return major;
    case Chord::Minor: return minor;
    case Chord::Dom7: return dom7;
    case Chord::Min7: return min7;
    case Chord::Sus4: return sus4;
    case Chord::Octaves: return octaves;
    }
    return minor;
}

void ChordResonator::reset() {
    for (int v = 0; v < kVoices; ++v) {
        std::fill(bufL_[v].begin(), bufL_[v].end(), 0.0f);
        std::fill(bufR_[v].begin(), bufR_[v].end(), 0.0f);
        dampL_[v] = dampR_[v] = 0.0f;
    }
    writePos_ = 0;
}

void ChordResonator::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const int maxD = sampleRate / 20 + 4; // covers down to 20 Hz
    if (static_cast<int>(bufL_[0].size()) != maxD) {
        for (int v = 0; v < kVoices; ++v) {
            bufL_[v].assign(static_cast<size_t>(maxD), 0.0f);
            bufR_[v].assign(static_cast<size_t>(maxD), 0.0f);
        }
        writePos_ = 0;
    }
    // Per-voice delay length from each chord tone's frequency (equal temperament from the root).
    const int* iv = chordIntervals(chord_);
    int delay[kVoices];
    for (int v = 0; v < kVoices; ++v) {
        const double freq = 440.0 * std::pow(2.0, (static_cast<double>(root_ + iv[v]) - 69.0) / 12.0);
        int d = static_cast<int>(static_cast<double>(sampleRate) / freq + 0.5);
        if (d < 1) {
            d = 1;
        }
        if (d >= maxD) {
            d = maxD - 1;
        }
        delay[v] = d;
    }
    const float damp = std::clamp(damping_, 0.0f, 1.0f);
    const float voiceGain = 1.0f / static_cast<float>(kVoices);
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        float wetL = 0.0f, wetR = 0.0f;
        for (int v = 0; v < kVoices; ++v) {
            int readPos = writePos_ - delay[v];
            if (readPos < 0) {
                readPos += maxD;
            }
            dampL_[v] = bufL_[v][static_cast<size_t>(readPos)] * (1.0f - damp) + dampL_[v] * damp;
            dampR_[v] = bufR_[v][static_cast<size_t>(readPos)] * (1.0f - damp) + dampR_[v] * damp;
            const float sl = l + feedback_ * dampL_[v];
            const float sr = r + feedback_ * dampR_[v];
            bufL_[v][static_cast<size_t>(writePos_)] = sl;
            bufR_[v][static_cast<size_t>(writePos_)] = sr;
            wetL += sl;
            wetR += sr;
        }
        wetL *= voiceGain;
        wetR *= voiceGain;
        stereo[2 * i] = l * (1.0f - mix_) + wetL * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wetR * mix_;
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
        float lfo;
        switch (shape_) {
        case Shape::Square:
            lfo = phase_ < 0.5 ? 1.0f : 0.0f;
            break;
        case Shape::Triangle:
            lfo = 1.0f - 2.0f * static_cast<float>(std::fabs(phase_ - 0.5)); // 0→1→0 ramp gate
            break;
        case Shape::Saw:
            lfo = static_cast<float>(phase_); // rising ramp then reset (asymmetric fade)
            break;
        default: // Sine
            lfo = 0.5f + 0.5f * static_cast<float>(std::sin(phase_ * kTwoPi));
            break;
        }
        const float gain = (1.0f - depth_) + depth_ * lfo; // depth 0 → unity (transparent)
        stereo[2 * i] *= gain;
        stereo[2 * i + 1] *= gain;
        phase_ += inc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- StepGate ---------------------------------------------------------------

void StepGate::reset() {
    phase_ = 0.0;
    smoothedGain_ = 1.0f;
}

void StepGate::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    // Each of the 16 steps is one note division, so the whole pattern cycles at 1/16 that rate.
    rateHz_ = modSyncRateHz(syncDiv_, bpm) / static_cast<float>(kSteps);
}

void StepGate::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const double inc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    // One-pole smoothing (~3 ms) so step edges don't click.
    const float sc = 1.0f - std::exp(-1.0f / (0.003f * static_cast<float>(sampleRate)));
    for (int i = 0; i < frames; ++i) {
        int s = static_cast<int>(phase_ * kSteps);
        if (s < 0) {
            s = 0;
        }
        if (s >= kSteps) {
            s = kSteps - 1;
        }
        smoothedGain_ += sc * (steps_[static_cast<size_t>(s)] - smoothedGain_);
        const float g = 1.0f - mix + mix * smoothedGain_;
        stereo[2 * i] *= g;
        stereo[2 * i + 1] *= g;
        phase_ += inc;
        if (phase_ >= 1.0) {
            phase_ -= std::floor(phase_);
        }
    }
}

// ---- StereoDelay ------------------------------------------------------------

void StereoDelay::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    writePos_ = 0;
    dampL_ = dampR_ = 0.0f;
    lcL_ = lcR_ = 0.0f;
}

void StereoDelay::updateTempo(double bpm) {
    if (!sync_ || bpm <= 0.0) {
        return;
    }
    leftMs_ = Delay::syncTimeMs(leftDiv_, bpm);
    rightMs_ = Delay::syncTimeMs(rightDiv_, bpm);
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
    // Feedback tone: high-cut (damping) then low-cut (high-pass) on the fed-back signal.
    const float dampCoef = std::clamp(damping_, 0.0f, 1.0f);
    const bool doLowCut = fbLowCutHz_ > 0.0f;
    const float aLow =
        doLowCut ? 1.0f - std::exp(-2.0f * 3.14159265358979f * fbLowCutHz_ /
                                       static_cast<float>(sampleRate))
                 : 0.0f;
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
        // Feedback tone: darken (high-cut) then de-mud (low-cut) each channel's fed-back echo.
        dampL_ += (1.0f - dampCoef) * (echoL - dampL_);
        dampR_ += (1.0f - dampCoef) * (echoR - dampR_);
        float fbL = dampL_;
        float fbR = dampR_;
        if (doLowCut) {
            lcL_ += aLow * (fbL - lcL_);
            lcR_ += aLow * (fbR - lcR_);
            fbL -= lcL_;
            fbR -= lcR_;
        }
        // Ping-pong cross-routes each channel's echo into the other line (L↔R bounce); otherwise each
        // channel feeds its own (tone-shaped) echo back into its own line at its own time.
        if (pingPong_) {
            bufL_[static_cast<size_t>(writePos_)] = inL + fbR * feedback_;
            bufR_[static_cast<size_t>(writePos_)] = inR + fbL * feedback_;
        } else {
            bufL_[static_cast<size_t>(writePos_)] = inL + fbL * feedback_;
            bufR_[static_cast<size_t>(writePos_)] = inR + fbR * feedback_;
        }
        stereo[2 * i] = inL * (1.0f - mix_) + echoL * mix_;
        stereo[2 * i + 1] = inR * (1.0f - mix_) + echoR * mix_;
        if (++writePos_ >= maxD) {
            writePos_ = 0;
        }
    }
}

// ---- ReverseDelay -----------------------------------------------------------

void ReverseDelay::ensureSized(int sampleRate) {
    int len = static_cast<int>(timeMs_ * 0.001f * static_cast<float>(sampleRate));
    if (len < 1) {
        len = 1;
    }
    // Rebuild (and restart) whenever the sample rate or the chunk length changes.
    if (sizedFor_ != sampleRate || chunkLen_ != len) {
        sizedFor_ = sampleRate;
        chunkLen_ = len;
        recL_.assign(static_cast<size_t>(len), 0.0f);
        recR_.assign(static_cast<size_t>(len), 0.0f);
        playL_.assign(static_cast<size_t>(len), 0.0f);
        playR_.assign(static_cast<size_t>(len), 0.0f);
        wpos_ = 0;
        havePlay_ = false;
    }
}

void ReverseDelay::reset() {
    std::fill(recL_.begin(), recL_.end(), 0.0f);
    std::fill(recR_.begin(), recR_.end(), 0.0f);
    std::fill(playL_.begin(), playL_.end(), 0.0f);
    std::fill(playR_.begin(), playR_.end(), 0.0f);
    wpos_ = 0;
    havePlay_ = false;
}

void ReverseDelay::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    ensureSized(sampleRate);
    const int n = chunkLen_;
    // Short raised edge-fades (~5 ms, at most a quarter-chunk) declick each reversed grain's seams.
    int fade = static_cast<int>(0.005f * static_cast<float>(sampleRate));
    if (fade > n / 4) {
        fade = n / 4;
    }
    if (fade < 1) {
        fade = 1;
    }
    const float mix = mix_;
    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        // Read the previous chunk backwards, windowed.
        float wetL = 0.0f, wetR = 0.0f;
        if (havePlay_) {
            const int r = n - 1 - wpos_;
            float w = 1.0f;
            if (wpos_ < fade) {
                w = static_cast<float>(wpos_) / static_cast<float>(fade);
            } else if (wpos_ >= n - fade) {
                w = static_cast<float>(n - 1 - wpos_) / static_cast<float>(fade);
            }
            wetL = playL_[static_cast<size_t>(r)] * w;
            wetR = playR_[static_cast<size_t>(r)] * w;
        }
        // Record the dry input (plus the reversed wet, for repeating reverse echoes).
        recL_[static_cast<size_t>(wpos_)] = dryL + feedback_ * wetL;
        recR_[static_cast<size_t>(wpos_)] = dryR + feedback_ * wetR;
        if (++wpos_ >= n) {
            recL_.swap(playL_);
            recR_.swap(playR_);
            wpos_ = 0;
            havePlay_ = true;
        }
        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;
    }
}

// ---- MultiTapDelay ----------------------------------------------------------

void MultiTapDelay::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    writePos_ = 0;
}

void MultiTapDelay::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // The line must hold the longest tap (kMaxTaps × the max base time of 1 s).
    const int maxLen = sampleRate * kMaxTaps + 4;
    if (sizedFor_ != sampleRate || static_cast<int>(bufL_.size()) != maxLen) {
        sizedFor_ = sampleRate;
        bufL_.assign(static_cast<size_t>(maxLen), 0.0f);
        bufR_.assign(static_cast<size_t>(maxLen), 0.0f);
        writePos_ = 0;
    }
    const int baseD = static_cast<int>(timeMs_ * 0.001f * static_cast<float>(sampleRate));
    // Precompute each tap's delay + equal-power L/R gains (alternating pan, decaying level).
    int tapDelay[kMaxTaps];
    float gL[kMaxTaps], gR[kMaxTaps];
    for (int k = 0; k < taps_; ++k) {
        int d = baseD * (k + 1);
        if (d >= maxLen) {
            d = maxLen - 1;
        }
        tapDelay[k] = d;
        const float lvl = std::pow(decay_, static_cast<float>(k)); // tap 0 = full, then decays
        // Alternate the pan L/R/L/… by `spread`; even taps lean left, odd taps right.
        const float pan = (k % 2 == 0 ? -1.0f : 1.0f) * spread_;
        const float angle = (pan + 1.0f) * 0.5f * 1.57079632679f;
        gL[k] = std::cos(angle) * lvl;
        gR[k] = std::sin(angle) * lvl;
    }
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Feedback: re-inject the longest (last) tap — read before writing — so the whole cluster
        // repeats one cluster-length later. Skipped at 0 so the write is bit-for-bit the input.
        float fbMono = 0.0f;
        if (feedback_ > 0.0f && taps_ > 0) {
            int rpL = writePos_ - tapDelay[taps_ - 1];
            if (rpL < 0) {
                rpL += maxLen;
            }
            fbMono = 0.5f * (bufL_[static_cast<size_t>(rpL)] + bufR_[static_cast<size_t>(rpL)]);
        }
        bufL_[static_cast<size_t>(writePos_)] = l + fbMono * feedback_;
        bufR_[static_cast<size_t>(writePos_)] = r + fbMono * feedback_;
        float wetL = 0.0f, wetR = 0.0f;
        for (int k = 0; k < taps_; ++k) {
            int rp = writePos_ - tapDelay[k];
            if (rp < 0) {
                rp += maxLen;
            }
            // Feed each tap from the mono sum so its alternating pan is clean.
            const float s = 0.5f * (bufL_[static_cast<size_t>(rp)] + bufR_[static_cast<size_t>(rp)]);
            wetL += s * gL[k];
            wetR += s * gR[k];
        }
        stereo[2 * i] = l * (1.0f - mix_) + wetL * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wetR * mix_;
        if (++writePos_ >= maxLen) {
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
    // First two formant frequencies (Hz) for each vowel A,E,I,O,U.
    static const float kF1[5] = {800.0f, 400.0f, 300.0f, 450.0f, 325.0f};
    static const float kF2[5] = {1150.0f, 1700.0f, 2300.0f, 800.0f, 700.0f};
    float f1, f2;
    if (morphEnabled_) {
        // Continuously interpolate the formants along the vowel sequence.
        const float p = morph_ < 0.0f ? 0.0f : (morph_ > 4.0f ? 4.0f : morph_);
        const int i0 = static_cast<int>(p);
        const int i1 = i0 < 4 ? i0 + 1 : 4;
        const float fr = p - static_cast<float>(i0);
        f1 = kF1[i0] * (1.0f - fr) + kF1[i1] * fr;
        f2 = kF2[i0] * (1.0f - fr) + kF2[i1] * fr;
    } else {
        const int idx = static_cast<int>(vowel_);
        f1 = kF1[idx];
        f2 = kF2[idx];
    }
    // Formant / gender shift: scale both formants by 2^(semis/12), clamped to a sane range.
    if (formantShift_ != 0.0f) {
        const float fmul = std::pow(2.0f, formantShift_ / 12.0f);
        f1 = std::clamp(f1 * fmul, 50.0f, 12000.0f);
        f2 = std::clamp(f2 * fmul, 50.0f, 12000.0f);
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

// ---- Vocoder ----------------------------------------------------------------

void Vocoder::reset() {
    for (int k = 0; k < kBands; ++k) {
        modBP_[k].reset();
        carBP_[k].reset();
        env_[k] = 0.0f;
    }
    carPhase_ = 0.0;
    rng_ = 0x2545F491u;
}

void Vocoder::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // Log-spaced band centres from 180 Hz to 7.5 kHz (the vocal-intelligibility range).
    float fc[kBands];
    for (int k = 0; k < kBands; ++k) {
        fc[k] = 180.0f * std::pow(7500.0f / 180.0f, static_cast<float>(k) / (kBands - 1));
    }
    const float atk = 1.0f - std::exp(-1.0f / (0.005f * sr));            // ~5 ms attack
    const float rel = 1.0f - std::exp(-1.0f / (releaseMs_ * 0.001f * sr));
    const double carInc = static_cast<double>(carrierHz_) / sr;
    constexpr float kBandQ = 5.0f;
    // Per-band output scale so the summed bands land near unity for a full-scale input.
    const float scale = 2.5f / std::sqrt(static_cast<float>(kBands));
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float mod = 0.5f * (l + r); // mono modulator (the input's articulation)
        // Generate the carrier: a buzzy saw (pitched) or white noise (whisper).
        float car;
        if (carrier_ == Carrier::Saw) {
            car = static_cast<float>(2.0 * carPhase_ - 1.0);
            carPhase_ += carInc;
            if (carPhase_ >= 1.0) {
                carPhase_ -= 1.0;
            }
        } else {
            rng_ ^= rng_ << 13;
            rng_ ^= rng_ >> 17;
            rng_ ^= rng_ << 5;
            car = static_cast<float>(rng_) / 2147483648.0f - 1.0f;
        }
        float wet = 0.0f;
        for (int k = 0; k < kBands; ++k) {
            const float m = modBP_[k].process(mod, fc[k], kBandQ, sampleRate,
                                              StateVariableFilter::Mode::BandPass);
            const float a = std::fabs(m);
            const float c = a > env_[k] ? atk : rel;
            env_[k] = c * a + (1.0f - c) * env_[k];
            const float cb = carBP_[k].process(car, fc[k], kBandQ, sampleRate,
                                              StateVariableFilter::Mode::BandPass);
            wet += cb * env_[k];
        }
        wet *= scale;
        stereo[2 * i] = l * (1.0f - mix_) + wet * mix_;
        stereo[2 * i + 1] = r * (1.0f - mix_) + wet * mix_;
    }
}

// ---- StereoWidener ----------------------------------------------------------

void StereoWidener::reset() {
    sideLp_ = 0.0f;
}

void StereoWidener::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const bool doBassMono = bassMonoHz_ > 0.0f;
    const float a =
        doBassMono ? 1.0f - std::exp(-2.0f * 3.14159265358979f * bassMonoHz_ /
                                         static_cast<float>(sampleRate))
                   : 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float mid = 0.5f * (l + r);
        float rawSide = 0.5f * (l - r);
        // Bass mono: low-pass the side and subtract it, so only the high side is widened (lows → mono).
        if (doBassMono) {
            sideLp_ += a * (rawSide - sideLp_);
            rawSide -= sideLp_;
        }
        const float side = rawSide * width_;
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
    const bool doWidth = width_ != 1.0f; // exactly 1 → skip (bit-for-bit unchanged)
    for (int i = 0; i < frames; ++i) {
        float l = stereo[2 * i] * ls;
        float r = stereo[2 * i + 1] * rs;
        if (mono_) {
            const float m = 0.5f * (l + r);
            l = m;
            r = m;
        } else if (doWidth) {
            // M/S width: scale the side (L−R) component around the mid.
            const float mid = 0.5f * (l + r);
            const float side = 0.5f * (l - r) * width_;
            l = mid + side;
            r = mid - side;
        }
        stereo[2 * i] = l * g;
        stereo[2 * i + 1] = r * g;
    }
}

// ---- Clipper ----------------------------------------------------------------

void Clipper::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float g = dbToLin(driveDb_);
    const float c = ceiling_;
    const float h = hardness_;
    const int n = frames * 2;
    for (int i = 0; i < n; ++i) {
        const float s = stereo[i] * g;
        const float hard = s < -c ? -c : (s > c ? c : s); // instantaneous flat-top clamp
        const float soft = c * std::tanh(s / c);          // smooth saturation, asymptotic to ±c
        stereo[i] = h * hard + (1.0f - h) * soft;         // both bounded by ±c → ceiling guaranteed
    }
}

// ---- Limiter ----------------------------------------------------------------

void Limiter::reset() {
    std::fill(dL_.begin(), dL_.end(), 0.0f);
    std::fill(dR_.begin(), dR_.end(), 0.0f);
    std::fill(dPeak_.begin(), dPeak_.end(), 0.0f);
    widx_ = 0;
    gain_ = 1.0f;
    grDb_ = 0.0f;
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
    float gMin = 1.0f; // deepest (smallest) gain this block, for the GR meter

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
        if (gain_ < gMin) {
            gMin = gain_; // track the deepest reduction for the GR meter
        }

        stereo[2 * i] = outL * gain_;
        stereo[2 * i + 1] = outR * gain_;
    }
    grDb_ = gMin < 1.0f ? linToDb(gMin) : 0.0f;
}

// ---- Leveler ----------------------------------------------------------------

void Leveler::reset() {
    env_ = 0.0f;
    gain_ = 1.0f;
    gainDb_ = 0.0f;
}

void Leveler::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // One slow follower for the level, and an even slower smoother on the gain, both from responseMs.
    const float envCoef = std::exp(-1.0f / (responseMs_ * 0.001f * sr));
    const float gainCoef = std::exp(-1.0f / (responseMs_ * 0.001f * sr));
    const float target = dbToLin(targetDb_);
    const float gMax = dbToLin(maxGainDb_);
    const float gMin = dbToLin(-maxGainDb_);
    constexpr float kFloor = 1e-4f; // don't try to lift near-silence (noise) up to the target
    float lastGain = gain_;
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float mag = std::max(std::fabs(l), std::fabs(r));
        // Slow level follower (mean-magnitude, one-pole).
        env_ = envCoef * env_ + (1.0f - envCoef) * mag;
        // Desired gain drives the level toward the target; only engage above the noise floor.
        float desired = 1.0f;
        if (env_ > kFloor) {
            desired = target / env_;
            desired = desired < gMin ? gMin : (desired > gMax ? gMax : desired);
        }
        gain_ = gainCoef * gain_ + (1.0f - gainCoef) * desired;
        stereo[2 * i] = l * gain_;
        stereo[2 * i + 1] = r * gain_;
        lastGain = gain_;
    }
    gainDb_ = linToDb(lastGain);
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
    // Shimmer grain buffer: ~50 ms, the octave-up pitch-shifter's delay line.
    shBuf_.assign(static_cast<size_t>(sampleRate) / 20 + 1, 0.0f);
    shWrite_ = 0;
    shPhase_ = 0.0;
    shState_ = 0.0f;
    // Tail-modulation delay lines: ~12 ms (well above the 6 ms max depth + interpolation guard).
    const int modLen = static_cast<int>(0.012 * static_cast<double>(sampleRate)) + 4;
    modBufL_.assign(static_cast<size_t>(modLen), 0.0f);
    modBufR_.assign(static_cast<size_t>(modLen), 0.0f);
    modWrite_ = 0;
    modPhase_ = 0.0;
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
    duckEnv_ = 0.0f;
    lcL_ = lcR_ = hcL_ = hcR_ = 0.0f;
    gateGain_ = 1.0f;
    gateCountdown_ = 0;
    std::fill(shBuf_.begin(), shBuf_.end(), 0.0f);
    shWrite_ = 0;
    shPhase_ = 0.0;
    shState_ = 0.0f;
    std::fill(modBufL_.begin(), modBufL_.end(), 0.0f);
    std::fill(modBufR_.begin(), modBufR_.end(), 0.0f);
    modWrite_ = 0;
    modPhase_ = 0.0;
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
    // Ducking envelope: follow the dry input's peak (fast attack, slower release) and pull the wet
    // down while the dry is loud. duck_ = 0 leaves the wet untouched.
    const float sr = static_cast<float>(sampleRate);
    const float duckAtk = std::exp(-1.0f / (0.005f * sr));  // ~5 ms attack
    const float duckRel = std::exp(-1.0f / (0.150f * sr));  // ~150 ms release
    // Gated reverb: hold the wet open while the input is present (+ gateMs after), then cut it fast.
    const bool doGate = gateMs_ > 0.0f;
    const int gateHold = static_cast<int>(gateMs_ * 0.001f * sr);
    const float gateClose = 1.0f / (0.004f * sr); // ~4 ms close ramp
    constexpr float kGateThresh = 0.02f;          // dry level that (re)opens the gate

    // Wet-tone one-pole coefficients (computed once per block). Low-cut off at 0 Hz, high-cut off at
    // 20 kHz — those defaults leave the wet untouched.
    const bool doLowCut = lowCutHz_ > 0.0f;
    const bool doHighCut = highCutHz_ < 20000.0f;
    const float aLow = doLowCut ? 1.0f - std::exp(-2.0f * 3.14159265358979f * lowCutHz_ / sr) : 0.0f;
    const float aHigh = doHighCut ? 1.0f - std::exp(-2.0f * 3.14159265358979f * highCutHz_ / sr) : 0.0f;

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

        // Track the dry level for ducking (stereo-linked peak follower).
        float duckGain = 1.0f;
        if (duck_ > 0.0f) {
            const float peak = std::max(std::fabs(dryL), std::fabs(dryR));
            const float coef = peak > duckEnv_ ? duckAtk : duckRel;
            duckEnv_ = coef * duckEnv_ + (1.0f - coef) * peak;
            duckGain = 1.0f - duck_ * std::min(1.0f, duckEnv_);
            if (duckGain < 0.0f) {
                duckGain = 0.0f;
            }
        }

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

        // Tail modulation: sweep a short fractional delay on the wet tail so it shimmers and
        // de-correlates (a lush plate/hall) instead of ringing statically. L/R LFOs in quadrature for
        // stereo movement. Skipped when depth is 0 (the wet is then bit-for-bit unchanged).
        if (modDepthMs_ > 0.0f && !modBufL_.empty()) {
            constexpr float kTwoPiF = 6.28318530718f;
            const int mlen = static_cast<int>(modBufL_.size());
            modBufL_[static_cast<size_t>(modWrite_)] = wetL;
            modBufR_[static_cast<size_t>(modWrite_)] = wetR;
            const float depthSamp = modDepthMs_ * 0.001f * static_cast<float>(sampleRate);
            const float lfoL = std::sin(static_cast<float>(modPhase_) * kTwoPiF);
            const float lfoR = std::sin(static_cast<float>(modPhase_ + 0.25) * kTwoPiF);
            auto readFrac = [&](const std::vector<float>& buf, float delay) {
                float rp = static_cast<float>(modWrite_) - delay;
                while (rp < 0.0f) {
                    rp += static_cast<float>(mlen);
                }
                const int i0 = static_cast<int>(rp);
                const float frac = rp - static_cast<float>(i0);
                const int i1 = (i0 + 1) % mlen;
                return buf[static_cast<size_t>(i0)] * (1.0f - frac) +
                       buf[static_cast<size_t>(i1)] * frac;
            };
            wetL = readFrac(modBufL_, 1.0f + depthSamp * (0.5f + 0.5f * lfoL));
            wetR = readFrac(modBufR_, 1.0f + depthSamp * (0.5f + 0.5f * lfoR));
            modWrite_ = (modWrite_ + 1) % mlen;
            modPhase_ += static_cast<double>(modRateHz_) / static_cast<double>(sampleRate);
            if (modPhase_ >= 1.0) {
                modPhase_ -= 1.0;
            }
        }

        // Shimmer: a parallel octave-up loop on the wet tail. A windowed two-tap granular shifter
        // transposes the wet up an octave; its own bounded self-feedback (kShFb < 1) restacks each
        // pass an octave higher for the ascending-octave halo. Added to the wet OUTPUT only — never
        // into the comb feedback — so it can never destabilise the reverb. Skipped when shimmer_ == 0
        // (the reverb is then bit-for-bit unchanged).
        if (shimmer_ > 0.0f && !shBuf_.empty()) {
            constexpr float kShFb = 0.6f; // shimmer cascade feedback (octave stacking); < 1 = stable
            const int shLen = static_cast<int>(shBuf_.size());
            const float src = 0.5f * (wetL + wetR) + kShFb * shState_;
            shBuf_[static_cast<size_t>(shWrite_)] = src;
            // Delay shrinks each sample (read runs at 2× write) → up one octave; wrap the grain phase.
            shPhase_ -= 1.0 / static_cast<double>(shLen);
            if (shPhase_ < 0.0) {
                shPhase_ += 1.0;
            }
            double p2 = shPhase_ + 0.5;
            if (p2 >= 1.0) {
                p2 -= 1.0;
            }
            auto readTap = [&](double phase) {
                double rp = static_cast<double>(shWrite_) - phase * static_cast<double>(shLen);
                while (rp < 0.0) {
                    rp += static_cast<double>(shLen);
                }
                const int i0 = static_cast<int>(rp);
                const float fr = static_cast<float>(rp - i0);
                const int i1 = (i0 + 1) % shLen;
                return shBuf_[static_cast<size_t>(i0)] * (1.0f - fr) +
                       shBuf_[static_cast<size_t>(i1)] * fr;
            };
            // Triangular grain windows, each zero at its own wrap point so the seam is silent.
            const float w1 = 1.0f - std::fabs(2.0f * static_cast<float>(shPhase_) - 1.0f);
            const float w2 = 1.0f - std::fabs(2.0f * static_cast<float>(p2) - 1.0f);
            const float shOut = readTap(shPhase_) * w1 + readTap(p2) * w2;
            shState_ = shOut;
            shWrite_ = (shWrite_ + 1) % shLen;
            wetL += shimmer_ * shOut;
            wetR += shimmer_ * shOut;
        }

        // Wet-tail tone: low-cut (high-pass = input − low-passed) then high-cut (low-pass) on the wet
        // only, so the tail can be de-mudded and de-harshed independently of the dry.
        if (doLowCut) {
            lcL_ += aLow * (wetL - lcL_);
            lcR_ += aLow * (wetR - lcR_);
            wetL -= lcL_;
            wetR -= lcR_;
        }
        if (doHighCut) {
            hcL_ += aHigh * (wetL - hcL_);
            hcR_ += aHigh * (wetR - hcR_);
            wetL = hcL_;
            wetR = hcR_;
        }

        // Duck the wet by the dry level so the tail steps out of the way of the source.
        wetL *= duckGain;
        wetR *= duckGain;

        // Gated reverb: keep the tail at full while the input is present (+ hold), then cut it fast.
        if (doGate) {
            const float peak = std::max(std::fabs(dryL), std::fabs(dryR));
            if (peak > kGateThresh) {
                gateCountdown_ = gateHold;
                gateGain_ = 1.0f;
            } else if (gateCountdown_ > 0) {
                --gateCountdown_;
            } else {
                gateGain_ -= gateClose;
                if (gateGain_ < 0.0f) {
                    gateGain_ = 0.0f;
                }
            }
            wetL *= gateGain_;
            wetR *= gateGain_;
        }

        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;
    }
}

// ---- Convolver --------------------------------------------------------------

namespace {
// Deterministic noise in [-1, 1) from an index — the IR is reproducible across runs/loads.
float convNoise(uint32_t i) {
    uint32_t x = i * 2654435761u + 0x9E3779B9u;
    x ^= x >> 15;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return static_cast<float>(x / 2147483648.0) - 1.0f;
}
} // namespace

void Convolver::buildIR(int sampleRate) {
    constexpr float kTwoPi = 6.283185307179586f;
    int len = static_cast<int>(decay_ * static_cast<float>(sampleRate));
    if (len < 1) {
        len = 1;
    }
    const int cap = sampleRate / 2; // hard cap at 0.5 s of IR
    if (len > cap) {
        len = cap;
    }
    irL_.assign(static_cast<size_t>(len), 0.0f);
    irR_.assign(static_cast<size_t>(len), 0.0f);
    const float aTone = 1.0f - std::exp(-kTwoPi * tone_ / static_cast<float>(sampleRate));
    // Exponential decay envelope: ~4 time-constants across the IR window so it fades to near silence.
    const float tau = decay_ * 0.25f * static_cast<float>(sampleRate);
    float lpL = 0.0f, lpR = 0.0f;
    double eL = 0.0, eR = 0.0;
    for (int i = 0; i < len; ++i) {
        // Two decorrelated noise streams → a wide, natural stereo tail. A tiny early gap keeps the
        // direct hit from being swamped by the very first IR sample.
        const float nL = convNoise(0x1000u + static_cast<uint32_t>(i));
        const float nR = convNoise(0x9000u + static_cast<uint32_t>(i));
        lpL += aTone * (nL - lpL);
        lpR += aTone * (nR - lpR);
        const float env = std::exp(-static_cast<float>(i) / tau);
        irL_[static_cast<size_t>(i)] = lpL * env;
        irR_[static_cast<size_t>(i)] = lpR * env;
        eL += static_cast<double>(irL_[static_cast<size_t>(i)]) * irL_[static_cast<size_t>(i)];
        eR += static_cast<double>(irR_[static_cast<size_t>(i)]) * irR_[static_cast<size_t>(i)];
    }
    // Energy-normalize each IR so the wet level is roughly constant regardless of decay/tone.
    const float gL = eL > 0.0 ? static_cast<float>(1.0 / std::sqrt(eL)) : 1.0f;
    const float gR = eR > 0.0 ? static_cast<float>(1.0 / std::sqrt(eR)) : 1.0f;
    for (int i = 0; i < len; ++i) {
        irL_[static_cast<size_t>(i)] *= gL;
        irR_[static_cast<size_t>(i)] *= gR;
    }
    histL_.assign(static_cast<size_t>(len), 0.0f);
    histR_.assign(static_cast<size_t>(len), 0.0f);
    histPos_ = 0;
    dirty_ = false;
    builtFor_ = sampleRate;
}

void Convolver::reset() {
    std::fill(histL_.begin(), histL_.end(), 0.0f);
    std::fill(histR_.begin(), histR_.end(), 0.0f);
    histPos_ = 0;
}

void Convolver::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    if (dirty_ || builtFor_ != sampleRate || irL_.empty()) {
        buildIR(sampleRate);
    }
    const int len = static_cast<int>(irL_.size());
    if (len <= 0) {
        return;
    }
    const float mix = mix_;
    const float* irL = irL_.data();
    const float* irR = irR_.data();
    float* hL = histL_.data();
    float* hR = histR_.data();
    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        hL[histPos_] = dryL;
        hR[histPos_] = dryR;
        // Direct-form convolution: sum ir[k] * history[now - k].
        double accL = 0.0, accR = 0.0;
        int idx = histPos_;
        for (int k = 0; k < len; ++k) {
            accL += static_cast<double>(irL[k]) * hL[idx];
            accR += static_cast<double>(irR[k]) * hR[idx];
            if (--idx < 0) {
                idx = len - 1;
            }
        }
        if (++histPos_ >= len) {
            histPos_ = 0;
        }
        stereo[2 * i] = dryL * (1.0f - mix) + static_cast<float>(accL) * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + static_cast<float>(accR) * mix;
    }
}

} // namespace maz::audio
