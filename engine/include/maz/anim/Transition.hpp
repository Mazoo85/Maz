#pragma once

#include <algorithm>
#include <cmath>

// maz::anim complete easing/transition set — Godot's Tween transition matrix. The existing
// anim::Tween (M59) carries a handy but partial easing enum; this adds the FULL Godot model: 12
// transition types x 4 ease types, computed with the Robert Penner equations Godot uses, so a Maz
// tween can reproduce any Godot ease exactly. All curves are normalised to t in [0,1] -> value
// (0->0, 1->1); Back/Elastic/Spring deliberately overshoot for anticipation/springy motion. Pure,
// header-only, deterministic — unit-tests exactly against known values and endpoint/symmetry laws.
namespace maz::anim {

enum class TransitionType {
    Linear,
    Sine,
    Quint,
    Quart,
    Quad,
    Expo,
    Elastic,
    Cubic,
    Circ,
    Bounce,
    Back,
    Spring,
};

enum class EaseType { In, Out, InOut, OutIn };

namespace detail {

inline float bounceOutEq(float t) {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

// Godot's spring "out" equation.
inline float springOutEq(float t) {
    const float pi = 3.14159265358979323846f;
    const float s = 1.0f - t;
    const float v = (std::sin(t * pi * (0.2f + 2.5f * t * t * t)) * std::pow(s, 2.2f) + t) *
                    (1.0f + (1.2f * s));
    return v;
}

// The IN form of each transition (0->0, 1->1). Out/InOut/OutIn are derived generically.
inline float easeIn(TransitionType type, float t) {
    const float pi = 3.14159265358979323846f;
    switch (type) {
    case TransitionType::Linear:
        return t;
    case TransitionType::Quad:
        return t * t;
    case TransitionType::Cubic:
        return t * t * t;
    case TransitionType::Quart:
        return t * t * t * t;
    case TransitionType::Quint:
        return t * t * t * t * t;
    case TransitionType::Sine:
        return 1.0f - std::cos(t * pi * 0.5f);
    case TransitionType::Expo:
        return (t <= 0.0f) ? 0.0f : (t >= 1.0f ? 1.0f : std::pow(2.0f, 10.0f * (t - 1.0f)));
    case TransitionType::Circ:
        return 1.0f - std::sqrt(std::max(0.0f, 1.0f - t * t));
    case TransitionType::Back: {
        const float s = 1.70158f;
        return t * t * ((s + 1.0f) * t - s);
    }
    case TransitionType::Elastic: {
        if (t <= 0.0f) {
            return 0.0f;
        }
        if (t >= 1.0f) {
            return 1.0f;
        }
        const float p = 0.3f;
        const float s = p * 0.25f;
        const float tt = t - 1.0f;
        return -(std::pow(2.0f, 10.0f * tt) * std::sin((tt - s) * (2.0f * pi) / p));
    }
    case TransitionType::Bounce:
        return 1.0f - bounceOutEq(1.0f - t);
    case TransitionType::Spring:
        return 1.0f - springOutEq(1.0f - t);
    }
    return t;
}

} // namespace detail

// Evaluate transition `type` with ease `ease` at normalised time t in [0,1].
inline float applyTransition(TransitionType type, EaseType ease, float t) {
    switch (ease) {
    case EaseType::In:
        return detail::easeIn(type, t);
    case EaseType::Out:
        return 1.0f - detail::easeIn(type, 1.0f - t);
    case EaseType::InOut:
        return t < 0.5f ? 0.5f * detail::easeIn(type, 2.0f * t)
                        : 1.0f - 0.5f * detail::easeIn(type, 2.0f - 2.0f * t);
    case EaseType::OutIn:
        return t < 0.5f ? 0.5f * (1.0f - detail::easeIn(type, 1.0f - 2.0f * t))
                        : 0.5f + 0.5f * detail::easeIn(type, 2.0f * t - 1.0f);
    }
    return t;
}

// Godot Tween.interpolate_value: from + (to-from) * eased(elapsed/duration), clamped to the ends.
inline float tweenInterpolate(float from, float to, float elapsed, float duration,
                              TransitionType type, EaseType ease) {
    if (duration <= 0.0f) {
        return to;
    }
    float t = elapsed / duration;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return from + (to - from) * applyTransition(type, ease, t);
}

} // namespace maz::anim
