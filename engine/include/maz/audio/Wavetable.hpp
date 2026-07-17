#pragma once

#include "maz/audio/Oscillator.hpp" // Waveform + waveSample

#include <array>
#include <cmath>
#include <cstddef>

namespace maz::audio {

// A morphing wavetable: a stack of single-cycle waveforms ("frames"). A position in [0, 1] scans
// through the stack, and playback bilinearly interpolates — linear across the sample within a frame
// and linear across adjacent frames — so sweeping the position morphs the timbre continuously. This
// is the heart of modern wavetable synths (Serum/Vital/Harmor-style). Pure data + math, no SDL, so
// it is unit-testable on its own.
class Wavetable {
public:
    static constexpr int kFrames = 4;      // number of single-cycle waveforms in the stack
    static constexpr int kSamples = 2048;  // samples per single-cycle frame

    // Build the default "analogue morph" table: frame 0 is a pure sine, and successive frames add
    // harmonics (triangle → saw → square) so position 0→1 sweeps from dark/pure to bright/buzzy.
    Wavetable() { setMorph(Waveform::Sine, Waveform::Triangle, Waveform::Saw, Waveform::Square); }

    // Overwrite the four frames from four base waveforms (each rendered as one band-unlimited cycle).
    void setMorph(Waveform a, Waveform b, Waveform c, Waveform d) {
        const Waveform w[kFrames] = {a, b, c, d};
        for (int f = 0; f < kFrames; ++f) {
            for (int i = 0; i < kSamples; ++i) {
                const double phase = static_cast<double>(i) / static_cast<double>(kSamples);
                data_[static_cast<size_t>(f)][static_cast<size_t>(i)] = waveSample(w[f], phase);
            }
        }
    }

    // Sample the table at morph position [0, 1] and cycle phase [0, 1). Bilinear interpolation.
    float sample(float position, double phase) const {
        // Locate the fractional sample position within a frame.
        const double sp = phase * static_cast<double>(kSamples);
        int i0 = static_cast<int>(sp);
        if (i0 >= kSamples) {
            i0 -= kSamples;
        }
        int i1 = i0 + 1;
        if (i1 >= kSamples) {
            i1 -= kSamples;
        }
        const float sfrac = static_cast<float>(sp - std::floor(sp));

        // Locate the fractional frame position within the stack.
        const float fp = (position < 0.0f ? 0.0f : (position > 1.0f ? 1.0f : position)) *
                         static_cast<float>(kFrames - 1);
        int f0 = static_cast<int>(fp);
        if (f0 >= kFrames - 1) {
            f0 = kFrames - 1;
        }
        int f1 = f0 + 1;
        if (f1 >= kFrames) {
            f1 = kFrames - 1;
        }
        const float ffrac = fp - static_cast<float>(f0);

        // Interpolate within each of the two frames, then across them.
        const auto lerp = [](float x, float y, float t) { return x + (y - x) * t; };
        const float a =
            lerp(data_[static_cast<size_t>(f0)][static_cast<size_t>(i0)],
                 data_[static_cast<size_t>(f0)][static_cast<size_t>(i1)], sfrac);
        const float b =
            lerp(data_[static_cast<size_t>(f1)][static_cast<size_t>(i0)],
                 data_[static_cast<size_t>(f1)][static_cast<size_t>(i1)], sfrac);
        return lerp(a, b, ffrac);
    }

private:
    std::array<std::array<float, kSamples>, kFrames> data_{};
};

} // namespace maz::audio
