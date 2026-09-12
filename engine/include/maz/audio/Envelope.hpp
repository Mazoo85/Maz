#pragma once

namespace maz::audio {

// ADSR envelope — the amplitude contour every synth voice is shaped by. When a key goes down (note-on)
// the level ramps 0 → 1 over ATTACK, falls to the SUSTAIN level over DECAY, then holds there for as long
// as the key is held; when the key is released (note-off) it ramps from wherever it is down to 0 over
// RELEASE. Multiplying a raw oscillator by this level turns a flat buzz into a note with a shape — a
// plucky blip, a slow-swelling pad, a percussive stab. Times are in seconds, the sustain level in [0,1].
// It's a tiny gated state machine advanced by process(dt); pure scalar math, deterministic, so it
// unit-tests headless and a synth callback just multiplies each sample by process(1/sampleRate).
struct ADSR {
    float attack = 0.01f;  // seconds, 0 -> 1
    float decay = 0.10f;   // seconds, 1 -> sustain
    float sustain = 0.70f; // held level, [0,1]
    float release = 0.20f; // seconds, current level -> 0

    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Stage::Idle;
    float level = 0.0f;

    // Press the key: (re)start from the attack stage. Attack ramps from the CURRENT level, so a fast
    // re-trigger doesn't click back to zero.
    void noteOn() { stage = Stage::Attack; }

    // Release the key: fall to zero over `release` from wherever the level currently is.
    void noteOff() {
        if (stage != Stage::Idle) {
            releaseFrom_ = level;
            stage = Stage::Release;
        }
    }

    void reset() {
        stage = Stage::Idle;
        level = 0.0f;
        releaseFrom_ = 0.0f;
    }

    bool active() const { return stage != Stage::Idle; }

    // Advance the envelope by `dt` seconds and return the new level. Linear segments (a clean, classic
    // contour); zero-length segments snap instantly to the next stage.
    float process(float dt) {
        switch (stage) {
        case Stage::Idle:
            level = 0.0f;
            break;
        case Stage::Attack:
            level += (attack > 1e-9f ? dt / attack : 2.0f);
            if (level >= 1.0f) {
                level = 1.0f;
                stage = Stage::Decay;
            }
            break;
        case Stage::Decay: {
            const float rate = decay > 1e-9f ? (1.0f - sustain) / decay : 1e9f;
            level -= rate * dt;
            if (level <= sustain) {
                level = sustain;
                stage = Stage::Sustain;
            }
            break;
        }
        case Stage::Sustain:
            level = sustain;
            break;
        case Stage::Release: {
            const float rate = release > 1e-9f ? releaseFrom_ / release : 1e9f;
            level -= rate * dt;
            if (level <= 0.0f) {
                level = 0.0f;
                stage = Stage::Idle;
            }
            break;
        }
        }
        return level;
    }

private:
    float releaseFrom_ = 0.0f; // level captured at note-off, so release scales from there
};

} // namespace maz::audio
