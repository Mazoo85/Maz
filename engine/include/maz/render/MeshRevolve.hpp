#pragma once

#include "maz/math/Math.hpp"      // math::vec3
#include "maz/render/MeshTools.hpp" // computeNormals
#include "maz/render/Shapes.hpp"    // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render REVOLVE / LATHE — spin a 2D outline (a "profile") around the vertical axis to build a solid of
// revolution. Give it the silhouette of a vase, a bottle, a wine glass, a wheel, a chess pawn, a lamp base, a
// bowl — a list of (radius-from-the-axis, height) points tracing the side view — and it sweeps that outline all
// the way around, stitching a smooth surface. This is the single most productive way to model any round object:
// it's Blender's "Spin", a wood-lathe in software, Godot's CSGPolygon3D in Spin mode. The profile is a polyline
// in the RADIUS-HEIGHT plane; the sweep runs around the +Y axis in `segments` angular steps. A partial sweep
// (`sweepRadians` < 2π) makes an open fan / arc wedge; a full turn makes a closed round body. Reuses the
// engine's area-weighted `computeNormals` so the result shades smoothly out of the box. Header-only, pure CPU,
// deterministic — no GPU needed to build or verify the geometry.
//
// Scope note (honest): this builds only the swept SIDE surface. It does NOT add end caps — a profile that stops
// short of the axis leaves the top/bottom open (a tube), exactly like a lathe with no facing cut; to close an
// end, run the profile down to radius 0 (a point on the axis, which forms a natural cone tip) or cap it later.
// Profile points that sit on the axis (radius 0) collapse to a single pole vertex per ring, so the degenerate
// zero-area triangles they would make are skipped, giving clean cone tips. Winding is outward (normals point
// away from the axis) for a positive-radius profile. Needs >= 2 profile points and >= 3 segments.
namespace maz::render {

// One point of the lathe outline: how far from the spin axis, and how high up it.
struct ProfilePoint {
    float radius = 0.0f; // distance from the +Y axis (>= 0)
    float height = 0.0f; // position along the +Y axis
};

// Sweep `profile` around the +Y axis into a mesh. `segments` = angular divisions of the sweep; `sweepRadians`
// defaults to a full turn (2π). Empty/too-small inputs yield an empty mesh.
inline shapes::MeshData revolveProfile(const std::vector<ProfilePoint>& profile, int segments,
                                       float sweepRadians = 6.28318530717958647692f) {
    shapes::MeshData out;
    const std::size_t P = profile.size();
    if (P < 2 || segments < 3) return out;

    const std::size_t rings = static_cast<std::size_t>(segments) + 1; // +1 seam ring (last coincides on a full turn)

    // --- Positions: one ring of P vertices per angular step. ---
    std::vector<math::vec3> positions;
    positions.reserve(rings * P);
    for (std::size_t j = 0; j < rings; ++j) {
        const float t = static_cast<float>(j) / static_cast<float>(segments);
        const float ang = sweepRadians * t;
        const float ca = std::cos(ang), sa = std::sin(ang);
        for (std::size_t i = 0; i < P; ++i) {
            const float r = profile[i].radius;
            positions.push_back(math::vec3(r * ca, profile[i].height, r * sa));
        }
    }

    // --- Faces: bridge consecutive rings with quads (two triangles), skipping degenerate ones at axis poles. ---
    auto degenerate = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        const math::vec3& pa = positions[a];
        const math::vec3& pb = positions[b];
        const math::vec3& pc = positions[c];
        const math::vec3 cr = math::cross(pb - pa, pc - pa);
        return (cr.x * cr.x + cr.y * cr.y + cr.z * cr.z) < 1e-20f;
    };
    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(segments) * (P - 1) * 6);
    auto emit = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        if (!degenerate(a, b, c)) { indices.push_back(a); indices.push_back(b); indices.push_back(c); }
    };
    for (std::size_t j = 0; j < static_cast<std::size_t>(segments); ++j) {
        for (std::size_t i = 0; i + 1 < P; ++i) {
            const std::uint32_t v00 = static_cast<std::uint32_t>(j * P + i);
            const std::uint32_t v01 = static_cast<std::uint32_t>(j * P + i + 1);
            const std::uint32_t v10 = static_cast<std::uint32_t>((j + 1) * P + i);
            const std::uint32_t v11 = static_cast<std::uint32_t>((j + 1) * P + i + 1);
            emit(v00, v01, v10); // outward winding for a positive-radius profile
            emit(v01, v11, v10);
        }
    }

    // --- Smooth normals + assemble vertices (white, UV = [sweep fraction, profile fraction]). ---
    const std::vector<math::vec3> normals = computeNormals(positions, indices);
    out.vertices.reserve(positions.size());
    for (std::size_t j = 0; j < rings; ++j) {
        const float u = static_cast<float>(j) / static_cast<float>(segments);
        for (std::size_t i = 0; i < P; ++i) {
            const std::size_t k = j * P + i;
            MeshVertex v{};
            v.px = positions[k].x; v.py = positions[k].y; v.pz = positions[k].z;
            v.nx = normals[k].x; v.ny = normals[k].y; v.nz = normals[k].z;
            v.r = v.g = v.b = 1.0f;
            v.u = u;
            v.v = static_cast<float>(i) / static_cast<float>(P - 1);
            out.vertices.push_back(v);
        }
    }
    out.indices = std::move(indices);
    return out;
}

} // namespace maz::render
