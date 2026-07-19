#include "maz/audio/DrumVoice.hpp"

#include <cmath>

namespace maz::audio {

namespace {
constexpr double kTwoPi = 6.283185307179586;

// Longest envelope time-constant per drum; once t exceeds a few of these the hit is silent and the
// voice can deactivate. (tau, in seconds.)
double decayTau(Drum type) {
    switch (type) {
    case Drum::Kick:
        return 0.16;
    case Drum::Snare:
        return 0.12;
    case Drum::ClosedHat:
        return 0.03;
    case Drum::OpenHat:
        return 0.28;
    case Drum::Clap:
        return 0.14;
    case Drum::Tom:
        return 0.20;
    case Drum::Cowbell:
        return 0.12;
    case Drum::Rimshot:
        return 0.05;
    case Drum::Crash:
        return 0.6;
    case Drum::Ride:
        return 0.5;
    case Drum::Shaker:
        return 0.08;
    case Drum::Clave:
        return 0.025;
    case Drum::Tambourine:
        return 0.12;
    case Drum::Conga:
        return 0.14;
    case Drum::Woodblock:
        return 0.035;
    case Drum::Bongo:
        return 0.09;
    case Drum::Triangle:
        return 0.7;
    case Drum::Kick808:
        return 0.45; // a long, sustaining sub tail — far longer than the short punchy kick
    case Drum::Zap:
        return 0.12; // a short, punchy electronic "pew"
    case Drum::Riser:
        return 0.13; // sets the swell length (~0.8 s) — the voice deactivates as the riser peaks
    case Drum::Snare808:
        return 0.11; // the "snappy" noise tail sets the length; the tuned shell decays faster
    case Drum::Hat808:
        return 0.035; // a tight, bright metallic closed hat
    case Drum::Clap808:
        return 0.12; // the smeared noise "reverb" tail sets the length
    case Drum::Snap:
        return 0.022; // a dry, tight finger snap — a very fast decay
    case Drum::Timbale:
        return 0.14; // a high, ringing metal-shell drum — a medium-short metallic ring
    case Drum::Agogo:
        return 0.16; // a bright, high metallic bell — a clear pitched ring
    case Drum::Splash:
        return 0.16; // a short, explosive cymbal — much shorter than the long crash wash
    case Drum::China:
        return 0.30; // a trashy cymbal — decay sits between the short splash and the long crash
    }
    return 0.1;
}
} // namespace

void DrumVoice::trigger(float velocity, float extraSemitones) {
    active_ = true;
    choking_ = false;
    chokeGain_ = 1.0f;
    t_ = 0.0;
    phase_ = 0.0;
    velocity_ = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
    // Capture the hit's pitch (base tune + a per-step offset) at strike time, so a later per-step
    // pitch change never retroactively bends a still-ringing hit.
    hitTune_ = tuneSemitones_ + extraSemitones;
    toneLp_ = 0.0f; // start the tone filter from silence so the attack is click-free
    hpLp_ = 0.0f;   // reset the high-pass state too
}

void DrumVoice::choke() {
    if (active_) {
        choking_ = true; // render() ramps chokeGain_ to 0 and deactivates
    }
}

float DrumVoice::noise() {
    // xorshift32 → float in [-1, 1).
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 2147483648.0f - 1.0f;
}

void DrumVoice::render(float* out, int frames, int sampleRate) {
    if (!active_ || sampleRate <= 0 || frames <= 0) {
        return;
    }
    const double dt = 1.0 / static_cast<double>(sampleRate);
    const double tau = decayTau(type_) * static_cast<double>(decayMul_);
    const double pitchMul = std::pow(2.0, static_cast<double>(hitTune_) / 12.0);
    // Per-voice tone low-pass: one-pole coefficient; skipped entirely when open (bit-transparent).
    const bool useTone = toneCutoff_ < 19000.0f;
    const float toneA =
        useTone ? 1.0f - std::exp(-2.0f * 3.14159265358979f * toneCutoff_ / static_cast<float>(sampleRate))
                : 0.0f;
    // Per-voice high-pass / low-cut: one-pole coefficient; the HP output is (signal − low band).
    const bool useHp = hpCutoff_ > 0.0f;
    const float hpA =
        useHp ? 1.0f - std::exp(-2.0f * 3.14159265358979f * hpCutoff_ / static_cast<float>(sampleRate))
              : 0.0f;

    for (int i = 0; i < frames; ++i) {
        const double env = std::exp(-t_ / tau);
        float s = 0.0f;

        switch (type_) {
        case Drum::Kick: {
            // Pitch sweeps from ~120 Hz down to ~45 Hz over the first few ms — the classic thump.
            // pitchEnv_ scales the sweep depth (0 = flat sub, 1 = natural, 2 = extra snap/punch).
            const double freq =
                (45.0 + 75.0 * static_cast<double>(pitchEnv_) *
                            std::exp(-t_ / (0.03 * static_cast<double>(pitchEnvTime_)))) *
                pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Snare: {
            const double tone = std::sin(phase_ * kTwoPi);
            const float n = noise();
            // Snap crossfades body (tone) ↔ wires (noise); snap 0.5 = the classic 0.4/0.9 mix.
            const double toneW = 0.8 * (1.0 - static_cast<double>(snap_));
            const double noiseW = 1.8 * static_cast<double>(snap_);
            s = static_cast<float>((toneW * tone + noiseW * static_cast<double>(n)) * env);
            phase_ += 180.0 * pitchMul * dt;
            break;
        }
        case Drum::ClosedHat:
        case Drum::OpenHat:
            s = static_cast<float>(static_cast<double>(noise()) * env);
            break;
        case Drum::Clap:
            // A slightly grittier noise burst.
            s = static_cast<float>(static_cast<double>(noise()) * env * env);
            break;
        case Drum::Tom: {
            // A tuned membrane: a sine with a gentle downward pitch sweep (200 → 100 Hz).
            // pitchEnv_ scales the sweep depth (0 = flat, 1 = natural, 2 = a deeper drop).
            const double freq =
                (100.0 + 100.0 * static_cast<double>(pitchEnv_) *
                             std::exp(-t_ / (0.06 * static_cast<double>(pitchEnvTime_)))) *
                pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Cowbell: {
            // The classic 808 cowbell: two detuned square tones (~540 + ~800 Hz) ringing together.
            const double a = std::sin(kTwoPi * 540.0 * pitchMul * t_) >= 0.0 ? 1.0 : -1.0;
            const double b = std::sin(kTwoPi * 800.0 * pitchMul * t_) >= 0.0 ? 1.0 : -1.0;
            s = static_cast<float>(0.5 * (a + b) * env);
            break;
        }
        case Drum::Rimshot: {
            // A short, bright crack: a high tone (~1700 Hz) plus a noise click, very fast decay.
            const double tone = std::sin(kTwoPi * 1700.0 * pitchMul * t_);
            s = static_cast<float>((0.7 * tone + 0.3 * static_cast<double>(noise())) * env);
            break;
        }
        case Drum::Crash: {
            // A long, bright cymbal wash: mostly white noise, with a few high inharmonic partials
            // adding metallic shimmer over the long decay.
            const double metal = std::sin(kTwoPi * 4200.0 * pitchMul * t_) +
                                 std::sin(kTwoPi * 5300.0 * pitchMul * t_) +
                                 std::sin(kTwoPi * 6700.0 * pitchMul * t_);
            s = static_cast<float>((0.6 * static_cast<double>(noise()) + 0.13 * metal) * env);
            break;
        }
        case Drum::Ride: {
            // A defined, pingy ride cymbal: strong high metallic partials (the "ping") with only a
            // little noise wash and a sharp attack transient, sustaining over a medium decay.
            const double ping = std::sin(kTwoPi * 3300.0 * pitchMul * t_) +
                                0.7 * std::sin(kTwoPi * 4100.0 * pitchMul * t_) +
                                0.5 * std::sin(kTwoPi * 5800.0 * pitchMul * t_);
            const double attack = std::exp(-t_ / 0.008); // a brief bright tick at the strike
            s = static_cast<float>(
                (0.22 * ping + 0.18 * static_cast<double>(noise()) * attack + 0.05 * static_cast<double>(noise())) *
                env);
            break;
        }
        case Drum::Shaker: {
            // A bright noise "shhh"/"tss" with a soft attack — rounder and airier than a hi-hat.
            const double attack = 1.0 - std::exp(-t_ / 0.004); // gentle onset (not an instant click)
            s = static_cast<float>(static_cast<double>(noise()) * env * attack);
            break;
        }
        case Drum::Clave: {
            // A short, bright wooden "tock": a pure high sine (~2500 Hz) with a very fast decay.
            s = static_cast<float>(std::sin(kTwoPi * 2500.0 * pitchMul * t_) * env);
            break;
        }
        case Drum::Tambourine: {
            // Bright jingling noise with a couple of high metallic partials for shimmer, medium decay.
            const double jingle = std::sin(kTwoPi * 8000.0 * pitchMul * t_) +
                                  std::sin(kTwoPi * 11000.0 * pitchMul * t_);
            s = static_cast<float>((0.7 * static_cast<double>(noise()) + 0.15 * jingle) * env);
            break;
        }
        case Drum::Conga: {
            // A tuned hand drum: a sine with a fast downward pitch sweep (~420 → 220 Hz), tighter and
            // higher than the tom.
            const double freq = (220.0 + 200.0 * std::exp(-t_ / 0.04)) * pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Woodblock: {
            // A hollow wooden "tok": a mid tone (~800 Hz) with a quieter inharmonic upper partial
            // (~1300 Hz) for the woody ring, a sharp attack and a fast decay. Lower and rounder than
            // the (much higher, pure) clave.
            const double tok = std::sin(kTwoPi * 800.0 * pitchMul * t_) +
                               0.4 * std::sin(kTwoPi * 1300.0 * pitchMul * t_);
            s = static_cast<float>(0.7 * tok * env);
            break;
        }
        case Drum::Bongo: {
            // A tight, high tuned hand drum: a sine with a fast downward pitch sweep (~650 → 350 Hz),
            // higher and snappier than the conga.
            const double freq = (350.0 + 300.0 * std::exp(-t_ / 0.025)) * pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Triangle: {
            // A percussion triangle: bright inharmonic high partials, purely tonal (no noise), with a
            // long shimmering ring — distinct from the noise-based cymbals.
            const double ring = std::sin(kTwoPi * 4200.0 * pitchMul * t_) +
                                0.7 * std::sin(kTwoPi * 5400.0 * pitchMul * t_) +
                                0.5 * std::sin(kTwoPi * 6900.0 * pitchMul * t_);
            s = static_cast<float>(0.3 * ring * env);
            break;
        }
        case Drum::Kick808: {
            // The classic 808 sub kick: a pure sine at a very low fundamental (~50 Hz) with a short
            // downward pitch glide from ~90 Hz over the first ~50 ms, and a long sustaining tail.
            // Deeper and far longer than the short, punchy standard kick.
            const double freq = (50.0 + 40.0 * std::exp(-t_ / 0.04)) * pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Zap: {
            // A synthwave/EDM "laser zap": a sine with a very fast, very wide downward pitch sweep
            // (~1800 Hz → ~70 Hz over the first ~20 ms) and a short punchy decay. Far higher and
            // faster than the kick/808 sweep, so it reads as an electronic "pew" rather than a thump.
            const double freq = (70.0 + 1730.0 * std::exp(-t_ / 0.02)) * pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Riser: {
            // Reverse-cymbal / riser: a bright noise burst that SWELLS UP (rather than decays) over
            // the voice's life, then cuts off — the classic build-up transition sweep. The swell
            // length tracks `tau` (and thus the per-channel decay knob); the voice deactivates as it
            // peaks (exp(-t/tau) crosses the inactive threshold at ~6.2 tau).
            const double riseTime = tau * 6.2;
            double renv = riseTime > 0.0 ? t_ / riseTime : 1.0;
            if (renv > 1.0) {
                renv = 1.0;
            }
            s = static_cast<float>(static_cast<double>(noise()) * renv * renv); // accelerating swell
            break;
        }
        case Drum::Snare808: {
            // The TR-808 snare: a tuned "shell" of two sine partials (~185 + ~330 Hz) that decays
            // fast, plus a longer "snappy" noise tail (the snare wires). `snap_` balances shell↔snappy
            // (0 = all body, 1 = all snap, 0.5 = the classic mix) — distinct from the acoustic Snare's
            // single-tone body + wire-noise blend by its two fixed tuned partials and dual decays.
            const double shell = 0.5 * (std::sin(kTwoPi * 185.0 * pitchMul * t_) +
                                        std::sin(kTwoPi * 330.0 * pitchMul * t_));
            const double envShell = std::exp(-t_ / 0.035); // fast tonal decay
            const double envSnap = std::exp(-t_ / 0.11);   // longer noise ("snappy") decay
            const double shellW = 0.7 * (1.0 - static_cast<double>(snap_));
            const double snapW = 1.2 * static_cast<double>(snap_);
            s = static_cast<float>(shellW * shell * envShell +
                                   snapW * static_cast<double>(noise()) * envSnap);
            break;
        }
        case Drum::Hat808: {
            // The TR-808 hi-hat: a dense cluster of six inharmonic square oscillators (the same
            // "metal" tone-generator idea as the cowbell, but six higher, closely-spaced partials)
            // with a tight, fast decay — bright and metallic, distinct from the noise-based hats.
            static const double freqs[6] = {682.0, 1041.0, 1123.0, 1479.0, 1693.0, 2140.0};
            double sum = 0.0;
            for (double fk : freqs) {
                sum += std::sin(kTwoPi * fk * pitchMul * t_) >= 0.0 ? 1.0 : -1.0;
            }
            s = static_cast<float>(sum / 6.0 * env);
            break;
        }
        case Drum::Clap808: {
            // The TR-808 clap: three sharp noise bursts ~9 ms apart, then a longer smeared noise
            // "reverb" tail — the signature stuttered clap, distinct from the single-burst acoustic
            // clap. Each burst is a fast exp decay gated at its onset; the tail decays slowly from the
            // third burst.
            auto burst = [&](double onset, double dec) {
                return t_ >= onset ? std::exp(-(t_ - onset) / dec) : 0.0;
            };
            const double e = burst(0.0, 0.004) + burst(0.009, 0.004) + burst(0.018, 0.004) +
                             0.6 * burst(0.018, 0.06); // the smeared tail
            s = static_cast<float>(static_cast<double>(noise()) * e);
            break;
        }
        case Drum::Snap: {
            // A dry finger snap: one short, band-focused noise pop with a woody ~2.2 kHz resonance
            // and a sharp attack, decaying very fast — tighter and drier than the (stuttered) 808/
            // acoustic clap and more noise-like than the (tonal) rimshot.
            const double body = std::sin(kTwoPi * 2200.0 * pitchMul * t_) * std::exp(-t_ / 0.010);
            const double click = static_cast<double>(noise()) * env; // sharp broadband transient
            s = static_cast<float>(0.5 * click + 0.6 * body * env);
            break;
        }
        case Drum::Timbale: {
            // A high, ringing metal-shell drum (Latin timbale): a tuned fundamental (~300 Hz) plus two
            // inharmonic metallic partials for the shell ring, and a sharp stick-attack noise tick.
            // Brighter and drier than the tom (a pure membrane sweep) and more tonal than the cowbell.
            const double bodyT = std::sin(kTwoPi * 300.0 * pitchMul * t_);
            const double ring = 0.4 * std::sin(kTwoPi * 845.0 * pitchMul * t_) +
                                0.25 * std::sin(kTwoPi * 1290.0 * pitchMul * t_);
            const double stick = std::exp(-t_ / 0.004); // brief bright stick tick
            s = static_cast<float>(
                (0.7 * bodyT + ring + 0.5 * static_cast<double>(noise()) * stick) * env);
            break;
        }
        case Drum::Agogo: {
            // A bright, high-pitched metallic bell (agogo): a clear ~780 Hz fundamental plus two
            // inharmonic bell partials (~2.8x, ~5.2x) for a shimmering ring, with a sharp attack tick.
            // Higher and more bell-like (pure sine partials) than the buzzy two-square cowbell.
            const double fund = std::sin(kTwoPi * 780.0 * pitchMul * t_);
            const double ring = 0.5 * std::sin(kTwoPi * 2184.0 * pitchMul * t_) +
                                0.3 * std::sin(kTwoPi * 4056.0 * pitchMul * t_);
            const double tick = std::exp(-t_ / 0.003); // a brief metallic strike tick
            s = static_cast<float>((fund + ring + 0.2 * static_cast<double>(noise()) * tick) * env);
            break;
        }
        case Drum::Splash: {
            // A short, explosive splash cymbal: as bright and noisy as a crash (white wash + high
            // inharmonic partials) but with a fast, sharp decay instead of the crash's long wash — so
            // it reads as a quick accent. Higher, tighter partials than the crash keep it brilliant.
            const double metal = std::sin(kTwoPi * 5200.0 * pitchMul * t_) +
                                 std::sin(kTwoPi * 6400.0 * pitchMul * t_) +
                                 std::sin(kTwoPi * 7900.0 * pitchMul * t_);
            const double attack = std::exp(-t_ / 0.006); // a bright burst right at the strike
            s = static_cast<float>(
                (0.6 * static_cast<double>(noise()) + 0.15 * metal + 0.2 * static_cast<double>(noise()) * attack) *
                env);
            break;
        }
        case Drum::China: {
            // A trashy, aggressive china cymbal: even more noise than a crash plus dense, high
            // inharmonic partials for a harsh, metallic clang, with a sharp attack and a medium decay
            // (between the short splash and the long crash).
            const double metal = std::sin(kTwoPi * 5900.0 * pitchMul * t_) +
                                 std::sin(kTwoPi * 7300.0 * pitchMul * t_) +
                                 std::sin(kTwoPi * 9100.0 * pitchMul * t_) +
                                 std::sin(kTwoPi * 11300.0 * pitchMul * t_);
            const double attack = std::exp(-t_ / 0.004); // a sharp trashy strike
            s = static_cast<float>(
                (0.7 * static_cast<double>(noise()) + 0.18 * metal + 0.25 * static_cast<double>(noise()) * attack) *
                env);
            break;
        }
        }

        if (phase_ >= 1.0) {
            phase_ -= std::floor(phase_);
        }

        // Drive: push the hit through a tanh soft-clipper (normalised so full-scale stays ~unity) to
        // add harmonics and grit. Skipped entirely at 0 so the clean drum is bit-identical.
        if (drive_ > 0.0f) {
            const float k = 1.0f + drive_ * 8.0f;
            s = std::tanh(s * k) / std::tanh(k);
        }

        // Tone: one-pole low-pass to darken the hit (post-drive so it also tames drive harmonics).
        if (useTone) {
            toneLp_ += toneA * (s - toneLp_);
            s = toneLp_;
        }

        // High-pass / low-cut: track the low band with a one-pole and subtract it, thinning the hit.
        if (useHp) {
            hpLp_ += hpA * (s - hpLp_);
            s = s - hpLp_;
        }

        // Choke fade: ~4 ms ramp to silence, then the voice deactivates.
        if (choking_) {
            chokeGain_ -= static_cast<float>(dt) / 0.004f;
            if (chokeGain_ <= 0.0f) {
                chokeGain_ = 0.0f;
                active_ = false;
            }
            s *= chokeGain_;
        }

        out[i] += s * gain_ * level_ * velocity_;
        t_ += dt;

        if (!active_) {
            break; // choked out mid-block
        }
    }

    // Deactivate once the envelope has effectively reached silence (~5 time-constants).
    if (std::exp(-t_ / tau) < 0.002) {
        active_ = false;
    }
}

} // namespace maz::audio
