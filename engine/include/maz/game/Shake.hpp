#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::game {

// Trauma-based camera shake (after Squirrel Eiserloh's "Juicing Your Cameras"). Impactful moments
// add "trauma" in [0,1]; the shake amount is trauma^2 so small hits barely wobble while big ones
// jolt, and trauma decays linearly so the shake settles on its own. The offset/rotation are pure
// functions of (trauma, time) built from layered sines, so no RNG state is needed and the motion is
// smooth and deterministic. Feed the offset into the camera position and the yaw/pitch into the
// look angles each frame.
class Shake {
public:
    // Add trauma from an event (e.g. 0.5 for a pickup, 1.0 for a big hit). Clamped to [0,1].
    void addTrauma(float amount) {
        m_trauma += amount;
        m_trauma = m_trauma < 0.0f ? 0.0f : (m_trauma > 1.0f ? 1.0f : m_trauma);
    }
    // Decay trauma toward zero. Call once per frame with the frame delta.
    void update(float dt) {
        m_trauma -= m_decay * dt;
        if (m_trauma < 0.0f) m_trauma = 0.0f;
    }

    float trauma() const { return m_trauma; }
    float shake() const { return m_trauma * m_trauma; } // eased amount actually applied

    // World-space positional offset at `time`, scaled to at most `maxOffset` on each axis.
    math::vec3 offset(float time, float maxOffset = 0.35f) const {
        const float s = shake() * maxOffset;
        return math::vec3(s * noise(time, 0.0f, 31.0f), s * noise(time, 1.7f, 27.0f),
                          s * noise(time, 3.1f, 23.0f));
    }
    // Rotational shake in radians (yaw, pitch), at most `maxRad` each. Add to the look angles.
    math::vec3 angles(float time, float maxRad = 0.05f) const {
        const float s = shake() * maxRad;
        return math::vec3(s * noise(time, 5.2f, 19.0f), s * noise(time, 6.9f, 17.0f), 0.0f);
    }

    void setDecayRate(float perSecond) { m_decay = perSecond; }

private:
    // Smooth pseudo-noise in [-1,1] from two incommensurate sines (no RNG, deterministic in time).
    static float noise(float time, float phase, float freq) {
        return std::sin(time * freq + phase) * std::cos(time * (freq * 0.61f) + phase * 1.3f);
    }

    float m_trauma = 0.0f;
    float m_decay = 1.4f; // trauma units shed per second
};

} // namespace maz::game
