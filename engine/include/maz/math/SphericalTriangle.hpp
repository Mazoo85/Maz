#pragma once

#include "maz/math/Math.hpp"      // vec3
#include "maz/math/VectorOps.hpp" // dot

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math spherical triangle — the area (equivalently, the SOLID ANGLE) of a triangle drawn on a sphere
// from three directions/vertices. This is what you need for the solid angle a triangle light or window
// subtends at a point (form factors, importance sampling, soft shadows), the fraction of the sky/globe a
// region covers, geodesic-dome face areas, and spherical coverage tests — none of which Godot exposes.
// A flat triangle's angles sum to π; a spherical one's sum EXCEEDS π, and that excess IS the area (Girard's
// theorem). The area is computed with the numerically-robust Van Oosterom–Strackee formula directly from the
// three unit vectors. All inputs are treated as unit directions from the sphere centre. Header-only,
// std-only, deterministic.
namespace maz::math {

namespace detail {
inline vec3 cross3(const vec3& u, const vec3& v) {
    return vec3(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
}
} // namespace detail

// The solid angle (steradians) subtended by the spherical triangle with unit-vector vertices a, b, c — which
// equals its area on the unit sphere. Van Oosterom–Strackee: tan(Ω/2) = |a·(b×c)| / (1 + a·b + b·c + c·a).
inline float sphericalTriangleArea(const vec3& a, const vec3& b, const vec3& c) {
    const float num = std::fabs(dot(a, detail::cross3(b, c)));
    const float den = 1.0f + dot(a, b) + dot(b, c) + dot(c, a);
    float omega = 2.0f * std::atan2(num, den); // atan2 handles den <= 0 (triangles past a hemisphere)
    if (omega < 0.0f) {
        omega += 6.28318530717958648f;
    }
    return omega;
}

// The interior angle (radians) of the spherical triangle at vertex `at`, between the great-circle arcs to
// `u` and `v` — the angle between the arcs' tangent directions at `at`.
inline float sphericalTriangleAngle(const vec3& at, const vec3& u, const vec3& v) {
    // Tangent to arc (at->x) at `at` is x with its `at` component removed, normalised.
    vec3 tu = u - at * dot(at, u);
    vec3 tv = v - at * dot(at, v);
    const float lu = std::sqrt(dot(tu, tu));
    const float lv = std::sqrt(dot(tv, tv));
    if (lu < 1e-9f || lv < 1e-9f) {
        return 0.0f;
    }
    tu = tu * (1.0f / lu);
    tv = tv * (1.0f / lv);
    float c = dot(tu, tv);
    c = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c);
    return std::acos(c);
}

// The spherical excess (A + B + C − π) of the triangle, which by Girard's theorem equals its area.
inline float sphericalExcess(const vec3& a, const vec3& b, const vec3& c) {
    const float A = sphericalTriangleAngle(a, b, c);
    const float B = sphericalTriangleAngle(b, c, a);
    const float C = sphericalTriangleAngle(c, a, b);
    return A + B + C - 3.14159265358979324f;
}

// The area (solid angle) of a spherical polygon given as unit-vector vertices in order, by fanning triangles
// from the first vertex. Correct for polygons that are star-shaped from vertex 0 (e.g. any convex spherical
// polygon). Returns 0 for fewer than 3 vertices.
inline float sphericalPolygonArea(const std::vector<vec3>& verts) {
    if (verts.size() < 3) {
        return 0.0f;
    }
    float total = 0.0f;
    for (std::size_t i = 1; i + 1 < verts.size(); ++i) {
        total += sphericalTriangleArea(verts[0], verts[i], verts[i + 1]);
    }
    return total;
}

} // namespace maz::math
