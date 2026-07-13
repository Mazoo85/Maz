#pragma once

// Scalar interpolation, remapping, and easing helpers built on the maz::math
// GLM aliases. Include this rather than GLM directly (see Math.hpp).
//
// Conventions:
//   - Everything here is float, inline, and header-only.
//   - Easing curves take t in [0,1] and (for the standard eases) return values
//     that pass through 0 at t==0 and 1 at t==1.
//   - lerpAngle interpolates along the shortest angular path; its result is NOT
//     normalized to [0,2pi).

#include "maz/math/Math.hpp"

#include <glm/gtc/constants.hpp>

#include <cmath>

namespace maz::math {

inline float clampf(float x, float lo, float hi) {
    return glm::clamp(x, lo, hi);
}

inline float saturate(float x) {
    return clampf(x, 0.0f, 1.0f);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float inverseLerp(float a, float b, float v) {
    return (a == b) ? 0.0f : (v - a) / (b - a);
}

inline float remap(float v, float inMin, float inMax, float outMin, float outMax) {
    return lerp(outMin, outMax, inverseLerp(inMin, inMax, v));
}

inline float smoothstep(float e0, float e1, float x) {
    float t = saturate(inverseLerp(e0, e1, x));
    return t * t * (3.0f - 2.0f * t);
}

inline float smootherstep(float e0, float e1, float x) {
    float t = saturate(inverseLerp(e0, e1, x));
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float moveToward(float from, float to, float maxDelta) {
    float d = to - from;
    if (std::fabs(d) <= std::fabs(maxDelta)) {
        return to;
    }
    return from + (d < 0.0f ? -1.0f : 1.0f) * std::fabs(maxDelta);
}

// Interpolates along the shortest angular path. The two-branch wrap is
// load-bearing: it folds the raw delta into [-pi,pi] before scaling. NOTE: the
// result is NOT normalized to [0,2pi).
inline float lerpAngle(float a, float b, float t) {
    const float pi = glm::pi<float>();
    const float twoPi = 2.0f * pi;
    float delta = std::fmod(b - a, twoPi);
    if (delta > pi) {
        delta -= twoPi;
    } else if (delta < -pi) {
        delta += twoPi;
    }
    return a + delta * t;
}

enum class Easing {
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    InSine,
    OutSine,
    InOutSine
};

inline float easeInQuad(float t) {
    return t * t;
}

inline float easeOutQuad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

inline float easeInOutQuad(float t) {
    return (t < 0.5f) ? 2.0f * t * t : 1.0f - 0.5f * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f);
}

inline float easeInCubic(float t) {
    return t * t * t;
}

inline float easeOutCubic(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

inline float easeInOutCubic(float t) {
    return (t < 0.5f) ? 4.0f * t * t * t
                      : 1.0f - 0.5f * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f);
}

inline float easeInSine(float t) {
    const float pi = glm::pi<float>();
    return 1.0f - std::cos(t * pi * 0.5f);
}

inline float easeOutSine(float t) {
    const float pi = glm::pi<float>();
    return std::sin(t * pi * 0.5f);
}

inline float easeInOutSine(float t) {
    const float pi = glm::pi<float>();
    return -0.5f * (std::cos(pi * t) - 1.0f);
}

inline float ease(Easing e, float t) {
    switch (e) {
    case Easing::Linear:
        return t;
    case Easing::InQuad:
        return easeInQuad(t);
    case Easing::OutQuad:
        return easeOutQuad(t);
    case Easing::InOutQuad:
        return easeInOutQuad(t);
    case Easing::InCubic:
        return easeInCubic(t);
    case Easing::OutCubic:
        return easeOutCubic(t);
    case Easing::InOutCubic:
        return easeInOutCubic(t);
    case Easing::InSine:
        return easeInSine(t);
    case Easing::OutSine:
        return easeOutSine(t);
    case Easing::InOutSine:
        return easeInOutSine(t);
    }
    return t;
}

} // namespace maz::math
