#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

// maz::render BEND — curl a straight mesh around an arc: a bar wraps onto a circle of radius `radius`, so a plank
// becomes an archway, a straight pipe becomes an elbow, a flat strip becomes a curled ribbon or a barrel stave.
// This is Blender's "Simple Deform → Bend" and the third classic deformer alongside twist (M581) and taper (M582).
// You pick the `alongAxis` the bar extends down and the `upAxis` it bends toward; the third axis rides through
// unchanged. A vertex's coordinate along the bar becomes an ANGLE (theta = alongCoord / radius) swept around a
// bend centre sitting `radius` up the up-axis, and its up-coordinate becomes how far it sits from that centre —
// so the whole length curls smoothly. Positions are exact; the normal's in-plane components rotate with the arc.
// Header-only, pure CPU, deterministic.
//
// Scope note (honest): the bend is exact for positions — a vertex's distance from the bend centre stays
// `radius − up` and its swept angle is `along / radius`, both unit-tested. Normals get their (along, up)
// components rotated by the local arc angle, which is right for the bend's rotation but ignores the slight
// scale the curl adds; re-run `computeNormals` for pixel-accurate shading on a tight bend. Smoothness of the arc
// is limited by how many segments the bar has along its length (a 2-segment bar bends into a single kink;
// subdivide first for a smooth curve). A smaller `radius` curls tighter (a full circle closes when the bar length
// reaches 2·pi·radius); `radius` may be negative to bend the other way. The bar should straddle alongCoord = 0
// for a symmetric bend (that column is the hinge that stays put).
namespace maz::render {

namespace detail {
inline float bendGet(const MeshVertex& v, int axis) { return axis == 0 ? v.px : (axis == 1 ? v.py : v.pz); }
inline void bendSetPos(MeshVertex& v, int axis, float val) {
    if (axis == 0) v.px = val; else if (axis == 1) v.py = val; else v.pz = val;
}
inline float bendGetN(const MeshVertex& v, int axis) { return axis == 0 ? v.nx : (axis == 1 ? v.ny : v.nz); }
inline void bendSetN(MeshVertex& v, int axis, float val) {
    if (axis == 0) v.nx = val; else if (axis == 1) v.ny = val; else v.nz = val;
}
} // namespace detail

// Bend `mesh` around an arc of `radius`: `alongAxis` is the bar's length, `upAxis` the direction it curls toward
// (must differ from alongAxis). The bend centre sits `radius` along the up-axis from the origin. Returns a bent
// copy.
inline shapes::MeshData bendMesh(const shapes::MeshData& mesh, int alongAxis, int upAxis, float radius) {
    shapes::MeshData out = mesh;
    const int a = alongAxis < 0 ? 0 : (alongAxis > 2 ? 2 : alongAxis);
    int u = upAxis < 0 ? 0 : (upAxis > 2 ? 2 : upAxis);
    if (u == a) u = (a + 1) % 3; // must be distinct
    if (std::fabs(radius) < 1e-8f) return out; // no arc to bend around

    for (MeshVertex& v : out.vertices) {
        const float along = detail::bendGet(v, a);
        const float up = detail::bendGet(v, u);
        const float angle = along / radius;
        const float r = radius - up; // distance from the bend centre depends only on the up-coordinate
        const float s = std::sin(angle), co = std::cos(angle);

        detail::bendSetPos(v, a, s * r);
        detail::bendSetPos(v, u, radius - co * r);

        // Rotate the normal's (along, up) components by the local arc angle.
        const float nA = detail::bendGetN(v, a), nU = detail::bendGetN(v, u);
        float nA2 = nA * co - nU * s, nU2 = nA * s + nU * co;
        const float nOther = detail::bendGetN(v, 3 - a - u);
        const float nlen = std::sqrt(nA2 * nA2 + nU2 * nU2 + nOther * nOther);
        if (nlen > 1e-12f) { nA2 /= nlen; nU2 /= nlen; }
        detail::bendSetN(v, a, nA2);
        detail::bendSetN(v, u, nU2);
        detail::bendSetN(v, 3 - a - u, nlen > 1e-12f ? nOther / nlen : nOther);
    }
    return out;
}

} // namespace maz::render
