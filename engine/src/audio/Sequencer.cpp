#include "maz/audio/Sequencer.hpp"

#include <algorithm>
#include <cmath>

namespace maz::audio {

namespace {
struct ChannelDef {
    const char* name;
    Drum drum;
    float gain;
};
// The default kit. Order defines the channel indices used by the UI and demo patterns.
constexpr ChannelDef kKit[] = {
    {"Kick", Drum::Kick, 0.95f},      {"Snare", Drum::Snare, 0.75f},
    {"Closed Hat", Drum::ClosedHat, 0.45f}, {"Open Hat", Drum::OpenHat, 0.40f},
    {"Clap", Drum::Clap, 0.60f},
};
} // namespace

Sequencer::Sequencer() {
    const int channelCount = static_cast<int>(sizeof(kKit) / sizeof(kKit[0]));
    channels_.resize(static_cast<size_t>(channelCount));
    names_.resize(static_cast<size_t>(channelCount));
    for (int c = 0; c < channelCount; ++c) {
        channels_[static_cast<size_t>(c)].setType(kKit[c].drum);
        channels_[static_cast<size_t>(c)].setGain(kKit[c].gain);
        names_[static_cast<size_t>(c)] = kKit[c].name;
    }
    chanVolume_.assign(static_cast<size_t>(channelCount), 1.0f);
    chanMute_.assign(static_cast<size_t>(channelCount), 0);
    chanSolo_.assign(static_cast<size_t>(channelCount), 0);
    addPattern(); // start with one empty pattern
}

void Sequencer::setChannelVolume(int c, float v) {
    if (c >= 0 && c < numChannels()) {
        chanVolume_[static_cast<size_t>(c)] = v;
    }
}
void Sequencer::setChannelMute(int c, bool m) {
    if (c >= 0 && c < numChannels()) {
        chanMute_[static_cast<size_t>(c)] = m ? 1u : 0u;
    }
}
void Sequencer::setChannelSolo(int c, bool s) {
    if (c >= 0 && c < numChannels()) {
        chanSolo_[static_cast<size_t>(c)] = s ? 1u : 0u;
    }
}
float Sequencer::channelVolume(int c) const {
    return (c >= 0 && c < numChannels()) ? chanVolume_[static_cast<size_t>(c)] : 1.0f;
}
bool Sequencer::channelMute(int c) const {
    return c >= 0 && c < numChannels() && chanMute_[static_cast<size_t>(c)] != 0;
}
bool Sequencer::channelSolo(int c) const {
    return c >= 0 && c < numChannels() && chanSolo_[static_cast<size_t>(c)] != 0;
}

int Sequencer::addPattern() {
    Pattern p;
    p.grid.assign(static_cast<size_t>(numChannels()) * static_cast<size_t>(numSteps_), 0);
    patterns_.push_back(std::move(p));
    return static_cast<int>(patterns_.size()) - 1;
}

void Sequencer::selectPattern(int i) {
    if (i >= 0 && i < patternCount()) {
        current_ = i;
    }
}

void Sequencer::clearArrangement() {
    patterns_.clear();
    playlist_.clear();
    songMode_ = false;
    playlistPos_ = 0;
    current_ = 0;
    addPattern();
}

void Sequencer::setSwing(float s) {
    swing_ = std::clamp(s, 0.0f, 0.9f);
}

double Sequencer::samplesPerStep(int sampleRate, int step) const {
    // beats/sec = bpm/60; steps/sec = beats/sec * stepsPerBeat; samples/step = sampleRate / steps-sec.
    const double stepsPerSec = (bpm_ / 60.0) * static_cast<double>(stepsPerBeat_);
    const double base = static_cast<double>(sampleRate) / stepsPerSec;
    // Swing: even steps get (1 + swing), odd steps (1 - swing) — a pair still sums to 2·base.
    const double factor = (step % 2 == 0) ? (1.0 + static_cast<double>(swing_))
                                          : (1.0 - static_cast<double>(swing_));
    return base * factor;
}

bool Sequencer::step(int channel, int step) const {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return false;
    }
    return patterns_[static_cast<size_t>(current_)]
               .grid[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
                     static_cast<size_t>(step)] != 0;
}

void Sequencer::setStep(int channel, int step, bool on) {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return;
    }
    patterns_[static_cast<size_t>(current_)]
        .grid[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
              static_cast<size_t>(step)] = on ? 1u : 0u;
}

void Sequencer::toggle(int channel, int step) {
    setStep(channel, step, !this->step(channel, step));
}

void Sequencer::clear() {
    Pattern& p = patterns_[static_cast<size_t>(current_)];
    std::fill(p.grid.begin(), p.grid.end(), static_cast<uint8_t>(0));
}

void Sequencer::triggerStep(int step) {
    // Drums: strike every channel switched on at this step.
    for (int c = 0; c < numChannels(); ++c) {
        if (this->step(c, step)) {
            channels_[static_cast<size_t>(c)].trigger();
        }
    }
    // Melody: note-offs first (so a note ending where another begins doesn't cut the new one),
    // then note-ons for notes starting on this step. Note ends wrap within the bar. The piano roll
    // drives the sampler when it's engaged (and loaded), otherwise the synth.
    const bool toSampler = useSampler_ && sampler_.loaded();
    const PianoRoll& roll = patterns_[static_cast<size_t>(current_)].roll;
    for (const Note& n : roll.notes()) {
        const int endStep = (n.startStep + n.lengthSteps) % numSteps_;
        if (endStep == step) {
            if (toSampler) {
                sampler_.noteOff(n.pitch);
            } else {
                synth_.noteOff(n.pitch);
            }
        }
    }
    for (const Note& n : roll.notes()) {
        if (n.startStep == step) {
            if (toSampler) {
                sampler_.noteOn(n.pitch, n.velocity);
            } else {
                synth_.noteOn(n.pitch, n.velocity);
            }
        }
    }
}

void Sequencer::play() {
    playing_ = true;
    currentStep_ = 0;
    samplesIntoStep_ = 0.0;
    playlistPos_ = 0;
    if (songMode_ && !playlist_.empty()) {
        selectPattern(playlist_[0]); // start the arrangement at the first playlist entry
    }
    triggerStep(0);
}

void Sequencer::stop() {
    playing_ = false;
    synth_.allNotesOff(); // let held notes release rather than hang
    sampler_.allNotesOff();
}

void Sequencer::render(float* out, int frames, int sampleRate) {
    if (frames <= 0 || sampleRate <= 0) {
        return;
    }
    int done = 0;
    while (done < frames) {
        int chunk = frames - done;

        // While playing, never render past the next step boundary so triggers stay sample-accurate.
        if (playing_) {
            const double sps = samplesPerStep(sampleRate, currentStep_);
            int toNext = static_cast<int>(std::ceil(sps - samplesIntoStep_));
            if (toNext < 1) {
                toNext = 1;
            }
            chunk = std::min(chunk, toNext);
        }

        // Sum the drums and the synth on their own buses (each with its own gain), combine, then
        // soft-limit with tanh before adding to the output. This keeps single hits punchy
        // (near-linear at low level) while stacked voices no longer sum past full scale and clip.
        mixScratch_.assign(static_cast<size_t>(chunk), 0.0f);
        synthScratch_.assign(static_cast<size_t>(chunk), 0.0f);
        bool anySolo = false;
        for (uint8_t s : chanSolo_) {
            anySolo = anySolo || s != 0;
        }
        for (int c = 0; c < numChannels(); ++c) {
            const bool audible =
                chanMute_[static_cast<size_t>(c)] == 0 &&
                (!anySolo || chanSolo_[static_cast<size_t>(c)] != 0);
            if (!audible) {
                continue; // muted, or another channel is soloed
            }
            channels_[static_cast<size_t>(c)].setLevel(chanVolume_[static_cast<size_t>(c)]);
            channels_[static_cast<size_t>(c)].render(mixScratch_.data(), chunk, sampleRate);
        }
        synth_.render(synthScratch_.data(), chunk, sampleRate);
        sampler_.render(synthScratch_.data(), chunk, sampleRate);
        for (int i = 0; i < chunk; ++i) {
            const float mixed = mixScratch_[static_cast<size_t>(i)] * drumGain_ +
                                synthScratch_[static_cast<size_t>(i)] * synthGain_;
            out[done + i] += static_cast<float>(std::tanh(static_cast<double>(mixed)));
        }

        if (playing_) {
            samplesIntoStep_ += static_cast<double>(chunk);
            const double sps = samplesPerStep(sampleRate, currentStep_);
            if (samplesIntoStep_ + 0.5 >= sps) {
                samplesIntoStep_ -= sps;
                currentStep_ = (currentStep_ + 1) % numSteps_;
                // At the top of each bar, in song mode, advance to the next playlist pattern.
                if (currentStep_ == 0 && songMode_ && !playlist_.empty()) {
                    playlistPos_ = (playlistPos_ + 1) % static_cast<int>(playlist_.size());
                    selectPattern(playlist_[static_cast<size_t>(playlistPos_)]);
                }
                triggerStep(currentStep_);
            }
        }

        done += chunk;
    }
}

} // namespace maz::audio
