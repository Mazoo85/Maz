#include "maz/audio/SynthInstrument.hpp"

#include "maz/audio/Effects.hpp" // modSyncRateHz for the tempo-synced cutoff LFO
#include "maz/audio/Pitch.hpp"

#include <algorithm>
#include <cstdint>

namespace maz::audio {

namespace {
// Deterministic sample & hold: a well-distributed value in [-1,1) that holds for each integer cycle
// of the phase `p` (same floor(p) → same value), matching the LFO struct's random-stepped mode.
float sampleHoldValue(double p) {
    const long long step = static_cast<long long>(std::floor(p));
    uint32_t x = static_cast<uint32_t>(step) * 2654435761u + 0x9E3779B9u;
    x ^= x >> 15;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return static_cast<float>(x) / 2147483648.0f - 1.0f;
}
} // namespace

void SynthInstrument::setEnvelope(float attack, float decay, float sustain, float release) {
    attack_ = std::max(attack, 0.0001f);
    decay_ = std::max(decay, 0.0001f);
    sustain_ = std::clamp(sustain, 0.0f, 1.0f);
    release_ = std::max(release, 0.0001f);
}

void SynthInstrument::setFilter(float cutoffHz, float resonance, float envAmt) {
    filterCutoff_ = std::clamp(cutoffHz, 20.0f, 20000.0f);
    filterReso_ = std::clamp(resonance, 0.5f, 20.0f);
    filterEnvAmt_ = envAmt;
}

void SynthInstrument::setFilterEnvelope(float attack, float decay, float sustain, float release) {
    filtA_ = std::max(attack, 0.0001f);
    filtD_ = std::max(decay, 0.0001f);
    filtS_ = std::clamp(sustain, 0.0f, 1.0f);
    filtR_ = std::max(release, 0.0001f);
}

void SynthInstrument::setUnison(int voices, float detuneCents) {
    unisonVoices_ = voices < 1 ? 1 : (voices > kMaxUnison ? kMaxUnison : voices);
    unisonDetune_ = detuneCents < 0.0f ? 0.0f : (detuneCents > 100.0f ? 100.0f : detuneCents);
}

void SynthInstrument::setOscillators(float detuneCents, float osc2Level, float subLevel,
                                     float noiseLevel) {
    detuneCents_ = std::clamp(detuneCents, 0.0f, 100.0f);
    osc2Level_ = std::clamp(osc2Level, 0.0f, 1.0f);
    subLevel_ = std::clamp(subLevel, 0.0f, 1.0f);
    noiseLevel_ = std::clamp(noiseLevel, 0.0f, 1.0f);
}

void SynthInstrument::updateTempo(double bpm) {
    if (bpm <= 0.0) {
        return;
    }
    if (filterLfoSync_) {
        filterLfoRate_ = modSyncRateHz(filterLfoSyncDiv_, bpm);
    }
    if (ampLfoSync_) {
        ampLfoRate_ = modSyncRateHz(ampLfoSyncDiv_, bpm);
    }
    if (vibSync_) {
        vibRate_ = modSyncRateHz(vibSyncDiv_, bpm);
    }
}

void SynthInstrument::noteOn(int midi, float velocity, float fineCents) {
    // Was another note being held (not yet released) when this one started? Used by legato-only glide.
    bool wasHeld = false;
    for (const Voice& vv : voices_) {
        if (vv.stage != Stage::Off && vv.stage != Stage::Release) {
            wasHeld = true;
            break;
        }
    }
    int chosen = 0;
    if (mono_) {
        // Monophonic: always the one voice (voice 0); release any others still ringing.
        for (int i = 1; i < kMaxVoices; ++i) {
            if (voices_[static_cast<size_t>(i)].stage != Stage::Off) {
                voices_[static_cast<size_t>(i)].stage = Stage::Release;
            }
        }
    } else {
        // Prefer a free voice; otherwise steal the quietest one so a new note always sounds.
        float lowest = 2.0f;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (voices_[static_cast<size_t>(i)].stage == Stage::Off) {
                chosen = i;
                break;
            }
            if (voices_[static_cast<size_t>(i)].env < lowest) {
                lowest = voices_[static_cast<size_t>(i)].env;
                chosen = i;
            }
        }
    }
    Voice& v = voices_[static_cast<size_t>(chosen)];
    v.stage = Stage::Attack;
    v.midi = midi;
    // Start-phase randomization: begin each oscillator at a random phase (scaled by phaseRandom_) so
    // repeated notes have different transients; 0 = the classic phase-coherent start at 0.
    auto nextPhase = [&]() -> double {
        if (phaseRandom_ <= 0.0f) {
            return 0.0;
        }
        phaseRng_ ^= phaseRng_ << 13;
        phaseRng_ ^= phaseRng_ >> 17;
        phaseRng_ ^= phaseRng_ << 5;
        return static_cast<double>(phaseRng_) / 4294967295.0 * static_cast<double>(phaseRandom_);
    };
    v.phase = nextPhase();
    for (int u = 0; u < kMaxUnison; ++u) {
        v.uniPhase[static_cast<size_t>(u)] = static_cast<double>(u) / kMaxUnison; // decorrelate
    }
    v.phase2 = nextPhase();
    v.phase3 = nextPhase();
    v.subPhase = nextPhase();
    v.modPhase = 0.0;
    v.targetFreq = midiToFreq(midi);
    // Glide: start at the previous note's pitch and slide to the target; otherwise start on pitch.
    // Legato mode only glides when another note was already held (fingered portamento).
    const bool doGlide =
        glideSeconds_ > 0.0f && lastFreq_ > 0.0f && (!glideLegato_ || wasHeld);
    v.freq = doGlide ? lastFreq_ : v.targetFreq;
    lastFreq_ = v.targetFreq;
    v.pitchEnv = pitchEnvAmt_; // seed the pitch envelope (decays to 0 in render)
    // Analog drift: detune this note by a small random amount within ±drift_ cents (deterministic).
    float detuneCents = fineCents; // start from the per-note fine tune
    if (drift_ > 0.0f) {
        driftRng_ ^= driftRng_ << 13;
        driftRng_ ^= driftRng_ >> 17;
        driftRng_ ^= driftRng_ << 5;
        const float r = static_cast<float>(driftRng_) / 4294967295.0f * 2.0f - 1.0f; // [-1,1]
        detuneCents += drift_ * r;
    }
    v.driftMul = detuneCents != 0.0f ? std::pow(2.0f, detuneCents / 1200.0f) : 1.0f;
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.env = 0.0f;
    v.ageSamples = 0.0;
    v.filtStage = Stage::Attack;
    v.filtEnv = 0.0f;
    v.filter.reset();
    v.filter2.reset();
    v.ksInit = true;     // (Pluck mode) re-excite the string on the next render sample
    v.percEnv = 1.0f;    // (Organ mode) seed the percussion key-click transient
}

void SynthInstrument::noteOff(int midi) {
    for (Voice& v : voices_) {
        if (v.midi == midi && v.stage != Stage::Off && v.stage != Stage::Release) {
            v.stage = Stage::Release;
            v.filtStage = Stage::Release;
        }
    }
}

void SynthInstrument::allNotesOff() {
    for (Voice& v : voices_) {
        if (v.stage != Stage::Off) {
            v.stage = Stage::Release;
            v.filtStage = Stage::Release;
        }
    }
}

bool SynthInstrument::active() const {
    for (const Voice& v : voices_) {
        if (v.stage != Stage::Off) {
            return true;
        }
    }
    return false;
}

int SynthInstrument::activeVoices() const {
    int n = 0;
    for (const Voice& v : voices_) {
        if (v.stage != Stage::Off) {
            ++n;
        }
    }
    return n;
}

void SynthInstrument::render(float* out, int frames, int sampleRate) {
    if (frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float attackStep = 1.0f / (attack_ * sr);
    const float decayStep = (1.0f - sustain_) / (decay_ * sr);
    const float releaseStep = sustain_ > 0.0f ? sustain_ / (release_ * sr) : 1.0f / (release_ * sr);
    // Dedicated filter-envelope ADSR increments (only used when its depth is non-zero).
    const float fAttackStep = 1.0f / (filtA_ * sr);
    const float fDecayStep = (1.0f - filtS_) / (filtD_ * sr);
    const float fReleaseStep = filtS_ > 0.0f ? filtS_ / (filtR_ * sr) : 1.0f / (filtR_ * sr);
    const bool useFilterEnv = filterEnvDepth_ != 0.0f;
    // Vibrato LFO (shared across voices): a per-block start phase so every voice wavers together.
    constexpr double kTwoPiVib = 6.283185307179586;
    const double vibInc = static_cast<double>(vibRate_) / static_cast<double>(sampleRate);
    // Wavetable scan LFO (shared across voices), same block-start-phase scheme as the vibrato.
    const double wtLfoInc = static_cast<double>(wtLfoRate_) / static_cast<double>(sampleRate);
    // Filter cutoff LFO (shared across voices), same block-start-phase scheme.
    const double filtLfoInc = static_cast<double>(filterLfoRate_) / static_cast<double>(sampleRate);
    // Amplitude LFO / tremolo (shared across voices), same block-start-phase scheme.
    const double ampLfoInc = static_cast<double>(ampLfoRate_) / static_cast<double>(sampleRate);
    // Pulse-width (PWM) LFO (shared across voices), same block-start-phase scheme.
    const double pwmLfoInc = static_cast<double>(pwmLfoRate_) / static_cast<double>(sampleRate);
    // Pitch-envelope decay coefficient (one time-constant = pitchEnvTime_).
    const float pitchEnvCoef = std::exp(-1.0f / (pitchEnvTime_ * sr));
    // Per-instrument octave shift as a frequency multiplier (2^octave).
    const double octMul = std::pow(2.0, static_cast<double>(octave_));

    for (Voice& v : voices_) {
        if (v.stage == Stage::Off) {
            continue;
        }
        // One-pole glide coefficient toward the target pitch (0 → instant when glide is off).
        const float glideCoef =
            glideSeconds_ > 0.0f ? (1.0f - std::exp(-1.0f / (glideSeconds_ * sr))) : 1.0f;
        for (int i = 0; i < frames; ++i) {
            // Portamento: slide the current frequency toward the note's target each sample.
            v.freq += (v.targetFreq - v.freq) * glideCoef;
            double vibMul = 1.0;
            if (vibDepth_ > 0.0f) {
                // Onset delay: hold vibrato off until vibDelay_ elapses for this note, then fade it in
                // over ~50 ms. 0 delay → full vibrato immediately.
                float vibOnset = 1.0f;
                if (vibDelay_ > 0.0f) {
                    const float ageSec = static_cast<float>(v.ageSamples) / sr;
                    vibOnset = (ageSec - vibDelay_) * 20.0f; // 1/0.05 s fade
                    vibOnset = vibOnset < 0.0f ? 0.0f : (vibOnset > 1.0f ? 1.0f : vibOnset);
                }
                if (vibOnset > 0.0f) {
                    double vp = vibPhase_ + static_cast<double>(i) * vibInc;
                    vp -= std::floor(vp); // wrap into [0,1) for the (non-sine) shapes
                    vibMul = std::pow(2.0, static_cast<double>(vibDepth_ * vibOnset) *
                                               static_cast<double>(waveSample(vibShape_, vp)) / 1200.0);
                }
            }
            v.ageSamples += 1.0;
            // Pitch envelope: apply the current offset, then decay it toward 0.
            double pitchMul = 1.0;
            if (v.pitchEnv != 0.0f) {
                pitchMul = std::pow(2.0, static_cast<double>(v.pitchEnv) / 12.0);
                v.pitchEnv *= pitchEnvCoef;
                if (std::fabs(v.pitchEnv) < 1e-4f) {
                    v.pitchEnv = 0.0f;
                }
            }
            const double phaseInc = static_cast<double>(v.freq) * vibMul * pitchMul * octMul *
                                    static_cast<double>(v.driftMul) / static_cast<double>(sampleRate);
            switch (v.stage) {
            case Stage::Attack:
                v.env += attackStep;
                if (v.env >= 1.0f) {
                    v.env = 1.0f;
                    v.stage = Stage::Decay;
                }
                break;
            case Stage::Decay:
                v.env -= decayStep;
                if (v.env <= sustain_) {
                    v.env = sustain_;
                    v.stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                break;
            case Stage::Release:
                v.env -= releaseStep;
                if (v.env <= 0.0f) {
                    v.env = 0.0f;
                    v.stage = Stage::Off;
                }
                break;
            case Stage::Off:
                break;
            }

            // Advance the dedicated filter envelope (its own ADSR) in lockstep with the note.
            if (useFilterEnv) {
                switch (v.filtStage) {
                case Stage::Attack:
                    v.filtEnv += fAttackStep;
                    if (v.filtEnv >= 1.0f) {
                        v.filtEnv = 1.0f;
                        v.filtStage = Stage::Decay;
                    }
                    break;
                case Stage::Decay:
                    v.filtEnv -= fDecayStep;
                    if (v.filtEnv <= filtS_) {
                        v.filtEnv = filtS_;
                        v.filtStage = Stage::Sustain;
                    }
                    break;
                case Stage::Sustain:
                    break;
                case Stage::Release:
                    v.filtEnv -= fReleaseStep;
                    if (v.filtEnv <= 0.0f) {
                        v.filtEnv = 0.0f;
                        v.filtStage = Stage::Off;
                    }
                    break;
                case Stage::Off:
                    break;
                }
            }

            float osc;
            if (mode_ == SynthMode::FM) {
                // 2-operator FM: a sine modulator at ratio×carrier phase-modulates a sine carrier.
                // Feedback routes the modulator's own previous output back into its phase (up to ±π
                // rad), adding progressively richer, sawtooth-like harmonics — the classic FM edge.
                constexpr double kTwoPi = 6.283185307179586;
                constexpr double kPi = 3.141592653589793;
                const double fb = static_cast<double>(fmFeedback_) * kPi * static_cast<double>(v.fmFb);
                const double m = std::sin(v.modPhase * kTwoPi + fb);
                v.fmFb = static_cast<float>(m);
                // Velocity → FM index: harder notes push the modulation depth up (brighter).
                const float effIndex = fmIndex_ + velFmIndex_ * v.velocity;
                const double mod = m * static_cast<double>(effIndex);
                osc = static_cast<float>(std::sin(v.phase * kTwoPi + mod));
                v.modPhase += phaseInc * static_cast<double>(fmRatio_);
                if (v.modPhase >= 1.0) {
                    v.modPhase -= std::floor(v.modPhase);
                }
            } else if (mode_ == SynthMode::Wavetable) {
                // Scan the morphing table; the amp envelope and a dedicated LFO can sweep the
                // position for continuous movement.
                float pos = wtPosition_ + wtMorphEnv_ * v.env + velWavePos_ * v.velocity;
                if (wtLfoDepth_ > 0.0f) {
                    const double lp = wtLfoPhase_ + static_cast<double>(i) * wtLfoInc;
                    const float lfoU = 0.5f + 0.5f * static_cast<float>(std::sin(lp * kTwoPiVib));
                    pos += wtLfoDepth_ * lfoU;
                }
                osc = wavetable_.sample(pos, v.phase);
            } else if (mode_ == SynthMode::Pluck) {
                // Karplus-Strong plucked string: a one-period delay line excited with a noise burst
                // on note-on, then repeatedly averaged with its neighbour (a one-zero low-pass) so the
                // burst decays into a warm, string-like pluck whose pitch is set by the line length.
                if (v.ksInit) {
                    int n = static_cast<int>(std::lround(static_cast<double>(sampleRate) /
                                                         std::max(20.0f, v.targetFreq)));
                    if (n < 2) {
                        n = 2;
                    }
                    v.ksBuf.assign(static_cast<size_t>(n), 0.0f);
                    for (int k = 0; k < n; ++k) {
                        v.rng ^= v.rng << 13;
                        v.rng ^= v.rng >> 17;
                        v.rng ^= v.rng << 5;
                        v.ksBuf[static_cast<size_t>(k)] =
                            static_cast<float>(v.rng) / 2147483648.0f - 1.0f;
                    }
                    // Pluck position: comb-filter the excitation (x[k] - x[k-D]) so the harmonics with
                    // a node at the strike point are nulled — centre plucks lose the even harmonics.
                    if (pluckPosition_ > 0.0f) {
                        int d = static_cast<int>(std::lround(static_cast<double>(pluckPosition_) * n));
                        if (d < 1) {
                            d = 1;
                        }
                        if (d > n - 1) {
                            d = n - 1;
                        }
                        const std::vector<float> src = v.ksBuf;
                        for (int k = 0; k < n; ++k) {
                            v.ksBuf[static_cast<size_t>(k)] =
                                src[static_cast<size_t>(k)] -
                                src[static_cast<size_t>(((k - d) % n + n) % n)];
                        }
                    }
                    v.ksPtr = 0;
                    v.ksInit = false;
                }
                const int n = static_cast<int>(v.ksBuf.size());
                const int cur = v.ksPtr;
                const int nxt = (cur + 1) % n;
                osc = v.ksBuf[static_cast<size_t>(cur)];
                // One-zero low-pass average = the string's natural decay; the damping control bleeds a
                // little extra energy out each pass for a faster, darker, more muted pluck.
                const float avg =
                    0.5f * (v.ksBuf[static_cast<size_t>(cur)] + v.ksBuf[static_cast<size_t>(nxt)]);
                v.ksBuf[static_cast<size_t>(cur)] = avg * (1.0f - 0.02f * pluckDamping_);
                v.ksPtr = nxt;
            } else if (mode_ == SynthMode::Organ) {
                // Additive drawbar organ: sum sines at harmonics 1..8 of the note, each scaled by its
                // drawbar level (a tonewheel-organ tone). All harmonics ride the fundamental phase, so
                // they stay phase-coherent and continuous across its wrap.
                constexpr double kTwoPi = 6.283185307179586;
                float o = 0.0f;
                for (int h = 0; h < kOrganBars; ++h) {
                    const float lvl = organBars_[static_cast<size_t>(h)];
                    if (lvl > 0.0f) {
                        o += lvl * static_cast<float>(std::sin(v.phase * kTwoPi * (h + 1)));
                    }
                }
                // Percussion (key-click): a fast-decaying 2nd/3rd-harmonic ping on the attack.
                if (organPercAmt_ > 0.0f && v.percEnv > 0.0001f) {
                    const int ph = organPercThird_ ? 3 : 2;
                    o += organPercAmt_ * v.percEnv *
                         static_cast<float>(std::sin(v.phase * kTwoPi * ph));
                    // ~0.15 s decay time constant.
                    v.percEnv *= std::exp(-1.0f / (0.15f * static_cast<float>(sampleRate)));
                }
                osc = o * 0.35f; // headroom for the summed partials
            } else if (mode_ == SynthMode::PhaseDistortion) {
                // Casio CZ-style phase distortion: warp the linear phase through a two-segment map
                // (a movable midpoint `m`) so the cosine cycle is squeezed into the first segment and
                // stretched over the second. At m = 0.5 the map is identity → a pure sine; as the
                // amount pushes m toward 0 the asymmetry injects progressively brighter harmonics —
                // a smooth sine→saw morph. Rides the shared phase, so glide/vibrato/drift still apply.
                constexpr double kTwoPi = 6.283185307179586;
                const double m = 0.5 * (1.0 - static_cast<double>(pdAmount_) * 0.98);
                const double p = v.phase;
                const double pp = (p < m) ? 0.5 * p / m : 0.5 + 0.5 * (p - m) / (1.0 - m);
                osc = static_cast<float>(-std::cos(pp * kTwoPi));
            } else {
                // Pulse-width, optionally swept by the PWM LFO (square-wave duty movement).
                float pw = pulseWidth_;
                if (pwmLfoDepth_ > 0.0f) {
                    const double pp = pwmLfoPhase_ + static_cast<double>(i) * pwmLfoInc;
                    pw = pulseWidth_ + pwmLfoDepth_ * static_cast<float>(std::sin(pp * kTwoPiVib));
                    pw = pw < 0.02f ? 0.02f : (pw > 0.98f ? 0.98f : pw);
                }
                if (unisonVoices_ > 1) {
                    // Supersaw: sum detuned copies spread ±unisonDetune_ cents, equal-power scaled.
                    float acc = 0.0f;
                    const int uv = unisonVoices_;
                    const float uniGain = 1.0f / std::sqrt(static_cast<float>(uv));
                    for (int u = 0; u < uv; ++u) {
                        const double spread = static_cast<double>(u) / (uv - 1) - 0.5; // -0.5..0.5
                        const double mul =
                            std::pow(2.0, spread * 2.0 * static_cast<double>(unisonDetune_) / 1200.0);
                        acc += waveSample(waveform_, v.uniPhase[static_cast<size_t>(u)], pw);
                        v.uniPhase[static_cast<size_t>(u)] += phaseInc * mul;
                        if (v.uniPhase[static_cast<size_t>(u)] >= 1.0) {
                            v.uniPhase[static_cast<size_t>(u)] -=
                                std::floor(v.uniPhase[static_cast<size_t>(u)]);
                        }
                    }
                    osc = acc * uniGain;
                } else {
                    osc = waveSample(waveform_, v.phase, pw);
                }
                if (osc2Level_ > 0.0f || ringMod_ > 0.0f) {
                    const float o1 = osc; // the primary oscillator, before osc2 is mixed in
                    const Waveform o2Wave = osc2WaveLinked_ ? waveform_ : osc2Waveform_;
                    const float o2 = waveSample(o2Wave, v.phase2, pw);
                    osc += o2 * osc2Level_;
                    // Ring modulation: add the product of the two oscillators for metallic,
                    // inharmonic (sum/difference) partials. 0 = off.
                    if (ringMod_ > 0.0f) {
                        osc += ringMod_ * o1 * o2;
                    }
                    // Hard sync: the slave runs at the sync ratio (reset on master wrap below);
                    // otherwise it is a plain detuned oscillator (coarse semitones + fine cents).
                    const double mul =
                        hardSync_
                            ? static_cast<double>(syncRatio_)
                            : std::pow(2.0, (static_cast<double>(osc2Semitones_) * 100.0 +
                                             static_cast<double>(detuneCents_)) /
                                                1200.0);
                    v.phase2 += phaseInc * mul;
                    if (v.phase2 >= 1.0) {
                        v.phase2 -= std::floor(v.phase2);
                    }
                }
                if (osc3Level_ > 0.0f) {
                    // A third oscillator stacked a fixed interval away (coarse semitones), with its
                    // own (or primary-linked) waveform.
                    const Waveform o3Wave = osc3WaveLinked_ ? waveform_ : osc3Waveform_;
                    osc += waveSample(o3Wave, v.phase3, pw) * osc3Level_;
                    const double mul3 =
                        std::pow(2.0, (static_cast<double>(osc3Semitones_) * 100.0 +
                                       static_cast<double>(osc3FineCents_)) /
                                          1200.0);
                    v.phase3 += phaseInc * mul3;
                    if (v.phase3 >= 1.0) {
                        v.phase3 -= std::floor(v.phase3);
                    }
                }
                if (subLevel_ > 0.0f) {
                    osc += waveSample(subWave_, v.subPhase) * subLevel_;
                    const double subMul = subOctave_ == 2 ? 0.25 : 0.5; // one or two octaves down
                    v.subPhase += phaseInc * subMul;
                    if (v.subPhase >= 1.0) {
                        v.subPhase -= std::floor(v.subPhase);
                    }
                }
                if (noiseLevel_ > 0.0f) {
                    v.rng ^= v.rng << 13;
                    v.rng ^= v.rng >> 17;
                    v.rng ^= v.rng << 5;
                    const float white = static_cast<float>(v.rng) / 2147483648.0f - 1.0f;
                    // Tone control: blend white with a one-pole low-passed (darker) copy.
                    v.noiseLp += 0.15f * (white - v.noiseLp);
                    const float shaped = white * (1.0f - noiseColor_) + v.noiseLp * noiseColor_;
                    osc += shaped * noiseLevel_;
                }
            }

            // Resonant low-pass (subtractive character): the amp envelope and the note's velocity
            // both open the cutoff (velocity sensitivity → harder hits sound brighter).
            if (filterCutoff_ < 19000.0f) {
                float cutoff = filterCutoff_ + filterEnvAmt_ * v.env + velCutoff_ * v.velocity;
                // Dedicated filter envelope: sweep the cutoff by its own ADSR × depth (Hz).
                if (useFilterEnv) {
                    cutoff += filterEnvDepth_ * v.filtEnv;
                }
                // Keyboard tracking: raise the cutoff with the note's pitch (relative to middle C) so
                // high notes stay bright. At amount 1 the cutoff tracks pitch fully (an octave up
                // doubles it); 0 = fixed cutoff.
                if (filterKeyTrack_ > 0.0f) {
                    cutoff *= std::pow(2.0f, filterKeyTrack_ * static_cast<float>(v.midi - 60) / 12.0f);
                }
                // Cutoff LFO: sweep the cutoff up/down by ±depth octaves for wobble/auto-wah movement.
                if (filterLfoDepth_ > 0.0f) {
                    const double fp = filterLfoPhase_ + static_cast<double>(i) * filtLfoInc;
                    // Sample & hold jumps to a new random level each cycle; else read the periodic shape.
                    const float lfoVal = filterLfoSampleHold_
                                             ? sampleHoldValue(fp)
                                             : waveSample(filterLfoShape_, fp - std::floor(fp));
                    cutoff *= std::pow(2.0f, filterLfoDepth_ * lfoVal);
                }
                cutoff = std::clamp(cutoff, 20.0f, 20000.0f);
                // Filter drive: overdrive the signal into the filter (tanh) for harmonics/grit before
                // it is filtered — the classic analog driven-filter growl. 0 = clean (unchanged).
                if (filterDrive_ > 0.0f) {
                    osc = std::tanh(osc * (1.0f + filterDrive_ * 5.0f));
                }
                osc = v.filter.process(osc, cutoff, filterReso_, sampleRate, filterMode_);
                // 24 dB/oct: a second identical stage cascades for a steeper Moog-style rolloff.
                if (filter24_) {
                    osc = v.filter2.process(osc, cutoff, filterReso_, sampleRate, filterMode_);
                }
            }

            // Noise attack transient: a fast-decaying white-noise burst on each note's onset — a
            // percussive click/chiff added post-filter so it stays bright. Uses the voice's own RNG.
            if (noiseAttackAmt_ > 0.0f) {
                const float nenv = std::exp(-static_cast<float>(v.ageSamples) /
                                            (noiseAttackDecayMs_ * 0.001f * sr));
                if (nenv > 0.0005f) {
                    v.rng ^= v.rng << 13;
                    v.rng ^= v.rng >> 17;
                    v.rng ^= v.rng << 5;
                    const float wn = static_cast<float>(v.rng) / 2147483648.0f - 1.0f;
                    osc += noiseAttackAmt_ * nenv * wn;
                }
            }

            // Velocity → amplitude, scaled by sensitivity: at 1 the velocity fully sets loudness, at
            // 0 every note is equally loud regardless of how hard it was played.
            const float velAmp = (1.0f - velSens_) + velSens_ * v.velocity;
            // Amplitude LFO (tremolo): a level dip that swings between full and (1 − depth).
            float ampMod = 1.0f;
            if (ampLfoDepth_ > 0.0f) {
                double ap = ampLfoPhase_ + static_cast<double>(i) * ampLfoInc;
                ap -= std::floor(ap); // wrap into [0,1) for the (non-sine) shapes
                const float lfoU = 0.5f + 0.5f * waveSample(ampLfoShape_, ap); // unipolar [0,1]
                ampMod = 1.0f - ampLfoDepth_ * lfoU;
            }
            out[i] += osc * v.env * velAmp * gain_ * ampMod;

            v.phase += phaseInc;
            if (v.phase >= 1.0) {
                v.phase -= 1.0;
                if (hardSync_) {
                    v.phase2 = 0.0; // slave restarts every master cycle → the sync timbre
                }
            }

            if (v.stage == Stage::Off) {
                break; // voice finished mid-block; rest of its samples are silence
            }
        }
    }

    // Advance the shared vibrato phase by one block so it stays continuous across render calls.
    vibPhase_ += vibInc * static_cast<double>(frames);
    if (vibPhase_ >= 1.0) {
        vibPhase_ -= std::floor(vibPhase_);
    }
    // Likewise the wavetable scan LFO.
    wtLfoPhase_ += wtLfoInc * static_cast<double>(frames);
    if (wtLfoPhase_ >= 1.0) {
        wtLfoPhase_ -= std::floor(wtLfoPhase_);
    }
    // Likewise the filter cutoff LFO.
    filterLfoPhase_ += filtLfoInc * static_cast<double>(frames);
    if (filterLfoPhase_ >= 1.0) {
        filterLfoPhase_ -= std::floor(filterLfoPhase_);
    }
    // Likewise the amplitude LFO.
    ampLfoPhase_ += ampLfoInc * static_cast<double>(frames);
    if (ampLfoPhase_ >= 1.0) {
        ampLfoPhase_ -= std::floor(ampLfoPhase_);
    }
    pwmLfoPhase_ += pwmLfoInc * static_cast<double>(frames);
    if (pwmLfoPhase_ >= 1.0) {
        pwmLfoPhase_ -= std::floor(pwmLfoPhase_);
    }
}

} // namespace maz::audio
