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
    v.modPhase = 0.0;
    v.freq = midiToFreq(midi);
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.env = 0.0f;
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
        const double phaseInc = static_cast<double>(v.freq) / static_cast<double>(sampleRate);
        for (int i = 0; i < frames; ++i) {
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
            } else {
                osc = waveSample(waveform_, v.phase);
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
