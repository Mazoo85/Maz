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

void Sampler::noteOn(int midi, float velocity) {
    if (sample_.empty()) {
        return;
    }
    int chosen = -1;
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
    Voice& v = voices_[static_cast<size_t>(chosen)];
    v.active = true;
    v.releasing = false;
    v.midi = midi;
    // Start reading from the offset; in reverse, from the end minus the offset.
    const double last = static_cast<double>(sample_.size() - 1);
    const double offset = static_cast<double>(startOffset_) * last;
    v.pos = reverse_ ? (last - offset) : offset;
    v.dir = reverse_ ? -1 : 1;
    v.velocity = std::clamp(velocity, 0.0f, 1.0f);
    v.env = 0.0f;
}

void Sampler::noteOff(int midi) {
    for (Voice& v : voices_) {
        if (v.active && v.midi == midi) {
            v.releasing = true;
        }
    }
}

void Sampler::allNotesOff() {
    for (Voice& v : voices_) {
        if (v.active) {
            v.releasing = true;
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

void Sampler::render(float* out, int frames, int sampleRate) {
    if (sample_.empty() || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const double srCorrect = static_cast<double>(sampleSr_) / static_cast<double>(sampleRate);
    const float attackStep = 1.0f / (attack_ * static_cast<float>(sampleRate));
    const float releaseStep = 1.0f / (release_ * static_cast<float>(sampleRate));
    const double baseFreq = static_cast<double>(midiToFreq(basePitch_));
    const size_t last = sample_.size() - 1;

    for (Voice& v : voices_) {
        if (!v.active) {
            continue;
        }
        const double rate = static_cast<double>(midiToFreq(v.midi)) / baseFreq * srCorrect *
                            std::pow(2.0, static_cast<double>(detuneCents_) / 1200.0); // read speed
        for (int i = 0; i < frames; ++i) {
            // Amp envelope: quick attack up, fast release when noteOff'd.
            if (v.releasing) {
                v.env -= releaseStep;
                if (v.env <= 0.0f) {
                    v.env = 0.0f;
                    v.active = false;
                    break;
                }
            } else if (v.env < 1.0f) {
                v.env = std::min(1.0f, v.env + attackStep);
            }

            // Bounds / looping, direction-aware. Ping-pong reflects off each end (flipping the
            // direction); a plain loop wraps; a one-shot stops. Forward voices watch the end, reverse
            // voices the start. When looping, the wrap/reflect boundaries are the loop region
            // [loopStart, loopEnd] rather than the whole sample, so the attack head plays once and just
            // the region sustains; when not looping they are the whole sample (a one-shot).
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

            size_t i0 = static_cast<size_t>(v.pos);
            if (i0 >= last) {
                i0 = last - 1; // keep i0+1 in range for interpolation
            }
            const float frac = static_cast<float>(v.pos - static_cast<double>(i0));
            const float s = sample_[i0] * (1.0f - frac) + sample_[i0 + 1] * frac;
            out[i] += s * v.env * v.velocity * gain_;

            v.pos += static_cast<double>(v.dir) * rate;
        }
    }
}

} // namespace maz::audio
