#include "maz/audio/Sampler.hpp"

#include "maz/audio/Pitch.hpp"
#include "maz/audio/WavReader.hpp"

#include <algorithm>

namespace maz::audio {

namespace {
constexpr float kAttack = 0.001f;  // seconds
constexpr float kRelease = 0.012f; // seconds
} // namespace

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
    v.pos = reverse_ ? static_cast<double>(sample_.size() - 1) : 0.0;
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
    const float attackStep = 1.0f / (kAttack * static_cast<float>(sampleRate));
    const float releaseStep = 1.0f / (kRelease * static_cast<float>(sampleRate));
    const double baseFreq = static_cast<double>(midiToFreq(basePitch_));
    const size_t last = sample_.size() - 1;

    for (Voice& v : voices_) {
        if (!v.active) {
            continue;
        }
        const double rate =
            static_cast<double>(midiToFreq(v.midi)) / baseFreq * srCorrect; // read speed
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

            // Bounds / looping, direction-aware: forward stops (or wraps) at the end, reverse at
            // the start.
            const double dlast = static_cast<double>(last);
            if (!reverse_) {
                if (v.pos >= dlast) {
                    if (loop_) {
                        v.pos -= dlast;
                    } else {
                        v.active = false;
                        break;
                    }
                }
            } else {
                if (v.pos < 0.0) {
                    if (loop_) {
                        v.pos += dlast;
                    } else {
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

            v.pos += reverse_ ? -rate : rate;
        }
    }
}

} // namespace maz::audio
