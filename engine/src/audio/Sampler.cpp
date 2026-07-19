#include "maz/audio/Sampler.hpp"

#include "maz/audio/Pitch.hpp"
#include "maz/audio/WavReader.hpp"

#include <algorithm>
#include <cmath>

namespace maz::audio {

void Sampler::setAmpEnv(float attackSec, float releaseSec) {
    attack_ = attackSec < 0.0001f ? 0.0001f : attackSec;
    release_ = releaseSec < 0.0001f ? 0.0001f : releaseSec;
}

bool Sampler::load(const std::string& path, std::string* err) {
    WavData wav;
    if (!readWav16(path, wav, err)) {
        return false;
    }
    sample_ = wav.toMono();
    sampleSr_ = wav.sampleRate;
    path_ = path;
    return !sample_.empty();
}

void Sampler::setSampleMono(std::vector<float> mono, int sampleRate) {
    sample_ = std::move(mono);
    sampleSr_ = sampleRate > 0 ? sampleRate : 48000;
    path_.clear();
}

float Sampler::samplePeak() const {
    float peak = 0.0f;
    for (float s : sample_) {
        const float a = std::fabs(s);
        if (a > peak) {
            peak = a;
        }
    }
    return peak;
}

void Sampler::normalize() {
    const float peak = samplePeak();
    if (peak <= 0.0f) {
        return; // empty or silent → nothing to scale
    }
    const float g = 1.0f / peak;
    for (float& s : sample_) {
        s *= g;
    }
}

void Sampler::fadeEdges(float ms) {
    if (sample_.empty() || ms <= 0.0f) {
        return;
    }
    int fade = static_cast<int>(ms * 0.001f * static_cast<float>(sampleSr_));
    const int half = static_cast<int>(sample_.size()) / 2;
    if (fade > half) {
        fade = half; // never overlap the two fades
    }
    if (fade < 1) {
        return;
    }
    const size_t n = sample_.size();
    for (int i = 0; i < fade; ++i) {
        const float g = static_cast<float>(i) / static_cast<float>(fade);
        sample_[static_cast<size_t>(i)] *= g;               // fade in from the start
        sample_[n - 1 - static_cast<size_t>(i)] *= g;       // fade out toward the end
    }
}

int Sampler::crossfadeLoop(float ms) {
    if (sample_.empty() || ms <= 0.0f) {
        return 0;
    }
    const int n = static_cast<int>(sample_.size());
    const int ls = static_cast<int>(loopStart_ * static_cast<float>(n)); // loop start frame
    const int le = static_cast<int>(loopEnd_ * static_cast<float>(n));   // loop end frame
    if (le <= ls) {
        return 0;
    }
    int fade = static_cast<int>(ms * 0.001f * static_cast<float>(sampleSr_));
    // Need `fade` frames of pre-roll before the loop start to blend from, and the crossfade must fit
    // inside the loop region.
    if (fade > ls) {
        fade = ls;
    }
    if (fade > le - ls) {
        fade = le - ls;
    }
    if (fade < 1) {
        return 0; // no room (e.g. loopStart at 0) → nothing to blend
    }
    // Snapshot the two source spans first (they may overlap the region we overwrite).
    std::vector<float> tail(static_cast<size_t>(fade));   // frames approaching loopEnd
    std::vector<float> pre(static_cast<size_t>(fade));    // frames approaching loopStart
    for (int k = 0; k < fade; ++k) {
        tail[static_cast<size_t>(k)] = sample_[static_cast<size_t>(le - fade + k)];
        pre[static_cast<size_t>(k)] = sample_[static_cast<size_t>(ls - fade + k)];
    }
    // Blend the tail into the pre-roll so that as playback nears loopEnd it morphs into the content
    // just before loopStart — the wrap loopEnd→loopStart then continues seamlessly.
    for (int k = 0; k < fade; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(fade); // 0 → 1
        sample_[static_cast<size_t>(le - fade + k)] =
            tail[static_cast<size_t>(k)] * (1.0f - t) + pre[static_cast<size_t>(k)] * t;
    }
    return fade;
}

void Sampler::noteOn(int midi, float velocity) {
    if (sample_.empty()) {
        return;
    }
    // A note is "legato" (for the legato-glide option) when it starts while another is still held.
    bool legatoActive = false;
    for (const Voice& ov : voices_) {
        if (ov.active && !ov.releasing) {
            legatoActive = true;
            break;
        }
    }
    int chosen = -1;
    if (mono_) {
        // Last-note priority: silence every voice and always (re)use voice 0, so only one sounds.
        for (Voice& ov : voices_) {
            ov.active = false;
        }
        chosen = 0;
    } else {
        float lowest = 2.0f;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (!voices_[static_cast<size_t>(i)].active) {
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
    v.active = true;
    v.releasing = false;
    v.midi = midi;
    const double last = static_cast<double>(sample_.size() - 1);
    if (slices_ > 1) {
        // Beat slicer: map the note (relative to the base) to a slice and play it once, forward, at
        // natural speed. The slice runs from its start up to the next slice boundary.
        const double total = static_cast<double>(sample_.size());
        const double sliceLen = total / static_cast<double>(slices_);
        int idx = midi - basePitch_;
        idx = idx < 0 ? 0 : (idx >= slices_ ? slices_ - 1 : idx);
        v.sliced = true;
        v.pos = static_cast<double>(idx) * sliceLen;
        v.sliceEnd = std::min(total, static_cast<double>(idx + 1) * sliceLen);
        v.dir = 1;
    } else {
        // Start reading from the offset; in reverse, from the end minus the offset.
        const double offset = static_cast<double>(startOffset_) * last;
        v.sliced = false;
        v.sliceEnd = 0.0;
        v.pos = reverse_ ? (last - offset) : offset;
        v.dir = reverse_ ? -1 : 1;
    }
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.env = 0.0f;
    v.ampStage = 0;  // amp attack
    v.filtEnv = 0.0f;
    v.filtStage = 0; // attack
    v.penv = 1.0;    // pitch envelope starts fully offset, slides to 0 (true pitch)

    // Portamento: pitched (key-tracked, non-sliced) notes carry a pitch ratio relative to the base;
    // sliced / fixed-pitch playback stays at ratio 1 so it is unaffected. When gliding, the read speed
    // starts at the previous pitched note's ratio and eases to this note's ratio in render().
    const bool pitched = !v.sliced && keyTrack_;
    const double noteRatio =
        pitched ? static_cast<double>(midiToFreq(midi)) / static_cast<double>(midiToFreq(basePitch_))
                : 1.0;
    const bool gliding = glideSeconds_ > 0.0f && pitched && (!glideLegato_ || legatoActive);
    v.noteRatio = noteRatio;
    v.glideRatio = gliding ? lastNoteRatio_ : noteRatio;
    if (pitched) {
        lastNoteRatio_ = noteRatio;
    }
}

void Sampler::noteOff(int midi) {
    for (Voice& v : voices_) {
        if (v.active && v.midi == midi) {
            v.releasing = true;
            v.filtStage = 3; // filter envelope enters release
        }
    }
}

void Sampler::allNotesOff() {
    for (Voice& v : voices_) {
        if (v.active) {
            v.releasing = true;
            v.filtStage = 3;
        }
    }
}

bool Sampler::active() const {
    for (const Voice& v : voices_) {
        if (v.active) {
            return true;
        }
    }
    return false;
}

int Sampler::activeVoices() const {
    int n = 0;
    for (const Voice& v : voices_) {
        if (v.active) {
            ++n;
        }
    }
    return n;
}

void Sampler::render(float* out, int frames, int sampleRate) {
    if (sample_.empty() || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const double srCorrect = static_cast<double>(sampleSr_) / static_cast<double>(sampleRate);
    const float attackStep = 1.0f / (attack_ * static_cast<float>(sampleRate));
    const float ampDecStep = (1.0f - ampSustain_) / (ampDecay_ * static_cast<float>(sampleRate));
    // Release ramps from the sustain level to 0 over release_ seconds (so the time is honoured
    // regardless of how low the sustain sits). A zero sustain falls back to a full 1→0 ramp time.
    const float releaseStep =
        (ampSustain_ > 0.0f ? ampSustain_ : 1.0f) / (release_ * static_cast<float>(sampleRate));
    // Filter-envelope per-sample increments (only used when the envelope has a non-zero depth).
    const bool useFilterEnv = filterEnvDepth_ != 0.0f;
    const bool useFilterVelo = filterVelo_ != 0.0f;
    // Pitch envelope: a linear slide of the initial pitch offset back to the true pitch.
    const bool usePitchEnv = pitchEnvDepth_ != 0.0f;
    const double penvStep = 1.0 / (static_cast<double>(pitchEnvTime_) * static_cast<double>(sampleRate));
    const float fAtkStep = 1.0f / (fEnvA_ * static_cast<float>(sampleRate));
    const float fDecStep = (1.0f - fEnvS_) / (fEnvD_ * static_cast<float>(sampleRate));
    const float fRelStep = (fEnvS_ > 0.0f ? fEnvS_ : 1.0f) / (fEnvR_ * static_cast<float>(sampleRate));
    const size_t last = sample_.size() - 1;
    // Portamento one-pole coefficient: how much of the remaining pitch gap survives each sample.
    // 0 (glide off) snaps instantly; the read speed uses v.glideRatio, which equals v.noteRatio when
    // glide is off, so playback stays bit-identical to before.
    const double glideCoeff =
        glideSeconds_ > 0.0f
            ? std::exp(-1.0 / (static_cast<double>(glideSeconds_) * static_cast<double>(sampleRate)))
            : 0.0;

    for (Voice& v : voices_) {
        if (!v.active) {
            continue;
        }
        const double detuneMul = std::pow(2.0, static_cast<double>(detuneCents_) / 1200.0);
        for (int i = 0; i < frames; ++i) {
            // Amp ADSR: attack up to 1, decay down to the sustain level, hold, then release on noteOff.
            if (v.releasing) {
                v.env -= releaseStep;
                if (v.env <= 0.0f) {
                    v.env = 0.0f;
                    v.active = false;
                    break;
                }
            } else {
                switch (v.ampStage) {
                case 0: // attack → 1
                    // Velocity → attack: softer hits take a smaller step (a longer swell). At amount 0
                    // the step is exactly attackStep, so the envelope is bit-for-bit unchanged.
                    v.env += velToAttack_ > 0.0f
                                 ? attackStep / (1.0f + velToAttack_ * (1.0f - v.velocity) * 4.0f)
                                 : attackStep;
                    if (v.env >= 1.0f) {
                        v.env = 1.0f;
                        v.ampStage = 1;
                    }
                    break;
                case 1: // decay → sustain
                    v.env -= ampDecStep;
                    if (v.env <= ampSustain_) {
                        v.env = ampSustain_;
                        v.ampStage = 2;
                    }
                    break;
                default: // sustain: hold at the sustain level
                    break;
                }
            }

            // Filter envelope (independent ADSR driving the cutoff).
            if (useFilterEnv) {
                switch (v.filtStage) {
                case 0: // attack → 1
                    v.filtEnv += fAtkStep;
                    if (v.filtEnv >= 1.0f) {
                        v.filtEnv = 1.0f;
                        v.filtStage = 1;
                    }
                    break;
                case 1: // decay → sustain
                    v.filtEnv -= fDecStep;
                    if (v.filtEnv <= fEnvS_) {
                        v.filtEnv = fEnvS_;
                        v.filtStage = 2;
                    }
                    break;
                case 3: // release → 0
                    v.filtEnv -= fRelStep;
                    if (v.filtEnv < 0.0f) {
                        v.filtEnv = 0.0f;
                    }
                    break;
                default: // sustain: hold
                    break;
                }
            }

            // Bounds / looping, direction-aware. Ping-pong reflects off each end (flipping the
            // direction); a plain loop wraps; a one-shot stops. Forward voices watch the end, reverse
            // voices the start. When looping, the wrap/reflect boundaries are the loop region
            // [loopStart, loopEnd] rather than the whole sample, so the attack head plays once and just
            // the region sustains; when not looping they are the whole sample (a one-shot).
            // Sliced voices are a plain forward one-shot bounded by the slice end — no loop/reverse.
            if (v.sliced) {
                if (v.pos >= v.sliceEnd) {
                    v.active = false;
                    break;
                }
            } else {
            const double dlast = static_cast<double>(last);
            const double lo = loop_ ? static_cast<double>(loopStart_) * dlast : 0.0;
            const double hi = loop_ ? static_cast<double>(loopEnd_) * dlast : dlast;
            const double span = hi - lo;
            if (pingPong_ && loop_) {
                if (v.pos >= hi) {
                    v.pos = hi - (v.pos - hi); // reflect back inside the region
                    if (v.pos < lo) {
                        v.pos = lo;
                    }
                    v.dir = -1;
                } else if (v.pos <= lo) {
                    v.pos = lo + (lo - v.pos);
                    if (v.pos > hi) {
                        v.pos = hi;
                    }
                    v.dir = 1;
                }
            } else if (v.dir > 0) {
                if (v.pos >= hi) {
                    if (loop_ && span > 0.0) {
                        v.pos -= span;
                    } else if (v.pos >= dlast) {
                        v.active = false;
                        break;
                    }
                }
            } else {
                if (v.pos < lo) {
                    if (loop_ && span > 0.0) {
                        v.pos += span;
                    } else if (v.pos < 0.0) {
                        v.active = false;
                        break;
                    }
                }
            }
            } // end non-sliced bounds

            size_t i0 = static_cast<size_t>(v.pos);
            if (i0 >= last) {
                i0 = last - 1; // keep i0+1 in range for interpolation
            }
            const float frac = static_cast<float>(v.pos - static_cast<double>(i0));
            float s = sample_[i0] * (1.0f - frac) + sample_[i0 + 1] * frac;
            // Playback low-pass (per voice): shape the sample's tone, with the optional filter
            // envelope sweeping the cutoff. Bypassed only when the base is open and no envelope is set.
            if (filterCutoff_ < 19000.0f || useFilterEnv || useFilterVelo) {
                float cutoff = filterCutoff_;
                if (useFilterEnv) {
                    cutoff += filterEnvDepth_ * v.filtEnv;
                }
                if (useFilterVelo) {
                    cutoff += filterVelo_ * v.velocity; // harder hits open the filter
                }
                cutoff = cutoff < 20.0f ? 20.0f : (cutoff > 20000.0f ? 20000.0f : cutoff);
                s = v.filter.process(s, cutoff, filterReso_, sampleRate,
                                     StateVariableFilter::Mode::LowPass);
            }
            // Drive: push through a tanh soft-clipper (normalised so full-scale stays ~unity) to warm
            // the sample / add grit. Skipped at 0 so the clean sample is bit-identical.
            if (drive_ > 0.0f) {
                const float k = 1.0f + drive_ * 8.0f;
                s = std::tanh(s * k) / std::tanh(k);
            }
            // Velocity → volume: blend between full level and velocity-scaled by velSens_.
            const float velGain = 1.0f - velSens_ * (1.0f - v.velocity);
            out[i] += s * v.env * velGain * gain_;

            // Glide: ease the read-speed ratio toward this note's target ratio (a no-op when glide is
            // off, since glideRatio already equals noteRatio). Then map the ratio to a read speed.
            if (glideSeconds_ > 0.0f) {
                v.glideRatio = v.noteRatio + (v.glideRatio - v.noteRatio) * glideCoeff;
            }
            // Pitch envelope: scale the read speed by the (decaying) semitone offset, then advance
            // the slide toward 0. Off → curRate == the glided base rate, so playback is bit-identical.
            double curRate = v.glideRatio * srCorrect * detuneMul;
            if (usePitchEnv) {
                curRate *= std::pow(2.0, static_cast<double>(pitchEnvDepth_) * v.penv / 12.0);
                v.penv -= penvStep;
                if (v.penv < 0.0) {
                    v.penv = 0.0;
                }
            }
            v.pos += static_cast<double>(v.dir) * curRate;
        }
    }
}

} // namespace maz::audio
