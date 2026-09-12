#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::audio {

// 2D positional audio — Godot's AudioStreamPlayer2D. Given a listener (position + a "right" axis) and a
// sound source position, compute the source's per-channel (left/right) gain: a distance ATTENUATION
// (the source fades with range) times a constant-power PAN (a source off to one side is louder in that
// ear). Pure math — no device, no mixer — so it unit-tests headlessly and can drive any backend that
// accepts a left/right gain per voice (the Maz mixer does, via SoundDesc::leftGain/rightGain).

enum class Attenuation {
    Linear,          // full at refDistance, ramps linearly to 0 at maxDistance
    InverseDistance, // refDistance / d (natural 1/r falloff), hard-cut past maxDistance
};

struct Listener2D {
    math::vec2 pos{0.0f, 0.0f};
    math::vec2 right{1.0f, 0.0f}; // unit vector toward the listener's right, for panning
};

struct StereoGain {
    float left = 0.0f;
    float right = 0.0f;
};

// Distance-only gain (ignores panning): 1 at/inside refDistance, 0 at/beyond maxDistance.
inline float attenuation(float distance, float refDistance, float maxDistance, Attenuation mode) {
    if (distance >= maxDistance) {
        return 0.0f;
    }
    if (distance <= refDistance) {
        return 1.0f;
    }
    if (mode == Attenuation::Linear) {
        const float span = maxDistance - refDistance;
        return span > 1e-6f ? (maxDistance - distance) / span : 0.0f;
    }
    return refDistance / distance; // InverseDistance
}

// Full stereo gain of a source heard by the listener: baseVolume * attenuation, split across the two
// channels by a constant-power pan (left^2 + right^2 == (baseVolume*attenuation)^2).
inline StereoGain spatialize(const Listener2D& listener, math::vec2 source, float baseVolume,
                             float refDistance, float maxDistance,
                             Attenuation mode = Attenuation::InverseDistance) {
    const math::vec2 rel = source - listener.pos;
    const float d = std::sqrt(glm::dot(rel, rel));
    const float att = attenuation(d, refDistance, maxDistance, mode);

    float pan = 0.0f; // -1 = hard left, +1 = hard right
    if (d > 1e-5f) {
        pan = glm::dot(rel, listener.right) / d;
        pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);
    }
    // Constant-power pan law: theta sweeps 0..pi/2 as pan goes -1..+1.
    const float theta = (pan + 1.0f) * 0.25f * 3.14159265358979f;
    const float g = baseVolume * att;
    return StereoGain{g * std::cos(theta), g * std::sin(theta)};
}

} // namespace maz::audio
