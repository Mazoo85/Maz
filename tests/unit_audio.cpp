// Unit tests for maz::audio — pure DSP, no audio device required. Verifies the oscillator produces
// a sane signal (correct length, bounded amplitude, non-zero energy, expected frequency) and that
// the offline engine render matches the requested duration.

#include "maz/audio/AudioEngine.hpp"
#include "maz/audio/Oscillator.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Count rising zero-crossings and convert to an estimated fundamental frequency.
double estimateHz(const std::vector<float>& mono, int sampleRate) {
    if (mono.size() < 2) {
        return 0.0;
    }
    int crossings = 0;
    float prev = mono[0];
    for (size_t i = 1; i < mono.size(); ++i) {
        if (prev <= 0.0f && mono[i] > 0.0f) {
            ++crossings;
        }
        prev = mono[i];
    }
    return static_cast<double>(crossings) * static_cast<double>(sampleRate) /
           static_cast<double>(mono.size());
}

} // namespace

int main() {
    const int sampleRate = 48000;
    const int frames = sampleRate; // one second

    // --- Oscillator: pure DSP ------------------------------------------------
    audio::Oscillator osc;
    osc.setWaveform(audio::Waveform::Sine);
    osc.setAmplitude(0.5f);
    osc.noteOn(440.0f);

    std::vector<float> mono(static_cast<size_t>(frames), 0.0f);
    osc.render(mono.data(), frames, sampleRate);

    check(mono.size() == static_cast<size_t>(frames), "render fills the requested sample count");

    float peak = 0.0f;
    double energy = 0.0;
    bool inRange = true;
    for (float s : mono) {
        peak = std::max(peak, std::fabs(s));
        energy += static_cast<double>(s) * static_cast<double>(s);
        if (s < -1.0f || s > 1.0f) {
            inRange = false;
        }
    }
    check(inRange, "all samples stay within [-1, 1]");
    check(energy > 0.0, "signal carries non-zero energy");
    check(peak > 0.4f && peak <= 0.5f + 1e-4f, "peak tracks the 0.5 amplitude");

    const double hz = estimateHz(mono, sampleRate);
    check(std::fabs(hz - 440.0) < 2.0, "estimated frequency is ~440 Hz");

    // noteOff() then a full ramp should decay to silence.
    osc.noteOff();
    std::vector<float> tail(static_cast<size_t>(frames), 0.0f);
    osc.render(tail.data(), frames, sampleRate);
    check(!osc.active(), "voice goes inactive after release");
    check(std::fabs(tail.back()) < 1e-6f, "signal decays to silence after noteOff");

    // Frequency should be settable and reflected in the output.
    audio::Oscillator osc2;
    osc2.noteOn(880.0f);
    std::vector<float> mono2(static_cast<size_t>(frames), 0.0f);
    osc2.render(mono2.data(), frames, sampleRate);
    check(std::fabs(estimateHz(mono2, sampleRate) - 880.0) < 3.0, "880 Hz note renders at ~880 Hz");

    // --- AudioEngine: offline render ----------------------------------------
    audio::AudioEngine engine;
    engine.initOffline();
    engine.noteOn(440.0f);
    const std::vector<float> stereo = engine.renderOffline(1.0);
    const int channels = engine.config().channels;
    check(stereo.size() == static_cast<size_t>(frames) * static_cast<size_t>(channels),
          "offline render length matches seconds * sampleRate * channels");
    check(engine.framesRendered() == static_cast<uint64_t>(frames),
          "sample clock advanced by exactly one second of frames");
    // Left and right channels should be identical (mono voice fanned out).
    check(stereo.size() >= 2 && std::fabs(stereo[0] - stereo[1]) < 1e-6f,
          "interleaved channels carry the same mono signal");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
