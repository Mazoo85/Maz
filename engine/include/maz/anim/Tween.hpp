#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::anim {

// Easing + tweening: the engine's general-purpose "animate a value from A to B over time" toolkit.
// Cross-cutting on purpose — the same curves drive UI transitions, moving platforms, doors, camera
// moves, color fades, and gameplay juice. Pure math (no GPU/allocation), so it unit-tests headless
// and stays deterministic under the fixed timestep.

// Standard easing curves (Penner-style). Each maps normalized time t in [0,1] to an eased value;
// most stay in [0,1] but Back/Elastic deliberately overshoot for anticipation/spring effects.
enum class Ease {
    Linear,
    QuadIn,
    QuadOut,
    QuadInOut,
    CubicIn,
    CubicOut,
    CubicInOut,
    SineIn,
    SineOut,
    SineInOut,
    ExpoOut,
    CircOut,
    BackOut,
    ElasticOut,
    BounceOut,
};

// BounceOut is defined out-of-line-ish below; declared here so the switch can call it.
inline float bounceOut(float t) {
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

// Evaluate an easing curve. t is clamped to [0,1] first, so callers can pass raw progress safely.
inline float ease(Ease type, float t) {
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    constexpr float kPi = 3.14159265358979323846f;
    switch (type) {
        case Ease::Linear:
            return t;
        case Ease::QuadIn:
            return t * t;
        case Ease::QuadOut:
            return t * (2.0f - t);
        case Ease::QuadInOut:
            return t < 0.5f ? 2.0f * t * t : 1.0f - 0.5f * (2.0f * t - 2.0f) * (2.0f * t - 2.0f);
        case Ease::CubicIn:
            return t * t * t;
        case Ease::CubicOut: {
            const float u = t - 1.0f;
            return u * u * u + 1.0f;
        }
        case Ease::CubicInOut:
            if (t < 0.5f) {
                return 4.0f * t * t * t;
            } else {
                const float u = 2.0f * t - 2.0f;
                return 0.5f * u * u * u + 1.0f;
            }
        case Ease::SineIn:
            return 1.0f - std::cos(t * kPi * 0.5f);
        case Ease::SineOut:
            return std::sin(t * kPi * 0.5f);
        case Ease::SineInOut:
            return -0.5f * (std::cos(kPi * t) - 1.0f);
        case Ease::ExpoOut:
            return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
        case Ease::CircOut: {
            const float u = t - 1.0f;
            return std::sqrt(1.0f - u * u);
        }
        case Ease::BackOut: {
            constexpr float c1 = 1.70158f;
            constexpr float c3 = c1 + 1.0f;
            const float u = t - 1.0f;
            return 1.0f + c3 * u * u * u + c1 * u * u;
        }
        case Ease::ElasticOut: {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            constexpr float c4 = (2.0f * kPi) / 3.0f;
            return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }
        case Ease::BounceOut:
            return bounceOut(t);
    }
    return t;
}

// Generic linear interpolation. Works for float and glm vectors (via glm::mix); t is not clamped so
// callers can extrapolate if they want, but Tween::eased() always feeds a clamped, curved value.
template <typename T>
inline T mix(const T& a, const T& b, float t) {
    return glm::mix(a, b, t);
}
inline float mix(float a, float b, float t) {
    return a + (b - a) * t;
}

enum class Loop {
    Once,     // clamp at the end (finished = true)
    Repeat,   // wrap back to 0 and keep going
    PingPong, // bounce 0->1->0->1... (finished never set)
};

// A time cursor over [0, duration] with a loop policy. Advance it with update(dt); read normalized
// progress() (0..1, raw) or eased() (curve applied), and sample() to interpolate any value. One
// Tween can drive many values by calling sample() with different from/to pairs.
struct Tween {
    float duration = 1.0f;
    Ease easing = Ease::Linear;
    Loop loop = Loop::Once;
    float time = 0.0f;      // elapsed time within the current cycle direction
    bool finished = false;  // set only for Loop::Once when it reaches the end
    bool reversing = false; // PingPong: currently travelling 1 -> 0

    void reset() {
        time = 0.0f;
        finished = false;
        reversing = false;
    }

    void update(float dt) {
        if (finished || duration <= 0.0f) {
            if (duration <= 0.0f) {
                finished = loop == Loop::Once;
            }
            return;
        }
        time += dt;
        while (time >= duration) {
            switch (loop) {
                case Loop::Once:
                    time = duration;
                    finished = true;
                    return;
                case Loop::Repeat:
                    time -= duration;
                    break;
                case Loop::PingPong:
                    time -= duration;
                    reversing = !reversing;
                    break;
            }
        }
    }

    // Raw progress in [0,1], accounting for ping-pong direction.
    float progress() const {
        if (duration <= 0.0f) {
            return 1.0f;
        }
        const float p = time / duration;
        return reversing ? 1.0f - p : p;
    }

    // Curved progress in (roughly) [0,1] — progress() run through the easing function.
    float eased() const { return ease(easing, progress()); }

    // Interpolate from -> to using the current eased progress. Qualified calls so only anim::mix is
    // considered (an unqualified call would also pull in glm::mix via ADL for vector types).
    template <typename T>
    T sample(const T& from, const T& to) const {
        return maz::anim::mix(from, to, eased());
    }
    float sample(float from, float to) const { return maz::anim::mix(from, to, eased()); }
};

} // namespace maz::anim
