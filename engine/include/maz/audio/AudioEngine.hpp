#pragma once

#include "maz/audio/Oscillator.hpp"
#include "maz/audio/Sequencer.hpp"

#include <cstdint>
#include <vector>

// Forward-declared so this public header stays free of the SDL headers.
struct SDL_AudioStream;

namespace maz::audio {

struct AudioConfig {
    int sampleRate = 48000;
    int channels = 2; // interleaved output channels
};

// The audio engine: it owns the output device (real-time playback) OR renders offline to a buffer,
// and mixes the active voices into an interleaved float stream.
//
// There is a single Oscillator voice for now. The render() loop plus the framesRendered() sample
// clock are deliberately the seam a future FL-style step sequencer plugs into: a sequencer will
// schedule note-on/note-off events at absolute sample positions and this engine will service them
// from the same clock.
class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Open a real-time output device. Returns false on failure. Under SDL_AUDIODRIVER=dummy this
    // still succeeds (a silent device), so headless/CI paths that ask for real-time behave.
    bool initRealtime(const AudioConfig& cfg = {});

    // Offline engine (no device, never touches hardware). Pair with renderOffline().
    void initOffline(const AudioConfig& cfg = {});

    void shutdown();

    const AudioConfig& config() const { return cfg_; }
    Oscillator& voice() { return voice_; }

    // The built-in step sequencer, mixed into the output alongside the oscillator voice.
    Sequencer& sequencer() { return sequencer_; }

    // Convenience passthrough to the single voice.
    void noteOn(float freqHz) { voice_.noteOn(freqHz); }
    void noteOff() { voice_.noteOff(); }

    // Total per-channel frames rendered since init — the master sample clock.
    uint64_t framesRendered() const { return framesRendered_; }

    // Render `frames` interleaved samples (frames * channels floats) into out, mixing all active
    // voices and advancing the sample clock. Called by both the device callback and renderOffline.
    void render(float* out, int frames);

    // Render `seconds` of audio offline into a freshly allocated interleaved buffer
    // (frames * channels floats). Handy for WAV export and deterministic tests.
    std::vector<float> renderOffline(double seconds);

private:
    AudioConfig cfg_{};
    Oscillator voice_{};
    Sequencer sequencer_{};
    SDL_AudioStream* stream_ = nullptr; // non-null only in real-time mode
    uint64_t framesRendered_ = 0;
    std::vector<float> scratch_; // reused mono render buffer for mixing
};

} // namespace maz::audio
