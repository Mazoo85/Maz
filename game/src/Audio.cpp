#include "zomboid/audio/Audio.hpp"

#include <algorithm>
#include <cmath>

namespace zb::audio {

namespace {

constexpr float kMaster = 0.45f; // master gain in the reference
constexpr float kMusic = 0.18f;  // music bus gain

float waveform(Wave type, double frac) {
    switch (type) {
    case Wave::Square: return frac < 0.5 ? 1.0f : -1.0f;
    case Wave::Triangle: return static_cast<float>(2.0 * std::fabs(2.0 * frac - 1.0) - 1.0);
    case Wave::Saw: return static_cast<float>(2.0 * frac - 1.0);
    case Wave::Sine: return static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * frac));
    }
    return 0.0f;
}

// Exponential attack (to 5ms) then exponential decay to ~silence, matching the
// WebAudio exponentialRampToValueAtTime envelope used in the reference.
float envelope(float t, float dur, float peak) {
    const float floorG = 0.0001f;
    const float a = std::min(0.005f, dur * 0.5f);
    if (t < a) return floorG * std::pow(peak / floorG, t / a);
    const float rem = std::max(1e-4f, dur - a);
    return peak * std::pow(floorG / peak, (t - a) / rem);
}

} // namespace

void Clip::addBlip(float startSec, float freq, float dur, Wave type, float gain, float slideTo) {
    const int s0 = static_cast<int>(std::lround(startSec * static_cast<float>(m_sr)));
    const int n = static_cast<int>(std::lround(dur * static_cast<float>(m_sr)));
    if (n <= 0 || s0 < 0) return;
    ensure(static_cast<size_t>(s0 + n));
    double phase = 0.0;
    for (int i = 0; i < n; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(m_sr);
        const float f =
            slideTo > 0.0f ? freq * std::pow(slideTo / freq, t / dur) : freq;
        phase += static_cast<double>(f) / static_cast<double>(m_sr);
        const double frac = phase - std::floor(phase);
        m_samples[static_cast<size_t>(s0 + i)] += waveform(type, frac) * envelope(t, dur, gain);
    }
}

void Clip::addNoise(float startSec, float dur, float gain, float hpHz, Rng& rng) {
    const int s0 = static_cast<int>(std::lround(startSec * static_cast<float>(m_sr)));
    const int n = static_cast<int>(std::lround(dur * static_cast<float>(m_sr)));
    if (n <= 0 || s0 < 0) return;
    ensure(static_cast<size_t>(s0 + n));
    // One-pole high-pass.
    const float dt = 1.0f / static_cast<float>(m_sr);
    const float rc = 1.0f / (2.0f * 3.14159265358979323846f * hpHz);
    const float a = rc / (rc + dt);
    float prevX = 0.0f, prevY = 0.0f;
    for (int i = 0; i < n; i++) {
        const float fade = 1.0f - static_cast<float>(i) / static_cast<float>(n);
        const float x = (rng.nextFloat() * 2.0f - 1.0f) * fade;
        const float y = a * (prevY + x - prevX);
        prevX = x;
        prevY = y;
        m_samples[static_cast<size_t>(s0 + i)] += y * gain;
    }
}

bool sfxFromName(const std::string& name, Sfx& out) {
    static const struct {
        const char* n;
        Sfx s;
    } kMap[] = {{"hit", Sfx::Hit},     {"swing", Sfx::Swing},   {"gun", Sfx::Gun},
                {"shotgun", Sfx::Shotgun}, {"pickup", Sfx::Pickup}, {"open", Sfx::Open},
                {"hurt", Sfx::Hurt},   {"death", Sfx::Death},   {"eat", Sfx::Eat},
                {"drink", Sfx::Drink}, {"select", Sfx::Select}, {"start", Sfx::Start},
                {"zgroan", Sfx::Zgroan}, {"sega", Sfx::Sega}};
    for (const auto& e : kMap)
        if (name == e.n) {
            out = e.s;
            return true;
        }
    return false;
}

// Render one SFX at time offset `t0` into `c` (master gain baked in).
static void emitSfx(Clip& c, Sfx sfx, float t0, Rng& rng) {
    const float m = kMaster;
    switch (sfx) {
    case Sfx::Hit:
        c.addNoise(t0, 0.12f, 0.4f * m, 400.0f, rng);
        c.addBlip(t0, 140, 0.1f, Wave::Square, 0.25f * m, 60);
        break;
    case Sfx::Swing:
        c.addBlip(t0, 520, 0.08f, Wave::Triangle, 0.18f * m, 240);
        break;
    case Sfx::Gun:
        c.addNoise(t0, 0.18f, 0.6f * m, 200.0f, rng);
        c.addBlip(t0, 90, 0.14f, Wave::Saw, 0.4f * m, 40);
        break;
    case Sfx::Shotgun:
        c.addNoise(t0, 0.32f, 0.7f * m, 120.0f, rng);
        c.addBlip(t0, 70, 0.2f, Wave::Saw, 0.5f * m, 30);
        break;
    case Sfx::Pickup:
        c.addBlip(t0, 660, 0.06f, Wave::Square, 0.25f * m);
        c.addBlip(t0 + 0.06f, 990, 0.08f, Wave::Square, 0.25f * m);
        break;
    case Sfx::Open:
        c.addBlip(t0, 330, 0.1f, Wave::Square, 0.2f * m, 520);
        break;
    case Sfx::Hurt:
        c.addBlip(t0, 220, 0.18f, Wave::Saw, 0.35f * m, 110);
        c.addNoise(t0, 0.1f, 0.3f * m, 300.0f, rng);
        break;
    case Sfx::Death:
        c.addBlip(t0, 330, 0.5f, Wave::Saw, 0.4f * m, 60);
        c.addNoise(t0, 0.4f, 0.3f * m, 200.0f, rng);
        break;
    case Sfx::Eat:
        c.addBlip(t0, 300, 0.12f, Wave::Sine, 0.2f * m, 360);
        break;
    case Sfx::Drink:
        c.addBlip(t0, 420, 0.12f, Wave::Sine, 0.2f * m, 300);
        break;
    case Sfx::Select:
        c.addBlip(t0, 740, 0.06f, Wave::Square, 0.25f * m);
        break;
    case Sfx::Start: {
        const float notes[] = {523, 659, 784, 1046};
        for (int i = 0; i < 4; i++)
            c.addBlip(t0 + static_cast<float>(i) * 0.09f, notes[i], 0.12f, Wave::Square, 0.3f * m);
        break;
    }
    case Sfx::Zgroan:
        c.addBlip(t0, 120.0f + rng.nextFloat() * 40.0f, 0.4f, Wave::Saw, 0.12f * m, 70);
        break;
    case Sfx::Sega: {
        const float a[] = {262, 330, 392};
        for (float f : a) c.addBlip(t0, f, 0.9f, Wave::Saw, 0.18f * m, f * 1.5f);
        const float b[] = {392, 494, 587};
        for (float f : b) c.addBlip(t0 + 0.45f, f, 0.7f, Wave::Square, 0.16f * m);
        break;
    }
    }
}

Clip renderSfx(Sfx sfx, Rng& rng) {
    Clip c;
    emitSfx(c, sfx, 0.0f, rng);
    return c;
}

// Render `steps` steps of the looping city bass + lead into `c` starting at t0.
static void emitMusic(Clip& c, int steps, float t0) {
    static const float BASS[] = {110, 110, 165, 110, 98, 98, 147, 165};
    static const float LEAD[] = {440, 0, 523, 587, 0, 494, 440, 0};
    for (int s = 0; s < steps; s++) {
        const float t = t0 + static_cast<float>(s) * 0.18f;
        const float b = BASS[static_cast<size_t>(s) % 8];
        const float l = LEAD[static_cast<size_t>(s) % 8];
        if (b > 0.0f) c.addBlip(t, b, 0.22f, Wave::Triangle, 0.5f * kMusic);
        if (l > 0.0f) c.addBlip(t, l, 0.16f, Wave::Square, 0.22f * kMusic);
    }
}

Clip renderMusic(int steps, Rng& rng) {
    (void)rng;
    Clip c;
    emitMusic(c, steps, 0.0f);
    return c;
}

Clip renderDemo(Rng& rng) {
    Clip c;
    emitMusic(c, 40, 0.0f); // ~7.2s music bed
    const Sfx order[] = {Sfx::Start, Sfx::Select, Sfx::Open,   Sfx::Pickup, Sfx::Eat,
                         Sfx::Drink, Sfx::Swing,  Sfx::Hit,    Sfx::Gun,    Sfx::Shotgun,
                         Sfx::Zgroan, Sfx::Hurt,  Sfx::Death};
    float t = 0.4f;
    for (Sfx s : order) {
        emitSfx(c, s, t, rng);
        t += 0.55f;
    }
    return c;
}

std::vector<uint8_t> encodeWav(const Clip& clip) {
    const auto& s = clip.samples();
    const uint32_t sr = static_cast<uint32_t>(clip.sampleRate());
    const uint32_t dataBytes = static_cast<uint32_t>(s.size() * 2);
    std::vector<uint8_t> out;
    out.reserve(44 + dataBytes);

    auto u32 = [&](uint32_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    auto u16 = [&](uint16_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto tag = [&](const char* t4) {
        for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(t4[i]));
    };

    tag("RIFF");
    u32(36 + dataBytes);
    tag("WAVE");
    tag("fmt ");
    u32(16);        // fmt chunk size
    u16(1);         // PCM
    u16(1);         // mono
    u32(sr);        // sample rate
    u32(sr * 2);    // byte rate (mono, 16-bit)
    u16(2);         // block align
    u16(16);        // bits per sample
    tag("data");
    u32(dataBytes);
    for (float v : s) {
        const float clamped = std::max(-1.0f, std::min(1.0f, v));
        const int16_t iv = static_cast<int16_t>(std::lround(clamped * 32767.0f));
        u16(static_cast<uint16_t>(iv));
    }
    return out;
}

} // namespace zb::audio
