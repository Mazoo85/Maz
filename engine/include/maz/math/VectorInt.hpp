#pragma once

#include "maz/math/Math.hpp" // vec2, vec3

#include <cmath>
#include <cstdint>
#include <cstdlib> // std::abs

// maz::math integer vectors — Godot's Vector2i / Vector3i, the whole-number companions to vec2/vec3.
// Grid and tile coordinates, array indices, pixel sizes, and hash keys all want exact integer math,
// not floats that drift. These give the Godot API: component/scalar arithmetic (integer division,
// truncating like Godot), abs / sign, component-wise clamp / min / max, length (as a double) and
// exact 64-bit lengthSquared (no overflow), distance queries, aspect (2i), and float-vec conversion.
// Header-only, pure, exact — unit-tested to the integer.
namespace maz::math {

inline int isigni(int v) { return (v > 0) - (v < 0); } // -1 / 0 / +1

struct Vector2i {
    int x = 0;
    int y = 0;

    Vector2i() = default;
    Vector2i(int x_, int y_) : x(x_), y(y_) {}

    Vector2i operator+(const Vector2i& o) const { return {x + o.x, y + o.y}; }
    Vector2i operator-(const Vector2i& o) const { return {x - o.x, y - o.y}; }
    Vector2i operator*(const Vector2i& o) const { return {x * o.x, y * o.y}; }
    Vector2i operator/(const Vector2i& o) const { return {x / o.x, y / o.y}; } // integer division
    Vector2i operator*(int s) const { return {x * s, y * s}; }
    Vector2i operator/(int s) const { return {x / s, y / s}; }
    Vector2i operator-() const { return {-x, -y}; }
    bool operator==(const Vector2i& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vector2i& o) const { return !(*this == o); }

    Vector2i abs() const { return {std::abs(x), std::abs(y)}; }
    Vector2i sign() const { return {isigni(x), isigni(y)}; }
    Vector2i clamp(const Vector2i& lo, const Vector2i& hi) const {
        return {x < lo.x ? lo.x : (x > hi.x ? hi.x : x), y < lo.y ? lo.y : (y > hi.y ? hi.y : y)};
    }
    Vector2i min(const Vector2i& o) const { return {x < o.x ? x : o.x, y < o.y ? y : o.y}; }
    Vector2i max(const Vector2i& o) const { return {x > o.x ? x : o.x, y > o.y ? y : o.y}; }

    std::int64_t lengthSquared() const {
        return static_cast<std::int64_t>(x) * x + static_cast<std::int64_t>(y) * y;
    }
    double length() const { return std::sqrt(static_cast<double>(lengthSquared())); }
    double distanceTo(const Vector2i& o) const { return (o - *this).length(); }
    std::int64_t distanceSquaredTo(const Vector2i& o) const { return (o - *this).lengthSquared(); }
    float aspect() const { return static_cast<float>(x) / static_cast<float>(y); }

    vec2 toVec2() const { return vec2(static_cast<float>(x), static_cast<float>(y)); }
};

struct Vector3i {
    int x = 0;
    int y = 0;
    int z = 0;

    Vector3i() = default;
    Vector3i(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}

    Vector3i operator+(const Vector3i& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3i operator-(const Vector3i& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3i operator*(const Vector3i& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vector3i operator/(const Vector3i& o) const { return {x / o.x, y / o.y, z / o.z}; }
    Vector3i operator*(int s) const { return {x * s, y * s, z * s}; }
    Vector3i operator/(int s) const { return {x / s, y / s, z / s}; }
    Vector3i operator-() const { return {-x, -y, -z}; }
    bool operator==(const Vector3i& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vector3i& o) const { return !(*this == o); }

    Vector3i abs() const { return {std::abs(x), std::abs(y), std::abs(z)}; }
    Vector3i sign() const { return {isigni(x), isigni(y), isigni(z)}; }
    Vector3i clamp(const Vector3i& lo, const Vector3i& hi) const {
        return {x < lo.x ? lo.x : (x > hi.x ? hi.x : x), y < lo.y ? lo.y : (y > hi.y ? hi.y : y),
                z < lo.z ? lo.z : (z > hi.z ? hi.z : z)};
    }
    Vector3i min(const Vector3i& o) const {
        return {x < o.x ? x : o.x, y < o.y ? y : o.y, z < o.z ? z : o.z};
    }
    Vector3i max(const Vector3i& o) const {
        return {x > o.x ? x : o.x, y > o.y ? y : o.y, z > o.z ? z : o.z};
    }

    std::int64_t lengthSquared() const {
        return static_cast<std::int64_t>(x) * x + static_cast<std::int64_t>(y) * y +
               static_cast<std::int64_t>(z) * z;
    }
    double length() const { return std::sqrt(static_cast<double>(lengthSquared())); }
    double distanceTo(const Vector3i& o) const { return (o - *this).length(); }
    std::int64_t distanceSquaredTo(const Vector3i& o) const { return (o - *this).lengthSquared(); }

    vec3 toVec3() const {
        return vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
};

} // namespace maz::math
