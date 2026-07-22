#pragma once

#include "maz/math/Math.hpp"        // math::vec2, vec3, cross, dot
#include "maz/render/MeshTools.hpp"  // computeNormals
#include "maz/render/Shapes.hpp"     // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render SWEEP ALONG A PATH / LOFT — take a flat 2D cross-section (a "profile") and push it down a 3D path,
// leaving a solid tube of that shape behind it. Give it a circle and a curvy path and you get a pipe, cable, rope,
// wire, garden hose, or tentacle; give it a rectangle and you get a rail, a moulding, a road ribbon, a fence beam;
// give it a star or an L and you get an extruded girder or trim. This is Blender's "Curve → bevel/taper" sweep and
// Godot's CSGPolygon3D in Path mode — the standard way to build anything long and bendy that follows a line.
//
// The tricky part of sweeping is keeping the cross-section from spinning wildly as the path curves. This uses a
// ROTATION-MINIMIZING FRAME (parallel transport): the profile's orientation is carried forward from one path point
// to the next by the smallest rotation that follows the bend, so a pipe doesn't twist along its length. Reuses the
// engine's area-weighted `computeNormals` for smooth shading. Header-only, deterministic, headless.
//
// Scope note (honest): this builds the swept SIDE surface only — the two ends are left OPEN (like a cut pipe); cap
// them separately if you need a closed solid. `closedProfile` (default true) controls whether the cross-section is
// a closed ring (a tube) or an open strip (a ribbon). The path is a polyline of >= 2 points; a profile needs >= 2
// points. Consecutive duplicate path points (zero-length segments) are tolerated (they reuse the previous frame).
namespace maz::render {

// Sweep `profile` (a 2D cross-section in its own local plane) along `path` (a 3D polyline). Returns the tube mesh.
inline shapes::MeshData sweepProfile(const std::vector<math::vec2>& profile,
                                     const std::vector<math::vec3>& path, bool closedProfile = true) {
    shapes::MeshData out;
    const std::size_t P = profile.size();
    const std::size_t N = path.size();
    if (P < 2 || N < 2) return out;

    // --- Per-path-point tangents (finite differences; one-sided at the ends). ---
    auto safeNorm = [](const math::vec3& v) {
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return len > 1e-12f ? math::vec3(v.x / len, v.y / len, v.z / len) : math::vec3(0, 0, 1);
    };
    std::vector<math::vec3> tan(N);
    for (std::size_t i = 0; i < N; ++i) {
        math::vec3 d;
        if (i == 0) d = path[1] - path[0];
        else if (i + 1 == N) d = path[N - 1] - path[N - 2];
        else d = path[i + 1] - path[i - 1];
        tan[i] = safeNorm(d);
    }

    // --- Rotation-minimizing frames: seed a normal orthogonal to the first tangent, then parallel-transport it. ---
    auto rotate = [](const math::vec3& v, const math::vec3& axis, float c, float s) { // Rodrigues
        const math::vec3 cr = math::cross(axis, v);
        const float dt = math::dot(axis, v);
        return math::vec3(v.x * c + cr.x * s + axis.x * dt * (1.0f - c),
                          v.y * c + cr.y * s + axis.y * dt * (1.0f - c),
                          v.z * c + cr.z * s + axis.z * dt * (1.0f - c));
    };
    std::vector<math::vec3> nrm(N), bin(N);
    {
        // Seed normal: cross tangent with whichever world axis it is least aligned to.
        const math::vec3 t0 = tan[0];
        const math::vec3 ref = (std::fabs(t0.y) < 0.9f) ? math::vec3(0, 1, 0) : math::vec3(1, 0, 0);
        nrm[0] = safeNorm(math::cross(t0, ref));
        bin[0] = math::cross(t0, nrm[0]);
    }
    for (std::size_t i = 1; i < N; ++i) {
        const math::vec3 t0 = tan[i - 1], t1 = tan[i];
        const math::vec3 axis = math::cross(t0, t1);
        const float sinA = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        if (sinA < 1e-8f) {                         // (nearly) parallel: carry the frame unchanged
            nrm[i] = nrm[i - 1];
        } else {
            const math::vec3 a = math::vec3(axis.x / sinA, axis.y / sinA, axis.z / sinA);
            float cosA = math::dot(t0, t1);
            cosA = cosA < -1.0f ? -1.0f : (cosA > 1.0f ? 1.0f : cosA);
            nrm[i] = safeNorm(rotate(nrm[i - 1], a, cosA, sinA));
        }
        bin[i] = math::cross(t1, nrm[i]);
    }

    // --- Place a ring of profile points at each path point, oriented by that point's frame. ---
    std::vector<math::vec3> positions;
    positions.reserve(N * P);
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = 0; j < P; ++j) {
            const float u = profile[j].x, v = profile[j].y;
            positions.push_back(math::vec3(path[i].x + nrm[i].x * u + bin[i].x * v,
                                           path[i].y + nrm[i].y * u + bin[i].y * v,
                                           path[i].z + nrm[i].z * u + bin[i].z * v));
        }
    }

    // --- Bridge consecutive rings with quads (two triangles per profile edge). ---
    const std::size_t edges = closedProfile ? P : P - 1;
    std::vector<std::uint32_t> indices;
    indices.reserve((N - 1) * edges * 6);
    for (std::size_t i = 0; i + 1 < N; ++i) {
        for (std::size_t e = 0; e < edges; ++e) {
            const std::size_t j0 = e, j1 = (e + 1) % P;
            const std::uint32_t a = static_cast<std::uint32_t>(i * P + j0);
            const std::uint32_t b = static_cast<std::uint32_t>(i * P + j1);
            const std::uint32_t c = static_cast<std::uint32_t>((i + 1) * P + j0);
            const std::uint32_t d = static_cast<std::uint32_t>((i + 1) * P + j1);
            indices.push_back(a); indices.push_back(c); indices.push_back(d);
            indices.push_back(a); indices.push_back(d); indices.push_back(b);
        }
    }

    // --- Smooth normals + assemble vertices (white, UV = [along path, around profile]). ---
    const std::vector<math::vec3> normals = computeNormals(positions, indices);
    out.vertices.reserve(positions.size());
    for (std::size_t i = 0; i < N; ++i) {
        const float uu = static_cast<float>(i) / static_cast<float>(N - 1);
        for (std::size_t j = 0; j < P; ++j) {
            const std::size_t k = i * P + j;
            MeshVertex mv{};
            mv.px = positions[k].x; mv.py = positions[k].y; mv.pz = positions[k].z;
            mv.nx = normals[k].x; mv.ny = normals[k].y; mv.nz = normals[k].z;
            mv.r = mv.g = mv.b = 1.0f;
            mv.u = uu;
            mv.v = static_cast<float>(j) / static_cast<float>(P);
            out.vertices.push_back(mv);
        }
    }
    out.indices = std::move(indices);
    return out;
}

// Convenience: sweep a regular `sides`-gon circle of `radius` along `path` — the common "pipe / cable" case.
inline shapes::MeshData buildTube(const std::vector<math::vec3>& path, float radius, int sides) {
    if (sides < 3) return shapes::MeshData{};
    std::vector<math::vec2> circle;
    circle.reserve(static_cast<std::size_t>(sides));
    const float twoPi = 6.28318530717958647692f;
    for (int j = 0; j < sides; ++j) {
        const float a = twoPi * static_cast<float>(j) / static_cast<float>(sides);
        circle.push_back(math::vec2(std::cos(a) * radius, std::sin(a) * radius));
    }
    return sweepProfile(circle, path, /*closedProfile=*/true);
}

} // namespace maz::render
