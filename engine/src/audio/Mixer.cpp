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
    filter_.setEnabled(false);
    dist_.setEnabled(false);
    tape_.setEnabled(false);
    ringmod_.setEnabled(false);
    freqshift_.setEnabled(false);
    crush_.setEnabled(false);
    gate_.setEnabled(false);
    hp_.setEnabled(false);
    comp_.setEnabled(false);
    mbcomp_.setEnabled(false);
    transient_.setEnabled(false);
    chorus_.setEnabled(false);
    vibrato_.setEnabled(false);
    rotary_.setEnabled(false);
    flanger_.setEnabled(false);
    phaser_.setEnabled(false);
    delay_.setEnabled(false);
    reverb_.setEnabled(false);
    widener_.setEnabled(false);
    imager_.setEnabled(false);
    mbsat_.setEnabled(false);
    stereoEnhancer_.setEnabled(false);
    utility_.setEnabled(false);
    clipper_.setEnabled(false);
    limiter_.setEnabled(false);
    deEsser_.setEnabled(false);
    autopan_.setEnabled(false);
    autowah_.setEnabled(false);
    comb_.setEnabled(false);
    tremolo_.setEnabled(false);
    stepgate_.setEnabled(false);
    stereoDelay_.setEnabled(false);
    formant_.setEnabled(false);
    monobass_.setEnabled(false);
    subbass_.setEnabled(false);
    plugin_.setEnabled(false);
    clap_.setEnabled(false);
    vst3_.setEnabled(false);
    // Signal order: gate → high-pass → EQ → tilt → exciter → tone → resonant filter → drive → tape → ring-mod →
    // crush → dynamics (compressor → multiband compressor → transient shaper) → modulation (chorus → vibrato → flanger → phaser →
    // auto-wah → formant → comb → tremolo) → time fx (delay → stereo-delay → reverb) → width →
    // mono-bass → sub-bass → auto-pan → utility → clipper → brickwall limiter → plugins.
    chain_ = {&gate_,     &hp_,       &peq_,      &tilt_,      &exciter_,     &eq_,       &filter_,   &dist_,
              &tape_,     &ringmod_,  &pitchshift_, &freqshift_, &crush_,   &comp_,        &mbcomp_,    &transient_, &deEsser_, &chorus_,
              &vibrato_,  &rotary_,   &flanger_,  &phaser_,   &autowah_,  &formant_,   &comb_,        &tremolo_,  &stepgate_, &delay_,
              &stereoDelay_, &reverb_, &widener_, &imager_, &mbsat_, &stereoEnhancer_, &monobass_, &subbass_, &autopan_, &utility_,
              &clipper_,  &limiter_,  &plugin_,   &clap_,     &vst3_};

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
    auto runSend = [&](Effect& ret, float send, const std::vector<float>& aux) {
        const bool hasAux = static_cast<int>(aux.size()) >= n;
        if (send <= 0.0f && !hasAux) {
            return;
        }
        sendScratch_.assign(static_cast<size_t>(n), 0.0f);
        for (int i = 0; i < n; ++i) {
            sendScratch_[static_cast<size_t>(i)] =
                stereo[i] * send + (hasAux ? aux[static_cast<size_t>(i)] : 0.0f);
        }
        ret.process(sendScratch_.data(), frames, sampleRate);
        for (int i = 0; i < n; ++i) {
            stereo[i] += sendScratch_[static_cast<size_t>(i)];
        }
    };
    runSend(reverbReturn_, reverbSend_, reverbAux_);
    runSend(delayReturn_, delaySend_, delayAux_);
    // The per-bus aux feeds are per-block; clear them so the next block starts fresh.
    reverbAux_.clear();
    delayAux_.clear();

    // Master balance (stereo pan): attenuate the channel opposite the pan direction (transparent at
    // centre), then apply the master gain and the guaranteed-ceiling soft limiter.
    const float balL = masterBalance_ > 0.0f ? 1.0f - masterBalance_ : 1.0f;
    const float balR = masterBalance_ < 0.0f ? 1.0f + masterBalance_ : 1.0f;
    for (int i = 0; i < frames; ++i) {
        stereo[2 * i] = limit(stereo[2 * i] * masterGain_ * balL, limiterCeiling_);
        stereo[2 * i + 1] = limit(stereo[2 * i + 1] * masterGain_ * balR, limiterCeiling_);
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
