#pragma once

#include "maz/math/Math.hpp" // vec4

#include <cmath>
#include <cstdint>
#include <cstdlib> // std::abs

// maz::math 4D vectors — Godot's Vector4 / Vector4i. The float Vector4 carries the everyday API
// (length / normalized / dot / lerp / abs / sign / clamp / min / max / floor / ceil / round / snapped /
// distanceTo / directionTo / isEqualApprox), the same surface vec2/vec3 gameplay code already relies on,
// extended to four components — RGBA colour math, shader-uniform packing, homogeneous points, quaternion
// storage, and 4-wide data. Vector4i is the whole-number companion (grid/index math with truncating
// integer division, exact 64-bit lengthSquared, no float drift). Both are header-only, pure and exact —
// unit-tested component by component. A `toVec4()` bridges to GLM for the rendering path.
namespace maz::math {

inline int isigni4(int v) { return (v > 0) - (v < 0); } // -1 / 0 / +1
inline float fsignf4(float v) { return static_cast<float>((v > 0.0f) - (v < 0.0f)); }

struct Vector4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vector4() = default;
    Vector4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    Vector4 operator+(const Vector4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vector4 operator-(const Vector4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vector4 operator*(const Vector4& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
    Vector4 operator/(const Vector4& o) const { return {x / o.x, y / o.y, z / o.z, w / o.w}; }
    Vector4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    Vector4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    Vector4 operator-() const { return {-x, -y, -z, -w}; }
    bool operator==(const Vector4& o) const {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
    bool operator!=(const Vector4& o) const { return !(*this == o); }

    float dot(const Vector4& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    float lengthSquared() const { return dot(*this); }
    float length() const { return std::sqrt(lengthSquared()); }

    Vector4 normalized() const {
        const float len = length();
        return len > 0.0f ? *this / len : Vector4{};
    }
    bool isNormalized() const { return std::abs(lengthSquared() - 1.0f) < 1e-5f; }

    Vector4 abs() const { return {std::abs(x), std::abs(y), std::abs(z), std::abs(w)}; }
    Vector4 sign() const { return {fsignf4(x), fsignf4(y), fsignf4(z), fsignf4(w)}; }
    Vector4 floor() const {
        return {std::floor(x), std::floor(y), std::floor(z), std::floor(w)};
    }
    Vector4 ceil() const { return {std::ceil(x), std::ceil(y), std::ceil(z), std::ceil(w)}; }
    Vector4 round() const {
        return {std::round(x), std::round(y), std::round(z), std::round(w)};
    }

    Vector4 min(const Vector4& o) const {
        return {x < o.x ? x : o.x, y < o.y ? y : o.y, z < o.z ? z : o.z, w < o.w ? w : o.w};
    }
    Vector4 max(const Vector4& o) const {
        return {x > o.x ? x : o.x, y > o.y ? y : o.y, z > o.z ? z : o.z, w > o.w ? w : o.w};
    }
    Vector4 clamp(const Vector4& lo, const Vector4& hi) const {
        return {x < lo.x ? lo.x : (x > hi.x ? hi.x : x), y < lo.y ? lo.y : (y > hi.y ? hi.y : y),
                z < lo.z ? lo.z : (z > hi.z ? hi.z : z), w < lo.w ? lo.w : (w > hi.w ? hi.w : w)};
    }

    Vector4 lerp(const Vector4& to, float t) const { return *this + (to - *this) * t; }

    // Round each component to the nearest multiple of the matching `step` component (Godot's snapped).
    Vector4 snapped(const Vector4& step) const {
        auto snap1 = [](float v, float s) { return s != 0.0f ? std::round(v / s) * s : v; };
        return {snap1(x, step.x), snap1(y, step.y), snap1(z, step.z), snap1(w, step.w)};
    }

    float distanceTo(const Vector4& o) const { return (o - *this).length(); }
    float distanceSquaredTo(const Vector4& o) const { return (o - *this).lengthSquared(); }
    Vector4 directionTo(const Vector4& o) const { return (o - *this).normalized(); }

    bool isEqualApprox(const Vector4& o, float eps = 1e-5f) const {
        return std::abs(x - o.x) < eps && std::abs(y - o.y) < eps && std::abs(z - o.z) < eps &&
               std::abs(w - o.w) < eps;
    }

    // Every component finite (no NaN/inf) — Godot's Vector4.is_finite.
    bool isFinite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
    }
    // Every component within `eps` of zero — Godot's Vector4.is_zero_approx.
    bool isZeroApprox(float eps = 1e-5f) const {
        return std::abs(x) < eps && std::abs(y) < eps && std::abs(z) < eps && std::abs(w) < eps;
    }

    vec4 toVec4() const { return vec4(x, y, z, w); }
};

inline Vector4 operator*(float s, const Vector4& v) { return v * s; }

struct Vector4i {
    int x = 0;
    int y = 0;
    int z = 0;
    int w = 0;

    Vector4i() = default;
    Vector4i(int x_, int y_, int z_, int w_) : x(x_), y(y_), z(z_), w(w_) {}

    Vector4i operator+(const Vector4i& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vector4i operator-(const Vector4i& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vector4i operator*(const Vector4i& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
    Vector4i operator/(const Vector4i& o) const {
        return {x / o.x, y / o.y, z / o.z, w / o.w}; // integer division
    }
    Vector4i operator*(int s) const { return {x * s, y * s, z * s, w * s}; }
    Vector4i operator/(int s) const { return {x / s, y / s, z / s, w / s}; }
    Vector4i operator-() const { return {-x, -y, -z, -w}; }
    bool operator==(const Vector4i& o) const {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
    bool operator!=(const Vector4i& o) const { return !(*this == o); }

    Vector4i abs() const { return {std::abs(x), std::abs(y), std::abs(z), std::abs(w)}; }
    Vector4i sign() const { return {isigni4(x), isigni4(y), isigni4(z), isigni4(w)}; }
    Vector4i clamp(const Vector4i& lo, const Vector4i& hi) const {
        return {x < lo.x ? lo.x : (x > hi.x ? hi.x : x), y < lo.y ? lo.y : (y > hi.y ? hi.y : y),
                z < lo.z ? lo.z : (z > hi.z ? hi.z : z), w < lo.w ? lo.w : (w > hi.w ? hi.w : w)};
    }
    Vector4i min(const Vector4i& o) const {
        return {x < o.x ? x : o.x, y < o.y ? y : o.y, z < o.z ? z : o.z, w < o.w ? w : o.w};
    }
    Vector4i max(const Vector4i& o) const {
        return {x > o.x ? x : o.x, y > o.y ? y : o.y, z > o.z ? z : o.z, w > o.w ? w : o.w};
    }

    std::int64_t lengthSquared() const {
        return static_cast<std::int64_t>(x) * x + static_cast<std::int64_t>(y) * y +
               static_cast<std::int64_t>(z) * z + static_cast<std::int64_t>(w) * w;
    }
    double length() const { return std::sqrt(static_cast<double>(lengthSquared())); }
    double distanceTo(const Vector4i& o) const { return (o - *this).length(); }
    std::int64_t distanceSquaredTo(const Vector4i& o) const { return (o - *this).lengthSquared(); }

    vec4 toVec4() const {
        return vec4(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
                    static_cast<float>(w));
    }
};

} // namespace maz::math
