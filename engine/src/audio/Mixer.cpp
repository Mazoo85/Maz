#include "maz/audio/Mixer.hpp"

#include <cmath>

namespace maz::audio {

namespace {
// Master limiter: transparent below the knee, soft-clips above it toward the ceiling so the master
// never clips hard. The knee sits at 80% of the ceiling; output asymptotes to ±ceiling.
float limit(float x, float ceiling) {
    const float knee = 0.8f * ceiling;
    const float a = std::fabs(x);
    if (a <= knee) {
        return x;
    }
    const float span = ceiling - knee;
    if (span <= 0.0f) {
        return x < 0.0f ? -ceiling : ceiling;
    }
    const float shaped = knee + span * std::tanh((a - knee) / span);
    return x < 0.0f ? -shaped : shaped;
}
} // namespace

Mixer::Mixer() {
    // Effects off by default: a fresh mixer is transparent until something enables them.
    peq_.setEnabled(false);
    tilt_.setEnabled(false);
    exciter_.setEnabled(false);
    eq_.setEnabled(false);
    dist_.setEnabled(false);
    tape_.setEnabled(false);
    ringmod_.setEnabled(false);
    crush_.setEnabled(false);
    gate_.setEnabled(false);
    hp_.setEnabled(false);
    comp_.setEnabled(false);
    transient_.setEnabled(false);
    chorus_.setEnabled(false);
    flanger_.setEnabled(false);
    phaser_.setEnabled(false);
    delay_.setEnabled(false);
    reverb_.setEnabled(false);
    widener_.setEnabled(false);
    autopan_.setEnabled(false);
    autowah_.setEnabled(false);
    monobass_.setEnabled(false);
    plugin_.setEnabled(false);
    clap_.setEnabled(false);
    vst3_.setEnabled(false);
    // Signal order: gate → high-pass → EQ → tilt → exciter → tone → drive → tape → ring-mod →
    // crush → dynamics (compressor → transient shaper) → modulation (chorus → flanger → phaser →
    // auto-wah) → time fx → width → mono-bass → auto-pan → plugins.
    chain_ = {&gate_,     &hp_,       &peq_,      &tilt_,     &exciter_, &eq_,       &dist_,
              &tape_,     &ringmod_,  &crush_,    &comp_,     &transient_, &chorus_, &flanger_,
              &phaser_,   &autowah_,  &delay_,    &reverb_,   &widener_, &monobass_, &autopan_,
              &plugin_,   &clap_,     &vst3_};

    // The return buses are always "enabled" and fully wet — the send level (0 by default) gates how
    // much signal reaches them, so a fresh mixer stays transparent.
    reverbReturn_.setEnabled(true);
    reverbReturn_.setMix(1.0f);
    delayReturn_.setEnabled(true);
    delayReturn_.setMix(1.0f);
}

bool Mixer::anyTrackActive() const {
    for (const MixerTrack& t : tracks_) {
        if (t.active()) {
            return true;
        }
    }
    return false;
}

void Mixer::process(float* stereo, int frames, int sampleRate) {
    if (frames <= 0 || sampleRate <= 0) {
        return;
    }
    for (Effect* fx : chain_) {
        fx->process(stereo, frames, sampleRate); // each no-ops when disabled
    }
    const int n = frames * 2;

    // Parallel send/return buses: tap a scaled copy of the post-insert signal into each return's
    // wet-only effect, then sum it back. Skipped entirely when the send level is 0.
    auto runSend = [&](Effect& ret, float send) {
        if (send <= 0.0f) {
            return;
        }
        sendScratch_.assign(static_cast<size_t>(n), 0.0f);
        for (int i = 0; i < n; ++i) {
            sendScratch_[static_cast<size_t>(i)] = stereo[i] * send;
        }
        ret.process(sendScratch_.data(), frames, sampleRate);
        for (int i = 0; i < n; ++i) {
            stereo[i] += sendScratch_[static_cast<size_t>(i)];
        }
    };
    runSend(reverbReturn_, reverbSend_);
    runSend(delayReturn_, delaySend_);

    for (int i = 0; i < n; ++i) {
        stereo[i] = limit(stereo[i] * masterGain_, limiterCeiling_);
    }
}

void Mixer::reset() {
    for (Effect* fx : chain_) {
        fx->reset();
    }
    reverbReturn_.reset();
    delayReturn_.reset();
}

} // namespace maz::audio
