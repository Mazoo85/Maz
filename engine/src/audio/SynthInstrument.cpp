#include "maz/audio/SynthInstrument.hpp"

#include "maz/audio/Pitch.hpp"

#include <algorithm>

namespace maz::audio {

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

void SynthInstrument::noteOn(int midi, float velocity) {
    // Prefer a free voice; otherwise steal the quietest one so a new note always sounds.
    int chosen = -1;
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
    Voice& v = voices_[static_cast<size_t>(chosen)];
    v.stage = Stage::Attack;
    v.midi = midi;
    v.phase = 0.0;
    for (int u = 0; u < kMaxUnison; ++u) {
        v.uniPhase[static_cast<size_t>(u)] = static_cast<double>(u) / kMaxUnison; // decorrelate
    }
    v.phase2 = 0.0;
    v.subPhase = 0.0;
    v.modPhase = 0.0;
    v.targetFreq = midiToFreq(midi);
    // Glide: start at the previous note's pitch and slide to the target; otherwise start on pitch.
    v.freq = (glideSeconds_ > 0.0f && lastFreq_ > 0.0f) ? lastFreq_ : v.targetFreq;
    lastFreq_ = v.targetFreq;
    v.pitchEnv = pitchEnvAmt_; // seed the pitch envelope (decays to 0 in render)
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.env = 0.0f;
    v.filter.reset();
}

void SynthInstrument::noteOff(int midi) {
    for (Voice& v : voices_) {
        if (v.midi == midi && v.stage != Stage::Off && v.stage != Stage::Release) {
            v.stage = Stage::Release;
        }
    }
}

void SynthInstrument::allNotesOff() {
    for (Voice& v : voices_) {
        if (v.stage != Stage::Off) {
            v.stage = Stage::Release;
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

void SynthInstrument::render(float* out, int frames, int sampleRate) {
    if (frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float attackStep = 1.0f / (attack_ * sr);
    const float decayStep = (1.0f - sustain_) / (decay_ * sr);
    const float releaseStep = sustain_ > 0.0f ? sustain_ / (release_ * sr) : 1.0f / (release_ * sr);
    // Vibrato LFO (shared across voices): a per-block start phase so every voice wavers together.
    constexpr double kTwoPiVib = 6.283185307179586;
    const double vibInc = static_cast<double>(vibRate_) / static_cast<double>(sampleRate);
    // Wavetable scan LFO (shared across voices), same block-start-phase scheme as the vibrato.
    const double wtLfoInc = static_cast<double>(wtLfoRate_) / static_cast<double>(sampleRate);
    // Pitch-envelope decay coefficient (one time-constant = pitchEnvTime_).
    const float pitchEnvCoef = std::exp(-1.0f / (pitchEnvTime_ * sr));

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
                const double vp = vibPhase_ + static_cast<double>(i) * vibInc;
                vibMul = std::pow(2.0, static_cast<double>(vibDepth_) *
                                           std::sin(vp * kTwoPiVib) / 1200.0);
            }
            // Pitch envelope: apply the current offset, then decay it toward 0.
            double pitchMul = 1.0;
            if (v.pitchEnv != 0.0f) {
                pitchMul = std::pow(2.0, static_cast<double>(v.pitchEnv) / 12.0);
                v.pitchEnv *= pitchEnvCoef;
                if (std::fabs(v.pitchEnv) < 1e-4f) {
                    v.pitchEnv = 0.0f;
                }
            }
            const double phaseInc =
                static_cast<double>(v.freq) * vibMul * pitchMul / static_cast<double>(sampleRate);
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
                const double mod = m * static_cast<double>(fmIndex_);
                osc = static_cast<float>(std::sin(v.phase * kTwoPi + mod));
                v.modPhase += phaseInc * static_cast<double>(fmRatio_);
                if (v.modPhase >= 1.0) {
                    v.modPhase -= std::floor(v.modPhase);
                }
            } else if (mode_ == SynthMode::Wavetable) {
                // Scan the morphing table; the amp envelope and a dedicated LFO can sweep the
                // position for continuous movement.
                float pos = wtPosition_ + wtMorphEnv_ * v.env;
                if (wtLfoDepth_ > 0.0f) {
                    const double lp = wtLfoPhase_ + static_cast<double>(i) * wtLfoInc;
                    const float lfoU = 0.5f + 0.5f * static_cast<float>(std::sin(lp * kTwoPiVib));
                    pos += wtLfoDepth_ * lfoU;
                }
                osc = wavetable_.sample(pos, v.phase);
            } else {
                if (unisonVoices_ > 1) {
                    // Supersaw: sum detuned copies spread ±unisonDetune_ cents, equal-power scaled.
                    float acc = 0.0f;
                    const int uv = unisonVoices_;
                    const float uniGain = 1.0f / std::sqrt(static_cast<float>(uv));
                    for (int u = 0; u < uv; ++u) {
                        const double spread = static_cast<double>(u) / (uv - 1) - 0.5; // -0.5..0.5
                        const double mul =
                            std::pow(2.0, spread * 2.0 * static_cast<double>(unisonDetune_) / 1200.0);
                        acc += waveSample(waveform_, v.uniPhase[static_cast<size_t>(u)], pulseWidth_);
                        v.uniPhase[static_cast<size_t>(u)] += phaseInc * mul;
                        if (v.uniPhase[static_cast<size_t>(u)] >= 1.0) {
                            v.uniPhase[static_cast<size_t>(u)] -=
                                std::floor(v.uniPhase[static_cast<size_t>(u)]);
                        }
                    }
                    osc = acc * uniGain;
                } else {
                    osc = waveSample(waveform_, v.phase, pulseWidth_);
                }
                if (osc2Level_ > 0.0f || ringMod_ > 0.0f) {
                    const float o1 = osc; // the primary oscillator, before osc2 is mixed in
                    const float o2 = waveSample(waveform_, v.phase2, pulseWidth_);
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
                if (subLevel_ > 0.0f) {
                    osc += waveSample(subWave_, v.subPhase) * subLevel_;
                    v.subPhase += phaseInc * 0.5; // one octave down
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
                const float cutoff =
                    filterCutoff_ + filterEnvAmt_ * v.env + velCutoff_ * v.velocity;
                osc = v.filter.process(osc, cutoff, filterReso_, sampleRate,
                                       StateVariableFilter::Mode::LowPass);
            }

            out[i] += osc * v.env * v.velocity * gain_;

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
}

} // namespace maz::audio
