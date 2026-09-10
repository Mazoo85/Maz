// ZOMBOID: ANCHORAGE — headless chiptune synth (ported from js/audio.js).
//
// Renders the reference game's WebAudio SFX and looping music into PCM float
// buffers with no audio device, then out to WAV. Oscillator blips (square /
// triangle / saw / sine) with an attack-decay envelope and optional pitch
// slide, plus filtered noise bursts. Deterministic (seeded RNG) so output is
// reproducible and unit-testable. The engine's real-time audio module can drive
// the same note recipes later.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "zomboid/Rng.hpp"

namespace zb::audio {

enum class Wave { Square, Triangle, Saw, Sine };

constexpr int kSampleRate = 44100;

// A mono float sample buffer that voices are mixed into.
class Clip {
public:
    explicit Clip(int sampleRate = kSampleRate) : m_sr(sampleRate) {}

    int sampleRate() const { return m_sr; }
    const std::vector<float>& samples() const { return m_samples; }
    float durationSec() const { return static_cast<float>(m_samples.size()) / static_cast<float>(m_sr); }

    // A pitched voice: exponential attack (5ms) then exponential decay to end,
    // with an optional exponential pitch slide to slideTo over the duration.
    void addBlip(float startSec, float freq, float dur, Wave type, float gain, float slideTo = 0.0f);
    // A decaying white-noise burst through a one-pole high-pass (hpHz).
    void addNoise(float startSec, float dur, float gain, float hpHz, Rng& rng);

private:
    void ensure(size_t n) {
        if (m_samples.size() < n) m_samples.resize(n, 0.0f);
    }
    int m_sr;
    std::vector<float> m_samples;
};

// The SFX set (mirrors AUDIO.SFX in the reference).
enum class Sfx {
    Hit, Swing, Gun, Shotgun, Pickup, Open, Hurt, Death, Eat, Drink, Select, Start, Zgroan, Sega
};

// Look up an SFX name ("hit", "gun", ...); returns false if unknown.
bool sfxFromName(const std::string& name, Sfx& out);

// Render one SFX into a fresh Clip.
Clip renderSfx(Sfx sfx, Rng& rng);
// Render `steps` steps (~0.18s each) of the looping bass+lead city music.
Clip renderMusic(int steps, Rng& rng);
// A showcase: every SFX played in sequence over a few bars of the music bed.
Clip renderDemo(Rng& rng);

// Encode a Clip as a 16-bit mono PCM WAV file (bytes).
std::vector<uint8_t> encodeWav(const Clip& clip);

} // namespace zb::audio
