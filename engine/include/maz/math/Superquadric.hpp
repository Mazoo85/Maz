#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>

// maz::math superquadric (superellipsoid) — the 3D family of shapes that morph between a box, a sphere, a
// cylinder-ish barrel, a rounded cube, and a double-cone/octahedron by turning two "squareness" knobs. It is
// the 3D generalisation of the engine's 2D superellipse (Superellipse.hpp): semi-axes (a,b,c) set the size,
// and two exponents (e1 along the poles, e2 around the equator) set the roundness. Superquadrics are a
// classic procedural-modelling and shape-fitting primitive (rounded crates, pebbles, capsule-ish hulls,
// point-cloud fitting). This gives the parametric surface point, the exact surface normal, and the
// inside-outside function (a scalar that is <1 inside, ==1 on the surface, >1 outside). Godot has no
// superquadric primitive. Header-only, std-only, deterministic.
namespace maz::math {

namespace sq_detail {
// Sign-preserving power: sgn(x) * |x|^p, with 0 mapping to 0.
inline float signpow(float x, float p) {
    if (x > 0.0f) {
        return std::pow(x, p);
    }
    if (x < 0.0f) {
        return -std::pow(-x, p);
    }
    return 0.0f;
}
} // namespace sq_detail

// A point on the superellipsoid surface. `u` is latitude in [-pi/2, pi/2], `v` is longitude in [-pi, pi].
// (a,b,c) are the semi-axes; e1 shapes the north-south profile, e2 the east-west profile. e1=e2=1 is an
// ellipsoid; small exponents approach a box; large exponents approach an octahedron.
inline vec3 superellipsoidPoint(float a, float b, float c, float e1, float e2, float u, float v) {
    using sq_detail::signpow;
    const float cu = std::cos(u), su = std::sin(u);
    const float cv = std::cos(v), sv = std::sin(v);
    const float cuE = signpow(cu, e1);
    return vec3(a * cuE * signpow(cv, e2), b * cuE * signpow(sv, e2), c * signpow(su, e1));
}

// The inside-outside function F(p): < 1 strictly inside, == 1 on the surface, > 1 outside. This is the
// canonical superquadric implicit; it is NOT a Euclidean distance, but its 1-level-set is the surface.
inline float superquadricInsideOutside(float a, float b, float c, float e1, float e2, const vec3& p) {
    const float X = std::pow(std::fabs(p.x / a), 2.0f / e2);
    const float Y = std::pow(std::fabs(p.y / b), 2.0f / e2);
    const float Z = std::pow(std::fabs(p.z / c), 2.0f / e1);
    return std::pow(X + Y, e2 / e1) + Z;
}

// The outward unit surface normal at `p` (assumed on/near the surface): the normalized gradient of the
// inside-outside function, via central differences. Returns +Z if the gradient is degenerate.
inline vec3 superquadricNormal(float a, float b, float c, float e1, float e2, const vec3& p) {
    const float h = 1e-3f;
    auto F = [&](const vec3& q) { return superquadricInsideOutside(a, b, c, e1, e2, q); };
    const float gx = F(vec3(p.x + h, p.y, p.z)) - F(vec3(p.x - h, p.y, p.z));
    const float gy = F(vec3(p.x, p.y + h, p.z)) - F(vec3(p.x, p.y - h, p.z));
    const float gz = F(vec3(p.x, p.y, p.z + h)) - F(vec3(p.x, p.y, p.z - h));
    const vec3 g(gx, gy, gz);
    const float l = std::sqrt(g.x * g.x + g.y * g.y + g.z * g.z);
    return l > 1e-12f ? g * (1.0f / l) : vec3(0.0f, 0.0f, 1.0f);
}

} // namespace maz::math
