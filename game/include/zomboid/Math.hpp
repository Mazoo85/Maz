// ZOMBOID: ANCHORAGE — minimal math for the deterministic simulation.
// The sim is engine-agnostic (no GLM/Vulkan) so it can run and be unit-tested
// headless anywhere. The render layer bridges these types to maz::math later.
#pragma once

#include <cmath>

namespace zb {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

inline float length(float dx, float dy) { return std::sqrt(dx * dx + dy * dy); }

inline float dist(Vec2 a, Vec2 b) { return length(a.x - b.x, a.y - b.y); }

// Wrap an angle into (-pi, pi], matching the reference game's normAng.
inline float normAngle(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

} // namespace zb
