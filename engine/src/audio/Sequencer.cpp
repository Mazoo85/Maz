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
    chanPan_.assign(static_cast<size_t>(channelCount), 0.0f);
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
void Sequencer::setChannelPan(int c, float p) {
    if (c >= 0 && c < numChannels()) {
        chanPan_[static_cast<size_t>(c)] = std::clamp(p, -1.0f, 1.0f);
    }
}
float Sequencer::channelPan(int c) const {
    return (c >= 0 && c < numChannels()) ? chanPan_[static_cast<size_t>(c)] : 0.0f;
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

void Sequencer::setSidechain(bool on, float amount, float releaseMs) {
    sidechainOn_ = on;
    scAmount_ = std::clamp(amount, 0.0f, 1.0f);
    scReleaseMs_ = std::max(releaseMs, 1.0f);
}

void Sequencer::setArp(bool on, int mode) {
    arpOn_ = on;
    arpMode_ = (mode < 0 || mode > 2) ? 0 : mode;
    arpCounter_ = 0;
}

void Sequencer::setHumanize(float amount) {
    humanize_ = std::clamp(amount, 0.0f, 1.0f);
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
              static_cast<size_t>(step)] = on ? 255u : 0u;
}

float Sequencer::stepVelocity(int channel, int step) const {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return 0.0f;
    }
    return static_cast<float>(patterns_[static_cast<size_t>(current_)]
                                  .grid[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
                                        static_cast<size_t>(step)]) /
           255.0f;
}

void Sequencer::setStepVelocity(int channel, int step, float velocity) {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return;
    }
    const float v = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
    patterns_[static_cast<size_t>(current_)]
        .grid[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
              static_cast<size_t>(step)] = static_cast<uint8_t>(v * 255.0f + 0.5f);
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
            float vel = stepVelocity(c, step);
            if (humanize_ > 0.0f) {
                // Deterministic per-hit jitter: reduce velocity by up to 60% of the humanize amount.
                uint32_t h = humanizeCounter_ * 2654435761u + static_cast<uint32_t>(c) * 40503u;
                h ^= h >> 15;
                const float r = static_cast<float>(h & 0xFFFFu) / 65536.0f;
                vel *= 1.0f - humanize_ * r * 0.6f;
                ++humanizeCounter_;
            }
            channels_[static_cast<size_t>(c)].trigger(vel);
        }
    }
    // Sidechain: a kick (channel 0) hit ducks the melodic bus.
    if (sidechainOn_ && this->step(0, step)) {
        scEnv_ = 1.0f - scAmount_;
    }
    const bool toSampler = useSampler_ && sampler_.loaded();
    const PianoRoll& roll = patterns_[static_cast<size_t>(current_)].roll;

    // Arpeggiator: instead of playing the roll notes directly, play one note per step from the set
    // of pitches held at this step, cycling through them per the mode.
    if (arpOn_) {
        std::vector<int> held;
        for (const Note& n : roll.notes()) {
            if (step >= n.startStep && step < n.startStep + n.lengthSteps) {
                held.push_back(n.pitch);
            }
        }
        if (arpCurrentPitch_ >= 0) {
            if (toSampler) {
                sampler_.noteOff(arpCurrentPitch_);
            } else {
                synth_.noteOff(arpCurrentPitch_);
            }
            arpCurrentPitch_ = -1;
        }
        if (!held.empty()) {
            std::sort(held.begin(), held.end());
            const int n = static_cast<int>(held.size());
            int index = 0;
            if (arpMode_ == 1) { // down
                index = (n - 1) - (arpCounter_ % n);
            } else if (arpMode_ == 2 && n > 1) { // up-down
                const int period = 2 * n - 2;
                const int pos = arpCounter_ % period;
                index = pos < n ? pos : period - pos;
            } else { // up
                index = arpCounter_ % n;
            }
            const int pitch = held[static_cast<size_t>(index)];
            if (toSampler) {
                sampler_.noteOn(pitch, 0.9f);
            } else {
                synth_.noteOn(pitch, 0.9f);
            }
            arpCurrentPitch_ = pitch;
            ++arpCounter_;
        }
        return; // arp replaces the normal note scheduling below
    }

    // Melody: note-offs first (so a note ending where another begins doesn't cut the new one),
    // then note-ons for notes starting on this step. Note ends wrap within the bar. The piano roll
    // drives the sampler when it's engaged (and loaded), otherwise the synth.
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

    // Second (bass) lane → synth2 (offs before ons).
    const PianoRoll& roll2 = patterns_[static_cast<size_t>(current_)].roll2;
    for (const Note& n : roll2.notes()) {
        if ((n.startStep + n.lengthSteps) % numSteps_ == step) {
            synth2_.noteOff(n.pitch);
        }
    }
    for (const Note& n : roll2.notes()) {
        if (n.startStep == step) {
            synth2_.noteOn(n.pitch, n.velocity);
        }
    }
}

void Sequencer::play() {
    playing_ = true;
    currentStep_ = 0;
    samplesIntoStep_ = 0.0;
    playlistPos_ = 0;
    arpCounter_ = 0;
    arpCurrentPitch_ = -1;
    humanizeCounter_ = 0;
    metroLastStep_ = -1;
    metroEnv_ = 0.0f;
    if (songMode_ && !playlist_.empty()) {
        selectPattern(playlist_[0]); // start the arrangement at the first playlist entry
    }
    triggerStep(0);
}

void Sequencer::stop() {
    playing_ = false;
    synth_.allNotesOff(); // let held notes release rather than hang
    synth2_.allNotesOff();
    sampler_.allNotesOff();
}

void Sequencer::render(float* out, int frames, int sampleRate) {
    if (frames <= 0 || sampleRate <= 0) {
        return;
    }
    // Render the three buses separately, then sum them with the tanh bus soft-limit — punchy at low
    // level, no hard clipping when voices stack. Summing pre-limit stems reproduces the original
    // single-buffer behaviour exactly (tanh over drums + lead + bass).
    const size_t n = static_cast<size_t>(frames) * 2;
    stemDrums_.assign(n, 0.0f);
    stemLead_.assign(n, 0.0f);
    stemBass_.assign(n, 0.0f);
    renderStems(stemDrums_.data(), stemLead_.data(), stemBass_.data(), frames, sampleRate);
    for (size_t i = 0; i < n; ++i) {
        const double s = static_cast<double>(stemDrums_[i]) + static_cast<double>(stemLead_[i]) +
                         static_cast<double>(stemBass_[i]);
        out[i] += static_cast<float>(std::tanh(s));
    }
}

void Sequencer::renderStems(float* drums, float* lead, float* bass, int frames, int sampleRate) {
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

        // Metronome: fire an accented click when a new beat step begins (downbeat = brighter).
        if (playing_ && metronome_ && currentStep_ != metroLastStep_ &&
            (currentStep_ % stepsPerBeat_) == 0) {
            metroLastStep_ = currentStep_;
            metroEnv_ = 1.0f;
            metroFreq_ = (currentStep_ == 0) ? 1600.0f : 1000.0f;
            metroPhase_ = 0.0;
        }
        if (metronome_ && metroEnv_ > 0.0f) {
            constexpr double kTwoPi = 6.283185307179586;
            const double inc = static_cast<double>(metroFreq_) / static_cast<double>(sampleRate);
            const float decay = 1.0f / (0.04f * static_cast<float>(sampleRate)); // ~40 ms click
            for (int i = 0; i < chunk && metroEnv_ > 0.0f; ++i) {
                const float s = static_cast<float>(std::sin(metroPhase_ * kTwoPi)) * metroEnv_ * 0.5f;
                drums[2 * (done + i)] += s;
                drums[2 * (done + i) + 1] += s;
                metroPhase_ += inc;
                if (metroPhase_ >= 1.0) {
                    metroPhase_ -= 1.0;
                }
                metroEnv_ -= decay;
                if (metroEnv_ < 0.0f) {
                    metroEnv_ = 0.0f;
                }
            }
        }

        // Render each drum channel on its own so it can be panned into the stereo field, into the
        // drums bus; the lead (synth + sampler) and bass (synth2) buses render at center.
        synthScratch_.assign(static_cast<size_t>(chunk), 0.0f);
        bool anySolo = false;
        for (uint8_t s : chanSolo_) {
            anySolo = anySolo || s != 0;
        }
        constexpr float kHalfPi = 1.57079632679f;
        for (int c = 0; c < numChannels(); ++c) {
            const bool audible =
                chanMute_[static_cast<size_t>(c)] == 0 &&
                (!anySolo || chanSolo_[static_cast<size_t>(c)] != 0);
            if (!audible) {
                continue; // muted, or another channel is soloed
            }
            mixScratch_.assign(static_cast<size_t>(chunk), 0.0f);
            channels_[static_cast<size_t>(c)].setLevel(chanVolume_[static_cast<size_t>(c)]);
            channels_[static_cast<size_t>(c)].render(mixScratch_.data(), chunk, sampleRate);
            // Equal-power pan: angle 0..pi/2 as pan goes -1..+1.
            const float angle = (chanPan_[static_cast<size_t>(c)] + 1.0f) * 0.5f * kHalfPi;
            const float lg = std::cos(angle) * drumGain_;
            const float rg = std::sin(angle) * drumGain_;
            for (int i = 0; i < chunk; ++i) {
                const float s = mixScratch_[static_cast<size_t>(i)];
                drums[2 * (done + i)] += s * lg;
                drums[2 * (done + i) + 1] += s * rg;
            }
        }
        // Lead bus = synth + sampler; bass bus = synth2. Each has its own gain.
        synth_.render(synthScratch_.data(), chunk, sampleRate);
        sampler_.render(synthScratch_.data(), chunk, sampleRate);
        bassScratch_.assign(static_cast<size_t>(chunk), 0.0f);
        synth2_.render(bassScratch_.data(), chunk, sampleRate);
        constexpr float kCenter = 0.70710678f; // equal-power center gain
        const float scStep = 1.0f / (scReleaseMs_ * 0.001f * static_cast<float>(sampleRate));
        for (int i = 0; i < chunk; ++i) {
            const float duck = sidechainOn_ ? scEnv_ : 1.0f;
            const float leadS = synthScratch_[static_cast<size_t>(i)] * synthGain_ * kCenter * duck;
            const float bassS = bassScratch_[static_cast<size_t>(i)] * bassGain_ * kCenter * duck;
            lead[2 * (done + i)] += leadS;
            lead[2 * (done + i) + 1] += leadS;
            bass[2 * (done + i)] += bassS;
            bass[2 * (done + i) + 1] += bassS;
            if (sidechainOn_ && scEnv_ < 1.0f) {
                scEnv_ = std::min(1.0f, scEnv_ + scStep);
            }
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
