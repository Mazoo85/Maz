#include "maz/audio/Mixer.hpp"

#include <cmath>

namespace maz::audio {

namespace {
// Master limiter: transparent below the knee, soft-clips above it toward ±1 so the master never
// clips hard. knee = 0.8 → linear up to 0.8, asymptotes to ~1.0 beyond.
float limit(float x) {
    constexpr float knee = 0.8f;
    const float a = std::fabs(x);
    if (a <= knee) {
        return x;
    }
    const float over = a - knee;
    const float shaped = knee + (1.0f - knee) * std::tanh(over / (1.0f - knee));
    return x < 0.0f ? -shaped : shaped;
}
} // namespace

Mixer::Mixer() {
    // Effects off by default: a fresh mixer is transparent until something enables them.
    peq_.setEnabled(false);
    eq_.setEnabled(false);
    dist_.setEnabled(false);
    crush_.setEnabled(false);
    comp_.setEnabled(false);
    chorus_.setEnabled(false);
    phaser_.setEnabled(false);
    delay_.setEnabled(false);
    reverb_.setEnabled(false);
    plugin_.setEnabled(false);
    // Signal order: EQ → tone → drive → crush → dynamics → modulation → time effects → plugin.
    chain_ = {&peq_,  &eq_,    &dist_,  &crush_,   &comp_,
              &chorus_, &phaser_, &delay_, &reverb_, &plugin_};
}

void Mixer::process(float* stereo, int frames, int sampleRate) {
    if (frames <= 0 || sampleRate <= 0) {
        return;
    }
    for (Effect* fx : chain_) {
        fx->process(stereo, frames, sampleRate); // each no-ops when disabled
    }
    const int n = frames * 2;
    for (int i = 0; i < n; ++i) {
        stereo[i] = limit(stereo[i] * masterGain_);
    }
}

void Mixer::reset() {
    for (Effect* fx : chain_) {
        fx->reset();
    }
}

} // namespace maz::audio
