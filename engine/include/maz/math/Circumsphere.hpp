#pragma once

#include "maz/math/Math.hpp" // vec3, mat3, dot

#include <cmath>

// maz::math circumsphere of a tetrahedron — the unique sphere passing through four 3D points (its centre is
// equidistant from all four). This is the 3D companion to a triangle's circumcircle, and the core predicate
// of 3D Delaunay tetrahedralisation: a tetralisation is Delaunay iff no vertex lies inside any tetrahedron's
// circumsphere (the "in-sphere test"). Also used for bounding spheres of simplices, mesh-quality metrics
// (radius-edge ratio), and sphere-fitting. The engine has a Ritter-style bounding sphere (BoundingSphere.hpp)
// but no exact sphere-through-4-points; Godot has neither. Returns the centre, radius, and a validity flag
// (false when the four points are coplanar — no finite sphere). Header-only, std-only, deterministic.
namespace maz::math {

struct Circumsphere {
    bool valid = false;
    vec3 center{0.0f};
    float radius = 0.0f;
};

// The circumsphere of the tetrahedron (a, b, c, d). `valid` is false for (near-)coplanar inputs. Solves the
// 3x3 linear system from |x-a|^2 = |x-b|^2 = |x-c|^2 = |x-d|^2 (each equality is linear in the centre x).
inline Circumsphere circumsphere(const vec3& a, const vec3& b, const vec3& c, const vec3& d) {
    Circumsphere out;
    // Rows of the system: 2*(p-a) . x = |p|^2 - |a|^2, for p in {b, c, d}.
    const vec3 r0 = (b - a) * 2.0f;
    const vec3 r1 = (c - a) * 2.0f;
    const vec3 r2 = (d - a) * 2.0f;
    const float aa = dot(a, a);
    const vec3 e(dot(b, b) - aa, dot(c, c) - aa, dot(d, d) - aa);
    // Column-major mat3 whose ROWS are r0, r1, r2.
    const mat3 M(vec3(r0.x, r1.x, r2.x), vec3(r0.y, r1.y, r2.y), vec3(r0.z, r1.z, r2.z));
    const float det = glm::determinant(M);
    // Scale-aware singularity guard: the determinant grows like (edge length)^3.
    const float edge = std::sqrt(dot(b - a, b - a)) + std::sqrt(dot(c - a, c - a)) + std::sqrt(dot(d - a, d - a));
    const float scale = edge * edge * edge + 1e-20f;
    if (std::fabs(det) < 1e-9f * scale) {
        return out; // coplanar / degenerate
    }
    out.center = glm::inverse(M) * e;
    out.radius = std::sqrt(dot(out.center - a, out.center - a));
    out.valid = true;
    return out;
}

// In-sphere test: is `p` strictly inside the circumsphere of (a, b, c, d)? Returns false if degenerate or if
// `p` is on/outside the sphere. The signed form used by Delaunay flips; this convenience returns the boolean.
inline bool insideCircumsphere(const vec3& a, const vec3& b, const vec3& c, const vec3& d, const vec3& p) {
    const Circumsphere s = circumsphere(a, b, c, d);
    if (!s.valid) {
        return false;
    }
    return dot(p - s.center, p - s.center) < s.radius * s.radius;
}

} // namespace maz::math
