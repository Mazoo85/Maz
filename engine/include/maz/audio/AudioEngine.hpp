#pragma once

#include "maz/audio/Automation.hpp"
#include "maz/audio/Mixer.hpp"
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

    // The master bus mixer (effect chain + master gain) applied to the final stereo output.
    Mixer& mixer() { return mixer_; }

    // Parameter automation (LFO lanes) evaluated each block against the transport clock.
    Automation& automation() { return automation_; }

    // Convenience passthrough to the single voice.
    void noteOn(float freqHz) { voice_.noteOn(freqHz); }
    void noteOff() { voice_.noteOff(); }

    // Total per-channel frames rendered since init — the master sample clock.
    uint64_t framesRendered() const { return framesRendered_; }

    // --- Recording -----------------------------------------------------------
    // Capture the rendered master output into an internal buffer (a "record" of the session), which
    // can then be written to a WAV. Works in both real-time and offline modes.
    void armRecording();
    void stopRecording();
    bool recording() const { return recording_; }
    const std::vector<float>& recordedAudio() const { return recordBuffer_; }
    bool saveRecording(const std::string& path, std::string* err = nullptr) const;

    // Master output metering: the peak and RMS level of the most recently rendered block (0..~1),
    // for a level meter in the UI. Updated every render() call.
    float masterPeak() const { return masterPeak_; }
    float masterRms() const { return masterRms_; }

    // Open a real-time audio *input* (microphone/line) capture device and record it to the same
    // buffer. Returns false on failure; under SDL_AUDIODRIVER=dummy it opens a silent input.
    bool startInputCapture(const AudioConfig& cfg = {});
    void stopInputCapture();

    // Render `frames` interleaved samples (frames * channels floats) into out, mixing all active
    // voices and advancing the sample clock. Called by both the device callback and renderOffline.
    void render(float* out, int frames);

    // Render `seconds` of audio offline into a freshly allocated interleaved buffer
    // (frames * channels floats). Handy for WAV export and deterministic tests.
    std::vector<float> renderOffline(double seconds);

    // The three mixer buses rendered separately (each interleaved stereo), for FL-style stem export:
    // each bus runs through its own insert strip (per-track EQ/drive/comp/gain/pan) but NOT the master
    // chain, so the drum/lead/bass stems can be mixed or mastered downstream independently. Advances
    // the transport exactly like renderOffline (call play() first). Empty when not stereo.
    struct Stems {
        std::vector<float> drums, lead, bass;
    };
    Stems renderStemsOffline(double seconds);

private:
    AudioConfig cfg_{};
    Oscillator voice_{};
    Sequencer sequencer_{};
    Mixer mixer_{};
    Automation automation_{};
    SDL_AudioStream* stream_ = nullptr;        // non-null only in real-time mode
    SDL_AudioStream* captureStream_ = nullptr; // non-null while capturing input
    uint64_t framesRendered_ = 0;
    std::vector<float> scratch_; // reused mono render buffer for mixing
    std::vector<float> stemDrums_, stemLead_, stemBass_; // per-bus stems for the mixer-track path
    std::vector<float> reverbAuxBuf_, delayAuxBuf_;      // per-bus aux-send feeds for the returns
    float masterPeak_ = 0.0f; // peak level of the last rendered block (UI meter)
    float masterRms_ = 0.0f;  // RMS level of the last rendered block (UI meter)
    std::vector<float> recordBuffer_;
    bool recording_ = false;
};

} // namespace maz::audio
