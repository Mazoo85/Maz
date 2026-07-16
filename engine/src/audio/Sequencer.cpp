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
    grid_.assign(static_cast<size_t>(channelCount) * static_cast<size_t>(numSteps_), 0);
}

double Sequencer::samplesPerStep(int sampleRate) const {
    // beats/sec = bpm/60; steps/sec = beats/sec * stepsPerBeat; samples/step = sampleRate / steps-sec.
    const double stepsPerSec = (bpm_ / 60.0) * static_cast<double>(stepsPerBeat_);
    return static_cast<double>(sampleRate) / stepsPerSec;
}

bool Sequencer::step(int channel, int step) const {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return false;
    }
    return grid_[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
                 static_cast<size_t>(step)] != 0;
}

void Sequencer::setStep(int channel, int step, bool on) {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return;
    }
    grid_[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
          static_cast<size_t>(step)] = on ? 1u : 0u;
}

void Sequencer::toggle(int channel, int step) {
    setStep(channel, step, !this->step(channel, step));
}

void Sequencer::clear() {
    std::fill(grid_.begin(), grid_.end(), static_cast<uint8_t>(0));
}

void Sequencer::triggerStep(int step) {
    for (int c = 0; c < numChannels(); ++c) {
        if (this->step(c, step)) {
            channels_[static_cast<size_t>(c)].trigger();
        }
    }
}

void Sequencer::play() {
    playing_ = true;
    currentStep_ = 0;
    samplesIntoStep_ = 0.0;
    triggerStep(0);
}

void Sequencer::stop() {
    playing_ = false;
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
            const double sps = samplesPerStep(sampleRate);
            int toNext = static_cast<int>(std::ceil(sps - samplesIntoStep_));
            if (toNext < 1) {
                toNext = 1;
            }
            chunk = std::min(chunk, toNext);
        }

        // Sum the (active) channel voices into a scratch buffer, then soft-limit the bus with
        // tanh before adding to the output. This keeps single hits punchy (near-linear at low
        // level) while several simultaneous drums no longer sum past full scale and clip.
        mixScratch_.assign(static_cast<size_t>(chunk), 0.0f);
        for (DrumVoice& voice : channels_) {
            voice.render(mixScratch_.data(), chunk, sampleRate);
        }
        for (int i = 0; i < chunk; ++i) {
            out[done + i] += static_cast<float>(std::tanh(static_cast<double>(mixScratch_[static_cast<size_t>(i)])));
        }

        if (playing_) {
            samplesIntoStep_ += static_cast<double>(chunk);
            const double sps = samplesPerStep(sampleRate);
            if (samplesIntoStep_ + 0.5 >= sps) {
                samplesIntoStep_ -= sps;
                currentStep_ = (currentStep_ + 1) % numSteps_;
                triggerStep(currentStep_);
            }
        }

        done += chunk;
    }
}

} // namespace maz::audio
