#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>

namespace maz::core {

// Fixed-timestep render interpolation — Godot's physics interpolation. Physics runs on the fixed step,
// but the display refreshes at its own (usually higher, non-dividing) rate; drawing the raw physics state
// makes motion stutter. The fix is to keep the PREVIOUS and CURRENT physics values and, each render frame,
// blend them by the leftover accumulator fraction (`Clock::interpolationAlpha()`), so a body glides
// smoothly between steps. `Interpolated<T>` is that snapshot pair; `push` is called once per fixed step,
// `sample(alpha)` once per render frame. Pure math, header-only, deterministic.

// alpha clamped to [0,1] — a render frame never extrapolates past the current step.
inline double clampAlpha(double a) { return a < 0.0 ? 0.0 : (a > 1.0 ? 1.0 : a); }

// Component-wise linear blends (overloaded so glm types keep float precision under -Wconversion).
inline double interpLerp(double a, double b, double t) { return a + (b - a) * t; }
inline float interpLerp(float a, float b, double t) { return a + (b - a) * static_cast<float>(t); }
inline math::vec2 interpLerp(const math::vec2& a, const math::vec2& b, double t) {
    return a + (b - a) * static_cast<float>(t);
}
inline math::vec3 interpLerp(const math::vec3& a, const math::vec3& b, double t) {
    return a + (b - a) * static_cast<float>(t);
}

// Angle blend along the SHORTEST arc, so interpolating across the ±pi seam turns the short way.
inline float lerpAngle(float a, float b, double t) {
    const float twoPi = 6.2831853f;
    const float pi = 3.14159265f;
    float diff = std::fmod(b - a, twoPi);
    if (diff > pi) {
        diff -= twoPi;
    } else if (diff < -pi) {
        diff += twoPi;
    }
    return a + diff * static_cast<float>(t);
}

// A previous/current value pair blended by the render alpha. Seed with an initial value; `push` once per
// fixed step (the old current becomes the new previous); `sample(alpha)` each render frame.
template <class T>
class Interpolated {
public:
    Interpolated() = default;
    explicit Interpolated(const T& initial) : m_prev(initial), m_cur(initial) {}

    void reset(const T& v) {
        m_prev = v;
        m_cur = v;
    }
    void push(const T& v) {
        m_prev = m_cur;
        m_cur = v;
    }
    const T& previous() const { return m_prev; }
    const T& current() const { return m_cur; }

    T sample(double alpha) const { return interpLerp(m_prev, m_cur, clampAlpha(alpha)); }

private:
    T m_prev{};
    T m_cur{};
};

// A 2D pose (position / rotation / scale) interpolated as a unit — rotation takes the shortest arc.
struct Transform2DState {
    math::vec2 position{0.0f, 0.0f};
    float rotation = 0.0f; // radians
    math::vec2 scale{1.0f, 1.0f};
};

inline Transform2DState interpolate(const Transform2DState& a, const Transform2DState& b, double alpha) {
    const double t = clampAlpha(alpha);
    Transform2DState r;
    r.position = interpLerp(a.position, b.position, t);
    r.rotation = lerpAngle(a.rotation, b.rotation, t);
    r.scale = interpLerp(a.scale, b.scale, t);
    return r;
}

} // namespace maz::core
