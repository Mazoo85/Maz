#include "maz/audio/Mixer.hpp"

namespace maz::audio {

Mixer::Mixer() {
    // Effects off by default: a fresh mixer is transparent until something enables them.
    eq_.setEnabled(false);
    comp_.setEnabled(false);
    delay_.setEnabled(false);
    reverb_.setEnabled(false);
    chain_ = {&eq_, &comp_, &delay_, &reverb_};
}

void Mixer::process(float* stereo, int frames, int sampleRate) {
    if (frames <= 0 || sampleRate <= 0) {
        return;
    }
    for (Effect* fx : chain_) {
        fx->process(stereo, frames, sampleRate); // each no-ops when disabled
    }
    if (masterGain_ != 1.0f) {
        const int n = frames * 2;
        for (int i = 0; i < n; ++i) {
            stereo[i] *= masterGain_;
        }
    }
}

void Mixer::reset() {
    for (Effect* fx : chain_) {
        fx->reset();
    }
}

} // namespace maz::audio
