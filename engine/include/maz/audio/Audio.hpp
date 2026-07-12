#pragma once

namespace maz::audio {

enum class Wave { Sine, Square, Triangle, Noise };

// A procedurally-synthesized one-shot sound. No asset files needed.
struct SoundDesc {
    Wave wave = Wave::Square;
    float freq = 440.0f;    // starting frequency (Hz)
    float freqEnd = 0.0f;   // if > 0, glide to this frequency across the duration
    float duration = 0.15f; // seconds
    float volume = 0.30f;   // 0..1
    // Per-channel gain for stereo panning (1,1 = centred/full). Feed audio::spatialize() here to place
    // the sound in space; see maz/audio/Spatial2D.hpp.
    float leftGain = 1.0f;
    float rightGain = 1.0f;
};

// Real-time audio: opens an SDL audio device and mixes synthesized voices on the audio thread.
// Thread-safe: play()/setMusic()/setMasterVolume() may be called from the game thread. Degrades
// gracefully — if no device is available, everything becomes a no-op and the game still runs.
class Audio {
public:
    Audio() = default;
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init();
    void shutdown();

    void play(const SoundDesc& sound);
    void setMusic(bool on);        // toggles a looping arpeggio bed
    void setMasterVolume(float v); // 0..1

    bool active() const { return m_active; }

private:
    struct Impl;
    Impl* m_impl = nullptr; // pImpl keeps SDL + threading out of the public header
    bool m_active = false;
};

} // namespace maz::audio
