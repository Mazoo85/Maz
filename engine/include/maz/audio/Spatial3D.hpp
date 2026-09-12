#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>

namespace maz::audio {

// 3D spatial audio — Godot's AudioStreamPlayer3D. Where Spatial2D gives distance attenuation + a left/
// right pan on a plane, a 3D source heard by a 3D LISTENER needs three things: (1) distance
// ATTENUATION with a choice of falloff curve, (2) a stereo PAN derived from where the source sits
// relative to the listener's ORIENTATION (its forward/up basis), and (3) DOPPLER — the pitch shift when
// source and listener move relative to each other. All pure math — no device, no mixer — so it
// unit-tests headlessly and drives any backend that takes a per-voice left/right gain + pitch.

// Distance falloff curves (Godot AudioStreamPlayer3D attenuation models). `d` is clamped to
// [refDistance, maxDistance] first; gain is 1 at/inside refDistance.
enum class Attenuation3D {
    None,          // constant 1 (no falloff)
    Linear,        // 1 -> (1 - rolloff) linearly across [ref, max]
    Inverse,       // ref / (ref + rolloff*(d-ref))            (natural 1/r-ish)
    InverseSquare, // ref^2 / (ref^2 + rolloff*(d-ref)^2)      (steeper, ~1/r^2)
};

struct Listener3D {
    math::vec3 pos{0.0f, 0.0f, 0.0f};
    math::vec3 forward{0.0f, 0.0f, -1.0f}; // facing direction (unit)
    math::vec3 up{0.0f, 1.0f, 0.0f};       // up direction (unit)
    math::vec3 velocity{0.0f, 0.0f, 0.0f}; // for doppler
};

struct Source3D {
    math::vec3 pos{0.0f, 0.0f, 0.0f};
    math::vec3 velocity{0.0f, 0.0f, 0.0f}; // for doppler
};

struct SpatialMix {
    float left = 0.0f;   // left-channel gain (baseVolume * attenuation * pan)
    float right = 0.0f;  // right-channel gain
    float pitch = 1.0f;  // doppler pitch multiplier
    float distance = 0.0f;
    float pan = 0.0f;    // -1 hard-left .. +1 hard-right
};

// Distance-only gain in [0,1]. `d` clamped to [refDistance, maxDistance]; 1 at/inside ref.
inline float attenuation3D(float distance, float refDistance, float maxDistance, float rolloff,
                           Attenuation3D model) {
    if (model == Attenuation3D::None) {
        return 1.0f;
    }
    const float lo = refDistance;
    const float hi = std::max(maxDistance, refDistance + 1e-4f);
    const float d = std::min(std::max(distance, lo), hi);
    if (d <= lo) {
        return 1.0f;
    }
    switch (model) {
    case Attenuation3D::Linear: {
        const float t = (d - lo) / (hi - lo); // 0..1
        return std::max(0.0f, 1.0f - rolloff * t);
    }
    case Attenuation3D::Inverse:
        return lo / (lo + rolloff * (d - lo));
    case Attenuation3D::InverseSquare: {
        const float dl = d - lo;
        return (lo * lo) / (lo * lo + rolloff * dl * dl);
    }
    default:
        return 1.0f;
    }
}

// Pan position of a source relative to the listener's orientation: -1 hard-left .. +1 hard-right.
// Right axis = forward x up; the source's projection onto it (normalized) is the pan.
inline float panPosition(const Listener3D& l, math::vec3 sourcePos) {
    const math::vec3 toSrc = sourcePos - l.pos;
    const float len = std::sqrt(glm::dot(toSrc, toSrc));
    if (len < 1e-5f) {
        return 0.0f;
    }
    math::vec3 right = glm::cross(l.forward, l.up);
    const float rl = std::sqrt(glm::dot(right, right));
    if (rl < 1e-6f) {
        return 0.0f;
    }
    right /= rl;
    const float p = glm::dot(toSrc / len, right);
    return std::min(std::max(p, -1.0f), 1.0f);
}

// Split a gain into constant-power left/right channels for a pan in [-1,1] (left^2 + right^2 == gain^2).
inline void equalPowerPan(float pan, float gain, float& left, float& right) {
    const float t = (std::min(std::max(pan, -1.0f), 1.0f) * 0.5f + 0.5f); // 0..1
    const float angle = t * 1.5707963f;                                   // 0..pi/2
    left = gain * std::cos(angle);
    right = gain * std::sin(angle);
}

// Doppler pitch multiplier: f' / f. dHat points from source to listener. Source moving toward the
// listener (or listener toward the source) raises the pitch. Velocities are clamped below the speed of
// sound so the ratio stays positive and finite.
inline float dopplerPitch(const Listener3D& l, const Source3D& s, float speedOfSound = 343.0f) {
    const math::vec3 rel = l.pos - s.pos;
    const float len = std::sqrt(glm::dot(rel, rel));
    if (len < 1e-5f || speedOfSound <= 1e-3f) {
        return 1.0f;
    }
    const math::vec3 dHat = rel / len;
    const float cap = speedOfSound * 0.9f;
    const float vL = std::min(std::max(glm::dot(l.velocity, dHat), -cap), cap); // listener along dHat
    const float vS = std::min(std::max(glm::dot(s.velocity, dHat), -cap), cap); // source along dHat
    return (speedOfSound - vL) / (speedOfSound - vS);
}

struct SpatialConfig {
    float baseVolume = 1.0f;
    float refDistance = 1.0f;
    float maxDistance = 100.0f;
    float rolloff = 1.0f;
    float speedOfSound = 343.0f;
    Attenuation3D model = Attenuation3D::Inverse;
    bool doppler = true;
};

// The full spatialization of one source for one listener: distance attenuation x constant-power pan on
// the two channels, plus the doppler pitch.
inline SpatialMix computeSpatialMix(const Listener3D& l, const Source3D& s, const SpatialConfig& cfg) {
    SpatialMix out;
    const math::vec3 rel = s.pos - l.pos;
    out.distance = std::sqrt(glm::dot(rel, rel));
    const float att = attenuation3D(out.distance, cfg.refDistance, cfg.maxDistance, cfg.rolloff, cfg.model);
    out.pan = panPosition(l, s.pos);
    equalPowerPan(out.pan, cfg.baseVolume * att, out.left, out.right);
    out.pitch = cfg.doppler ? dopplerPitch(l, s, cfg.speedOfSound) : 1.0f;
    return out;
}

} // namespace maz::audio
