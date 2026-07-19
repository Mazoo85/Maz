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
    chanChoke_.assign(static_cast<size_t>(channelCount), 0);
    chanFlam_.assign(static_cast<size_t>(channelCount), 0.0f);
    // Default: the closed hat (2) and open hat (3) choke each other, like a real hi-hat.
    if (channelCount > 3) {
        chanChoke_[2] = 1;
        chanChoke_[3] = 1;
    }
    addPattern(); // start with one empty pattern
}

void Sequencer::setChannelChokeGroup(int c, int group) {
    if (c >= 0 && c < numChannels()) {
        chanChoke_[static_cast<size_t>(c)] = group < 0 ? 0 : group;
    }
}

int Sequencer::channelChokeGroup(int c) const {
    if (c >= 0 && c < numChannels()) {
        return chanChoke_[static_cast<size_t>(c)];
    }
    return 0;
}

void Sequencer::setNumSteps(int steps) {
    const int n = steps < 1 ? 1 : (steps > 64 ? 64 : steps);
    if (n == numSteps_) {
        return;
    }
    const int chans = numChannels();
    const int copy = std::min(n, numSteps_);
    // Re-lay-out every pattern's channel-major grids to the new step stride, keeping what fits.
    for (Pattern& p : patterns_) {
        std::vector<uint8_t> g(static_cast<size_t>(chans) * static_cast<size_t>(n), 0);
        std::vector<uint8_t> pr(static_cast<size_t>(chans) * static_cast<size_t>(n), 255);
        std::vector<uint8_t> rt(static_cast<size_t>(chans) * static_cast<size_t>(n), 1);
        for (int c = 0; c < chans; ++c) {
            for (int s = 0; s < copy; ++s) {
                const size_t src = static_cast<size_t>(c) * static_cast<size_t>(numSteps_) +
                                   static_cast<size_t>(s);
                const size_t dst =
                    static_cast<size_t>(c) * static_cast<size_t>(n) + static_cast<size_t>(s);
                if (src < p.grid.size()) {
                    g[dst] = p.grid[src];
                }
                if (src < p.prob.size()) {
                    pr[dst] = p.prob[src];
                }
                if (src < p.ratchet.size()) {
                    rt[dst] = p.ratchet[src];
                }
            }
        }
        p.grid = std::move(g);
        p.prob = std::move(pr);
        p.ratchet = std::move(rt);
    }
    numSteps_ = n;
    if (currentStep_ >= numSteps_) {
        currentStep_ = 0;
    }
}

void Sequencer::setChannelTune(int c, float semitones) {
    if (c >= 0 && c < numChannels()) {
        channels_[static_cast<size_t>(c)].setTune(semitones);
    }
}

float Sequencer::channelTune(int c) const {
    if (c >= 0 && c < numChannels()) {
        return channels_[static_cast<size_t>(c)].tune();
    }
    return 0.0f;
}

void Sequencer::setChannelDecay(int c, float mul) {
    if (c >= 0 && c < numChannels()) {
        channels_[static_cast<size_t>(c)].setDecay(mul);
    }
}

float Sequencer::channelDecay(int c) const {
    if (c >= 0 && c < numChannels()) {
        return channels_[static_cast<size_t>(c)].decay();
    }
    return 1.0f;
}

void Sequencer::setChannelDrive(int c, float drive) {
    if (c >= 0 && c < numChannels()) {
        channels_[static_cast<size_t>(c)].setDrive(drive);
    }
}

float Sequencer::channelDrive(int c) const {
    if (c >= 0 && c < numChannels()) {
        return channels_[static_cast<size_t>(c)].drive();
    }
    return 0.0f;
}

void Sequencer::setChannelFlam(int c, float ms) {
    if (c >= 0 && c < numChannels()) {
        chanFlam_[static_cast<size_t>(c)] = ms < 0.0f ? 0.0f : (ms > 50.0f ? 50.0f : ms);
    }
}

float Sequencer::channelFlam(int c) const {
    if (c >= 0 && c < numChannels()) {
        return chanFlam_[static_cast<size_t>(c)];
    }
    return 0.0f;
}

void Sequencer::setChannelType(int c, Drum type) {
    if (c >= 0 && c < numChannels()) {
        channels_[static_cast<size_t>(c)].setType(type);
    }
}

Drum Sequencer::channelType(int c) const {
    if (c >= 0 && c < numChannels()) {
        return channels_[static_cast<size_t>(c)].type();
    }
    return Drum::Kick;
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
    const size_t cells = static_cast<size_t>(numChannels()) * static_cast<size_t>(numSteps_);
    p.grid.assign(cells, 0);
    p.prob.assign(cells, 255); // every step defaults to "always fire"
    p.ratchet.assign(cells, 1); // one hit per step by default
    p.name = "Pattern " + std::to_string(patterns_.size() + 1);
    patterns_.push_back(std::move(p));
    return static_cast<int>(patterns_.size()) - 1;
}

int Sequencer::clonePattern(int src) {
    if (src < 0 || src >= patternCount()) {
        return addPattern();
    }
    Pattern copy = patterns_[static_cast<size_t>(src)]; // deep copy (grids + both rolls + name)
    copy.name = patterns_[static_cast<size_t>(src)].name + " copy";
    patterns_.push_back(std::move(copy));
    return static_cast<int>(patterns_.size()) - 1;
}

void Sequencer::setPatternName(int i, const std::string& name) {
    if (i >= 0 && i < patternCount()) {
        patterns_[static_cast<size_t>(i)].name = name;
    }
}

const std::string& Sequencer::patternName(int i) const {
    static const std::string kEmpty;
    if (i >= 0 && i < patternCount()) {
        return patterns_[static_cast<size_t>(i)].name;
    }
    return kEmpty;
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
    patterns_[static_cast<size_t>(current_)].swing = std::clamp(s, 0.0f, 0.9f);
}

void Sequencer::setSidechain(bool on, float amount, float releaseMs, float attackMs) {
    sidechainOn_ = on;
    scAmount_ = std::clamp(amount, 0.0f, 1.0f);
    scReleaseMs_ = std::max(releaseMs, 1.0f);
    scAttackMs_ = attackMs < 0.0f ? 0.0f : (attackMs > 500.0f ? 500.0f : attackMs);
}

void Sequencer::setArp(bool on, int mode) {
    arpOn_ = on;
    arpMode_ = (mode < 0 || mode > 4) ? 0 : mode;
    arpCounter_ = 0;
    arpRng_ = 0x1234567u;
}

void Sequencer::setHumanize(float amount) {
    humanize_ = std::clamp(amount, 0.0f, 1.0f);
}

double Sequencer::samplesPerStep(int sampleRate, int step) const {
    // beats/sec = bpm/60; steps/sec = beats/sec * stepsPerBeat; samples/step = sampleRate / steps-sec.
    const double stepsPerSec = (bpm_ / 60.0) * static_cast<double>(stepsPerBeat_);
    const double base = static_cast<double>(sampleRate) / stepsPerSec;
    // Swing (per-pattern): even steps get (1 + swing), odd steps (1 - swing) — a pair sums to 2·base.
    const double sw = static_cast<double>(patterns_[static_cast<size_t>(current_)].swing);
    const double factor = (step % 2 == 0) ? (1.0 + sw) : (1.0 - sw);
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

float Sequencer::stepProbability(int channel, int step) const {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return 1.0f;
    }
    const Pattern& p = patterns_[static_cast<size_t>(current_)];
    const size_t idx =
        static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) + static_cast<size_t>(step);
    if (idx >= p.prob.size()) {
        return 1.0f; // patterns loaded before probability existed default to "always"
    }
    return static_cast<float>(p.prob[idx]) / 255.0f;
}

void Sequencer::setStepProbability(int channel, int step, float probability) {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return;
    }
    Pattern& p = patterns_[static_cast<size_t>(current_)];
    if (p.prob.size() != p.grid.size()) {
        p.prob.assign(p.grid.size(), 255);
    }
    const float v = probability < 0.0f ? 0.0f : (probability > 1.0f ? 1.0f : probability);
    p.prob[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
            static_cast<size_t>(step)] = static_cast<uint8_t>(v * 255.0f + 0.5f);
}

int Sequencer::stepRatchet(int channel, int step) const {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return 1;
    }
    const Pattern& p = patterns_[static_cast<size_t>(current_)];
    const size_t idx =
        static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) + static_cast<size_t>(step);
    if (idx >= p.ratchet.size() || p.ratchet[idx] < 1) {
        return 1; // patterns loaded before ratchets existed → single hit
    }
    return static_cast<int>(p.ratchet[idx]);
}

void Sequencer::setStepRatchet(int channel, int step, int count) {
    if (channel < 0 || channel >= numChannels() || step < 0 || step >= numSteps_) {
        return;
    }
    Pattern& p = patterns_[static_cast<size_t>(current_)];
    if (p.ratchet.size() != p.grid.size()) {
        p.ratchet.assign(p.grid.size(), 1);
    }
    const int cnt = count < 1 ? 1 : (count > 4 ? 4 : count);
    p.ratchet[static_cast<size_t>(channel) * static_cast<size_t>(numSteps_) +
               static_cast<size_t>(step)] = static_cast<uint8_t>(cnt);
}

void Sequencer::toggle(int channel, int step) {
    setStep(channel, step, !this->step(channel, step));
}

int Sequencer::euclidFill(int channel, int pulses) {
    if (channel < 0 || channel >= numChannels() || numSteps_ <= 0) {
        return 0;
    }
    const int n = numSteps_;
    int k = pulses < 0 ? 0 : (pulses > n ? n : pulses);
    int placed = 0;
    // Bresenham-style even distribution: step i is a hit when (i·k) mod n < k. This lands exactly k
    // hits spread as evenly as possible — the Euclidean rhythm.
    for (int i = 0; i < n; ++i) {
        const bool on = k > 0 && (i * k) % n < k;
        setStep(channel, i, on);
        if (on) {
            ++placed;
        }
    }
    return placed;
}

void Sequencer::rotateChannel(int channel, int offset) {
    if (channel < 0 || channel >= numChannels() || numSteps_ <= 0) {
        return;
    }
    const int n = numSteps_;
    int off = offset % n;
    if (off < 0) {
        off += n;
    }
    if (off == 0) {
        return;
    }
    Pattern& p = patterns_[static_cast<size_t>(current_)];
    const size_t base = static_cast<size_t>(channel) * static_cast<size_t>(n);
    // Rotate one channel's slice of a parallel row (grid / prob / ratchet), if it is allocated.
    auto rotateRow = [&](std::vector<uint8_t>& v) {
        if (v.size() != p.grid.size()) {
            return; // prob/ratchet may be unallocated (all-default) → nothing to move
        }
        std::vector<uint8_t> row(static_cast<size_t>(n));
        for (int s = 0; s < n; ++s) {
            row[static_cast<size_t>((s + off) % n)] = v[base + static_cast<size_t>(s)];
        }
        for (int s = 0; s < n; ++s) {
            v[base + static_cast<size_t>(s)] = row[static_cast<size_t>(s)];
        }
    };
    rotateRow(p.grid);
    rotateRow(p.prob);
    rotateRow(p.ratchet);
}

void Sequencer::clear() {
    Pattern& p = patterns_[static_cast<size_t>(current_)];
    std::fill(p.grid.begin(), p.grid.end(), static_cast<uint8_t>(0));
}

void Sequencer::triggerStep(int step) {
    // Drums: strike every channel switched on at this step.
    for (int c = 0; c < numChannels(); ++c) {
        if (this->step(c, step)) {
            // Per-step probability: roll a deterministic RNG and skip the hit when it fails.
            const float pr = stepProbability(c, step);
            if (pr < 1.0f) {
                probRng_ ^= probRng_ << 13;
                probRng_ ^= probRng_ >> 17;
                probRng_ ^= probRng_ << 5;
                const float roll = static_cast<float>(probRng_ & 0xFFFFFFu) / 16777216.0f;
                if (roll >= pr) {
                    continue; // this hit is skipped this time
                }
            }
            float vel = stepVelocity(c, step);
            if (humanize_ > 0.0f) {
                // Deterministic per-hit jitter: reduce velocity by up to 60% of the humanize amount.
                uint32_t h = humanizeCounter_ * 2654435761u + static_cast<uint32_t>(c) * 40503u;
                h ^= h >> 15;
                const float r = static_cast<float>(h & 0xFFFFu) / 65536.0f;
                vel *= 1.0f - humanize_ * r * 0.6f;
                ++humanizeCounter_;
            }
            // Choke group: silence every other channel sharing this channel's group before striking.
            const int grp = chanChoke_[static_cast<size_t>(c)];
            if (grp != 0) {
                for (int other = 0; other < numChannels(); ++other) {
                    if (other != c && chanChoke_[static_cast<size_t>(other)] == grp) {
                        channels_[static_cast<size_t>(other)].choke();
                    }
                }
            }

            // Flam: a quiet grace hit now, then the full hit a few ms later (scheduled like a
            // ratchet sub-hit). Off → a single full-velocity hit.
            const float flamMs = chanFlam_[static_cast<size_t>(c)];
            if (flamMs > 0.0f) {
                channels_[static_cast<size_t>(c)].trigger(vel * 0.5f); // grace
                const int flamSamples =
                    static_cast<int>(flamMs * 0.001f * static_cast<float>(sampleRate_));
                ratchets_.push_back(RatchetHit{c, vel, flamSamples > 0 ? flamSamples : 1});
            } else {
                channels_[static_cast<size_t>(c)].trigger(vel);
            }

            // Ratchet: schedule extra evenly-spaced retriggers within this step's slot.
            const int r = stepRatchet(c, step);
            if (r > 1) {
                const int stepSamples = static_cast<int>(samplesPerStep(sampleRate_, step));
                const int interval = stepSamples / r;
                for (int k = 1; k < r; ++k) {
                    ratchets_.push_back(RatchetHit{c, vel, interval * k});
                }
            }
        }
    }
    // Sidechain: a kick (channel 0) hit ducks the melodic bus. With no attack the gain snaps down
    // instantly (the classic hard pump); with an attack time it ramps down to the floor over that
    // window (a softer, rounded duck) — handled per-sample in the render loop.
    if (sidechainOn_ && this->step(sidechainSource_, step)) {
        scTarget_ = 1.0f - scAmount_;
        if (scAttackMs_ <= 0.0f) {
            scEnv_ = scTarget_;
            scAttacking_ = false;
        } else {
            scAttacking_ = true; // ramp scEnv_ down toward scTarget_ starting now
        }
    }
    const bool toSampler = useSampler_ && sampler_.loaded();
    const PianoRoll& roll = patterns_[static_cast<size_t>(current_)].roll;

    // Arpeggiator: instead of playing the roll notes directly, play one note per step from the set
    // of pitches held at this step, cycling through them per the mode.
    if (arpOn_) {
        std::vector<int> held;
        for (const Note& n : roll.notes()) {
            if (step >= n.startStep && step < n.startStep + n.lengthSteps) {
                // Replicate each held pitch across the octave range so the arp spans wider.
                for (int o = 0; o < arpOctaves_; ++o) {
                    held.push_back(n.pitch + 12 * o);
                }
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
            const int n = static_cast<int>(held.size());
            // Modes 0–3 walk the pitches low→high; as-played (4) keeps the notes' entry order.
            std::vector<int> ord = held;
            if (arpMode_ != 4) {
                std::sort(ord.begin(), ord.end());
            }
            int index = 0;
            if (arpMode_ == 1) { // down
                index = (n - 1) - (arpCounter_ % n);
            } else if (arpMode_ == 2 && n > 1) { // up-down
                const int period = 2 * n - 2;
                const int pos = arpCounter_ % period;
                index = pos < n ? pos : period - pos;
            } else if (arpMode_ == 3) { // random (deterministic per-transport RNG)
                arpRng_ ^= arpRng_ << 13;
                arpRng_ ^= arpRng_ >> 17;
                arpRng_ ^= arpRng_ << 5;
                index = static_cast<int>(arpRng_ % static_cast<uint32_t>(n));
            } else { // up (0) or as-played (4)
                index = arpCounter_ % n;
            }
            const int pitch = ord[static_cast<size_t>(index)] + transpose_;
            if (toSampler) {
                sampler_.noteOn(pitch, 0.9f);
            } else {
                synth_.noteOn(pitch, 0.9f);
            }
            arpCurrentPitch_ = pitch;
            ++arpCounter_;
            // Gate: for a staccato arp, schedule an early note-off partway through the step; at full
            // gate (1) the note simply rings until the next step releases it (legato, as before).
            if (arpGate_ < 1.0f) {
                const double sps = samplesPerStep(sampleRate_, step);
                arpGateFramesLeft_ = std::max(1, static_cast<int>(static_cast<double>(arpGate_) * sps));
            } else {
                arpGateFramesLeft_ = -1;
            }
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
                sampler_.noteOff(n.pitch + transpose_);
            } else {
                synth_.noteOff(n.pitch + transpose_);
            }
        }
    }
    // Per-note probability: skip a note this loop when its odds fail (deterministic per-transport).
    auto noteFires = [this](const Note& n) {
        if (n.probability >= 1.0f) {
            return true;
        }
        probRng_ ^= probRng_ << 13;
        probRng_ ^= probRng_ >> 17;
        probRng_ ^= probRng_ << 5;
        const float r = static_cast<float>(probRng_ & 0xFFFFFFu) / 16777216.0f;
        return r < n.probability;
    };
    for (const Note& n : roll.notes()) {
        if (n.startStep == step && noteFires(n)) {
            if (toSampler) {
                sampler_.noteOn(n.pitch + transpose_, n.velocity);
            } else {
                synth_.noteOn(n.pitch + transpose_, n.velocity, n.fineTune);
            }
        }
    }

    // Second (bass) lane → synth2 (offs before ons).
    const PianoRoll& roll2 = patterns_[static_cast<size_t>(current_)].roll2;
    for (const Note& n : roll2.notes()) {
        if ((n.startStep + n.lengthSteps) % numSteps_ == step) {
            synth2_.noteOff(n.pitch + transpose_);
        }
    }
    for (const Note& n : roll2.notes()) {
        if (n.startStep == step && noteFires(n)) {
            synth2_.noteOn(n.pitch + transpose_, n.velocity, n.fineTune);
        }
    }
}

void Sequencer::play() {
    playing_ = true;
    currentStep_ = 0;
    samplesIntoStep_ = 0.0;
    playlistPos_ = 0;
    arpCounter_ = 0;
    arpRng_ = 0x1234567u;
    arpGateFramesLeft_ = -1;
    arpCurrentPitch_ = -1;
    humanizeCounter_ = 0;
    metroLastStep_ = -1;
    metroEnv_ = 0.0f;
    probRng_ = 0x9E3779B9u; // reseed so probability is reproducible per play()
    ratchets_.clear();
    countingIn_ = countInBars_ > 0;
    countInStepsRemaining_ = countInBars_ * numSteps_;
    if (songMode_ && !playlist_.empty()) {
        // Start at the loop-region start when one is set, else the first playlist entry.
        const int plSize = static_cast<int>(playlist_.size());
        const bool region = songLoopEnd_ > songLoopStart_ && songLoopStart_ < plSize;
        playlistPos_ = region ? songLoopStart_ : 0;
        selectPattern(playlist_[static_cast<size_t>(playlistPos_)]);
    }
    if (!countingIn_) {
        triggerStep(0); // when counting in, the pattern's first step fires after the count-in
    }
}

void Sequencer::stop() {
    playing_ = false;
    arpGateFramesLeft_ = -1;
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
    sampleRate_ = sampleRate; // used by triggerStep to schedule ratchet sub-hits
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

        // Ratchet sub-hits: fire any due now, and never render past the next pending one so it stays
        // sample-accurate.
        for (size_t ri = 0; ri < ratchets_.size();) {
            if (ratchets_[ri].framesUntil <= 0) {
                channels_[static_cast<size_t>(ratchets_[ri].channel)].trigger(ratchets_[ri].velocity);
                ratchets_.erase(ratchets_.begin() + static_cast<long>(ri));
            } else {
                chunk = std::min(chunk, ratchets_[ri].framesUntil);
                ++ri;
            }
        }

        // Arp gate: release the current arp note when its gate expires (staccato), and don't render
        // past that point so the release lands sample-accurately.
        if (arpGateFramesLeft_ == 0) {
            if (arpCurrentPitch_ >= 0) {
                synth_.noteOff(arpCurrentPitch_);
                sampler_.noteOff(arpCurrentPitch_);
                arpCurrentPitch_ = -1;
            }
            arpGateFramesLeft_ = -1;
        }
        if (arpGateFramesLeft_ > 0) {
            chunk = std::min(chunk, arpGateFramesLeft_);
        }

        // Metronome / count-in: fire an accented click when a new beat step begins (downbeat is
        // brighter). Count-in forces clicks regardless of the metronome toggle.
        const bool click = metronome_ || countingIn_;
        if (playing_ && click && currentStep_ != metroLastStep_ &&
            (currentStep_ % stepsPerBeat_) == 0) {
            metroLastStep_ = currentStep_;
            metroEnv_ = 1.0f;
            metroFreq_ = (currentStep_ == 0) ? 1600.0f : 1000.0f;
            metroPhase_ = 0.0;
        }
        if (click && metroEnv_ > 0.0f) {
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

        // Count-in: the click above is the only sound. Advance the count-in transport (no pattern
        // steps trigger) and, when the last count-in step passes, start the pattern at step 0.
        if (countingIn_) {
            samplesIntoStep_ += static_cast<double>(chunk);
            const double sps = samplesPerStep(sampleRate, currentStep_);
            if (samplesIntoStep_ + 0.5 >= sps) {
                samplesIntoStep_ -= sps;
                currentStep_ = (currentStep_ + 1) % numSteps_;
                if (--countInStepsRemaining_ <= 0) {
                    countingIn_ = false;
                    currentStep_ = 0;
                    samplesIntoStep_ = 0.0;
                    metroLastStep_ = -1;
                    triggerStep(0);
                }
            }
            done += chunk;
            continue; // no pattern/instrument rendering during the count-in
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
        // Equal-power pan per melodic bus (pan 0 → both gains 0.707, matching the old center mix).
        constexpr float kQuarterPi = 0.78539816f;
        const float leadTheta = (leadPan_ + 1.0f) * kQuarterPi; // 0..pi/2
        const float bassTheta = (bassPan_ + 1.0f) * kQuarterPi;
        const float leadL = std::cos(leadTheta), leadR = std::sin(leadTheta);
        const float bassL = std::cos(bassTheta), bassR = std::sin(bassTheta);
        const float scStep = 1.0f / (scReleaseMs_ * 0.001f * static_cast<float>(sampleRate));
        const float scAtkStep =
            scAttackMs_ > 0.0f ? 1.0f / (scAttackMs_ * 0.001f * static_cast<float>(sampleRate)) : 1.0f;
        for (int i = 0; i < chunk; ++i) {
            const float duck = sidechainOn_ ? scEnv_ : 1.0f;
            const float leadS = synthScratch_[static_cast<size_t>(i)] * synthGain_ * duck;
            const float bassS = bassScratch_[static_cast<size_t>(i)] * bassGain_ * duck;
            lead[2 * (done + i)] += leadS * leadL;
            lead[2 * (done + i) + 1] += leadS * leadR;
            bass[2 * (done + i)] += bassS * bassL;
            bass[2 * (done + i) + 1] += bassS * bassR;
            if (sidechainOn_) {
                if (scAttacking_) {
                    // Attack phase: ramp the gain down toward the floor, then hand off to recovery.
                    scEnv_ -= scAtkStep;
                    if (scEnv_ <= scTarget_) {
                        scEnv_ = scTarget_;
                        scAttacking_ = false;
                    }
                } else if (scEnv_ < 1.0f) {
                    scEnv_ = std::min(1.0f, scEnv_ + scStep); // release recovery back up to open
                }
            }
        }

        // Advance the pending ratchets that existed during this chunk by the frames just rendered.
        // (New hits pushed by triggerStep below are timed from the upcoming boundary, so exclude
        // them here.)
        const size_t ratchetsThisChunk = ratchets_.size();
        for (size_t i = 0; i < ratchetsThisChunk; ++i) {
            ratchets_[i].framesUntil -= chunk;
        }
        // Count down the arp gate by the frames just rendered so its note-off lands on time.
        if (arpGateFramesLeft_ > 0) {
            arpGateFramesLeft_ -= chunk;
        }

        if (playing_) {
            samplesIntoStep_ += static_cast<double>(chunk);
            const double sps = samplesPerStep(sampleRate, currentStep_);
            if (samplesIntoStep_ + 0.5 >= sps) {
                samplesIntoStep_ -= sps;
                currentStep_ = (currentStep_ + 1) % numSteps_;
                // At the top of each bar, in song mode, advance to the next playlist pattern.
                if (currentStep_ == 0 && songMode_ && !playlist_.empty()) {
                    // Honour the loop region [start, end) when set; otherwise the whole playlist.
                    const int plSize = static_cast<int>(playlist_.size());
                    const bool region =
                        songLoopEnd_ > songLoopStart_ && songLoopStart_ < plSize;
                    const int loopStart = region ? songLoopStart_ : 0;
                    const int loopEnd = region ? std::min(songLoopEnd_, plSize) : plSize;
                    const int next = playlistPos_ + 1;
                    if (next >= loopEnd) {
                        if (songLoop_) {
                            playlistPos_ = loopStart;
                            selectPattern(playlist_[static_cast<size_t>(loopStart)]);
                        } else {
                            // Play-once: stop cleanly at the end of the arrangement/region.
                            playing_ = false;
                            synth_.allNotesOff();
                            synth2_.allNotesOff();
                            sampler_.allNotesOff();
                        }
                    } else {
                        playlistPos_ = next;
                        selectPattern(playlist_[static_cast<size_t>(next)]);
                    }
                }
                if (playing_) {
                    triggerStep(currentStep_);
                }
            }
        }

        done += chunk;
    }
}

} // namespace maz::audio
