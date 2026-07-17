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
    v.phase2 = 0.0;
    v.subPhase = 0.0;
    v.modPhase = 0.0;
    v.targetFreq = midiToFreq(midi);
    // Glide: start at the previous note's pitch and slide to the target; otherwise start on pitch.
    v.freq = (glideSeconds_ > 0.0f && lastFreq_ > 0.0f) ? lastFreq_ : v.targetFreq;
    lastFreq_ = v.targetFreq;
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
            const double phaseInc = static_cast<double>(v.freq) / static_cast<double>(sampleRate);
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
                constexpr double kTwoPi = 6.283185307179586;
                const double mod = std::sin(v.modPhase * kTwoPi) * static_cast<double>(fmIndex_);
                osc = static_cast<float>(std::sin(v.phase * kTwoPi + mod));
                v.modPhase += phaseInc * static_cast<double>(fmRatio_);
                if (v.modPhase >= 1.0) {
                    v.modPhase -= std::floor(v.modPhase);
                }
            } else if (mode_ == SynthMode::Wavetable) {
                // Scan the morphing table; the amp envelope can sweep the position for movement.
                const float pos = wtPosition_ + wtMorphEnv_ * v.env;
                osc = wavetable_.sample(pos, v.phase);
            } else {
                osc = waveSample(waveform_, v.phase);
                if (osc2Level_ > 0.0f) {
                    osc += waveSample(waveform_, v.phase2) * osc2Level_;
                    const double detune = std::pow(2.0, static_cast<double>(detuneCents_) / 1200.0);
                    v.phase2 += phaseInc * detune;
                    if (v.phase2 >= 1.0) {
                        v.phase2 -= std::floor(v.phase2);
                    }
                }
                if (subLevel_ > 0.0f) {
                    constexpr double kTwoPi = 6.283185307179586;
                    osc += static_cast<float>(std::sin(v.subPhase * kTwoPi)) * subLevel_;
                    v.subPhase += phaseInc * 0.5; // one octave down
                    if (v.subPhase >= 1.0) {
                        v.subPhase -= std::floor(v.subPhase);
                    }
                }
                if (noiseLevel_ > 0.0f) {
                    v.rng ^= v.rng << 13;
                    v.rng ^= v.rng >> 17;
                    v.rng ^= v.rng << 5;
                    osc += (static_cast<float>(v.rng) / 2147483648.0f - 1.0f) * noiseLevel_;
                }
            }

            // Resonant low-pass (subtractive character), with the amp envelope opening the cutoff.
            if (filterCutoff_ < 19000.0f) {
                const float cutoff = filterCutoff_ + filterEnvAmt_ * v.env;
                osc = v.filter.process(osc, cutoff, filterReso_, sampleRate,
                                       StateVariableFilter::Mode::LowPass);
            }

            out[i] += osc * v.env * v.velocity * gain_;

            v.phase += phaseInc;
            if (v.phase >= 1.0) {
                v.phase -= 1.0;
            }

            if (v.stage == Stage::Off) {
                break; // voice finished mid-block; rest of its samples are silence
            }
        }
    }
}

} // namespace maz::audio
