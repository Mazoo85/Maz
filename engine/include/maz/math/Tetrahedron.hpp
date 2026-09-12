#pragma once

#include "maz/math/Math.hpp"       // vec3
#include "maz/math/VectorOps.hpp"  // dot
#include "maz/math/Geometry3D.hpp" // closestPointOnTriangle

#include <cmath>

// maz::math tetrahedron utilities — signed volume, barycentric coordinates, point-in-tetrahedron and the
// closest point on/in a tetrahedron. Tetrahedra are the 3D analogue of triangles: they are the cells of
// volumetric (tet) meshes, the interpolation stencil for scattered 3D data (FEM, fluid/soft-body sims,
// volumetric fields), the containment primitive for deformation "cages", and the building block GJK/EPA walk
// through. Barycentric coordinates express any point as a weighted blend of the four corners (weights summing
// to 1, all non-negative exactly when the point is inside) — the natural way to interpolate a colour, weight
// or field value stored at the vertices. Godot exposes none of this. Header-only, std-only, deterministic.
namespace maz::math {

namespace detail {
inline vec3 tetCross(const vec3& u, const vec3& v) {
    return vec3(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
}
// Six times the signed volume of tetrahedron (a,b,c,d): (b-a)·((c-a)×(d-a)).
inline float sixVolume(const vec3& a, const vec3& b, const vec3& c, const vec3& d) {
    return dot(b - a, tetCross(c - a, d - a));
}
} // namespace detail

// The signed volume of the tetrahedron (a,b,c,d). Positive when (b,c,d) is counter-clockwise seen from a
// (i.e. d is on the positive side of the plane abc); the sign flips when two vertices are swapped.
inline float tetrahedronVolume(const vec3& a, const vec3& b, const vec3& c, const vec3& d) {
    return detail::sixVolume(a, b, c, d) / 6.0f;
}

// Barycentric weights (w0..w3, summing to 1) of point `p` with respect to the tetrahedron (a,b,c,d), so that
// p = w0*a + w1*b + w2*c + w3*d. All four are >= 0 exactly when p lies inside (or on) the tetrahedron.
struct BaryTet {
    float w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

inline BaryTet barycentricTetrahedron(const vec3& p, const vec3& a, const vec3& b, const vec3& c,
                                      const vec3& d) {
    BaryTet out;
    const float vol = detail::sixVolume(a, b, c, d);
    if (std::fabs(vol) < 1e-20f) {
        return out; // degenerate (flat) tetrahedron
    }
    const float inv = 1.0f / vol;
    out.w[0] = detail::sixVolume(p, b, c, d) * inv;
    out.w[1] = detail::sixVolume(a, p, c, d) * inv;
    out.w[2] = detail::sixVolume(a, b, p, d) * inv;
    out.w[3] = detail::sixVolume(a, b, c, p) * inv;
    return out;
}

// True if `p` is inside or on the tetrahedron (a,b,c,d), within tolerance `eps` on the barycentric weights.
inline bool tetrahedronContains(const vec3& p, const vec3& a, const vec3& b, const vec3& c, const vec3& d,
                                float eps = 1e-5f) {
    const BaryTet bc = barycentricTetrahedron(p, a, b, c, d);
    return bc.w[0] >= -eps && bc.w[1] >= -eps && bc.w[2] >= -eps && bc.w[3] >= -eps;
}

// The closest point to `p` inside or on the tetrahedron (a,b,c,d): `p` itself if it is inside, otherwise the
// nearest point among the four triangular faces.
inline vec3 closestPointTetrahedron(const vec3& p, const vec3& a, const vec3& b, const vec3& c,
                                    const vec3& d) {
    if (tetrahedronContains(p, a, b, c, d, 0.0f)) {
        return p;
    }
    const vec3 faces[4][3] = {{a, b, c}, {a, b, d}, {a, c, d}, {b, c, d}};
    vec3 best = closestPointOnTriangle(p, faces[0][0], faces[0][1], faces[0][2]);
    float bestD = dot(p - best, p - best);
    for (int i = 1; i < 4; ++i) {
        const vec3 q = closestPointOnTriangle(p, faces[i][0], faces[i][1], faces[i][2]);
        const float dd = dot(p - q, p - q);
        if (dd < bestD) {
            bestD = dd;
            best = q;
        }
    }
    return best;
}

} // namespace maz::math
