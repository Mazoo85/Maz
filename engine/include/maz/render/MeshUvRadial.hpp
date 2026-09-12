#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

// maz::render SPHERICAL & CYLINDRICAL UV PROJECTION — the wraparound auto-unwraps for round objects, completing
// the projection-UV family alongside planar/box (M551). SPHERICAL maps each vertex by its direction from a
// centre to longitude (u, the angle around) and latitude (v, top-to-bottom) — the equirectangular / lat-long
// layout a planet, an eyeball, a ball, or a skydome wants (world maps are stored exactly this way). CYLINDRICAL
// maps the angle around an axis to u and the height along it to v — what a bottle label, a tree trunk, a pipe,
// or a tin can wants. Both are the one-click UVs a round mesh needs before it can wear a texture, with no manual
// seam work. UVs are written onto the existing vertices (topology unchanged). Reuses only vec3 from maz::math.
// Header-only, deterministic.
//
// Scope note (honest): both have the inherent projection artefacts — a single wrap SEAM where u jumps from ~1
// back to 0 (the mesh should be split there, or the texture set to wrap), spherical PINCHING at the two poles
// (all longitudes converge), and cylindrical stretch on caps that face along the axis. These are properties of
// the projection, not bugs; an LSCM/angle-based unwrap is the distortion-free follow-up.
namespace maz::render {

// Spherical (lat-long / equirectangular) projection about `center`: u = longitude in [0,1) around +Y, v =
// latitude in [0,1] from the +Y pole (v=0) to the −Y pole (v=1). `uScale`/`vScale` tile the result.
inline shapes::MeshData sphericalUv(const shapes::MeshData& mesh, const math::vec3& center = math::vec3(0, 0, 0),
                                    float uScale = 1.0f, float vScale = 1.0f) {
    shapes::MeshData out = mesh;
    const double pi = 3.14159265358979323846;
    for (MeshVertex& vert : out.vertices) {
        const double dx = static_cast<double>(vert.px - center.x);
        const double dy = static_cast<double>(vert.py - center.y);
        const double dz = static_cast<double>(vert.pz - center.z);
        const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1e-20) { vert.u = 0.5f * uScale; vert.v = 0.5f * vScale; continue; }
        const double ny = dy / len;
        const double u = 0.5 + std::atan2(dz, dx) / (2.0 * pi);         // longitude around +Y
        const double clampedY = ny < -1.0 ? -1.0 : (ny > 1.0 ? 1.0 : ny);
        const double v = 0.5 - std::asin(clampedY) / pi;               // +Y pole -> 0, -Y pole -> 1
        vert.u = static_cast<float>(u) * uScale;
        vert.v = static_cast<float>(v) * vScale;
    }
    return out;
}

// Cylindrical projection about `center` along `axis` (0=X,1=Y,2=Z): u = angle in [0,1) around the axis, v =
// signed distance along the axis times `vScale` (plus `vOffset`). Topology unchanged.
inline shapes::MeshData cylindricalUv(const shapes::MeshData& mesh, int axis = 1,
                                      const math::vec3& center = math::vec3(0, 0, 0),
                                      float uScale = 1.0f, float vScale = 1.0f, float vOffset = 0.0f) {
    shapes::MeshData out = mesh;
    if (axis < 0 || axis > 2) return out;
    const double pi = 3.14159265358979323846;
    // Two coordinates perpendicular to the axis (ascending index) span the angle; the axis coordinate is height.
    const int a0 = axis == 0 ? 1 : 0;
    const int a1 = axis == 2 ? 1 : 2;
    const float c[3] = {center.x, center.y, center.z};
    for (MeshVertex& vert : out.vertices) {
        const float p[3] = {vert.px, vert.py, vert.pz};
        const double angle = std::atan2(static_cast<double>(p[a1] - c[a1]), static_cast<double>(p[a0] - c[a0]));
        const double u = 0.5 + angle / (2.0 * pi);
        vert.u = static_cast<float>(u) * uScale;
        vert.v = (p[axis] - c[axis]) * vScale + vOffset;
    }
    return out;
}

} // namespace maz::render
