#pragma once

#include "maz/math/Math.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace maz::render {

// Mesh post-processing — the "fill in the vertex attributes an importer or generator left blank" step,
// Godot's SurfaceTool.generate_normals() / generate_tangents(). A raw mesh is often just positions +
// indices (+ maybe UVs): a heightfield you built procedurally, a decimated collision hull, a glTF that
// shipped without a NORMAL/TANGENT stream. Lighting needs a per-vertex NORMAL, and normal mapping needs
// a per-vertex TANGENT frame; deriving them from the geometry is a standard, well-defined computation.
// Maz could build primitive shapes (which bake their own normals) and load glTF (which may carry them),
// but had no way to (re)generate these for arbitrary geometry. Pure vector math, header-only,
// deterministic — it unit-tests exactly (a flat mesh yields the plane normal; a shared ridge yields the
// averaged normal) and drives a golden.
//
// computeNormals uses AREA-WEIGHTED face accumulation: each triangle adds its un-normalized cross
// product (whose magnitude is twice the triangle area) to its three vertices, so larger faces pull a
// shared vertex more — the same default SurfaceTool uses, and it gives smooth results on curved meshes
// while degenerate (zero-area) triangles contribute nothing. computeTangents uses Lengyel's method
// (the one Godot/most engines use): accumulate per-triangle tangent/bitangent from the UV gradient, then
// Gram-Schmidt-orthonormalize against the normal and store handedness in .w so the shader can rebuild the
// bitangent as cross(normal, tangent.xyz) * tangent.w.
//
// Scope note (honest): these operate on a single indexed triangle stream with fully shared vertices
// (smoothing groups are "every face that shares a vertex index"). They do not split vertices along hard
// edges / UV seams, weld a soft threshold, or triangulate polygons — a full SurfaceTool with
// index/dedup/seam handling remains a follow-up.

namespace detail {
inline float length3(const math::vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline math::vec3 safeNormalize3(const math::vec3& v) {
    const float len = length3(v);
    return len > 1e-12f ? math::vec3(v.x / len, v.y / len, v.z / len) : math::vec3(0.0f, 0.0f, 0.0f);
}
} // namespace detail

// Area-weighted smooth vertex normals for an indexed triangle mesh. `indices` is a flat list of triples;
// any trailing partial triangle is ignored. The returned vector has one unit normal per position (a
// vertex touched by no valid triangle gets a zero normal).
inline std::vector<math::vec3> computeNormals(const std::vector<math::vec3>& positions,
                                              const std::vector<std::uint32_t>& indices) {
    std::vector<math::vec3> normals(positions.size(), math::vec3(0.0f, 0.0f, 0.0f));
    const std::size_t triCount = indices.size() / 3;
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t ia = indices[t * 3 + 0];
        const std::uint32_t ib = indices[t * 3 + 1];
        const std::uint32_t ic = indices[t * 3 + 2];
        if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
            continue;
        }
        const math::vec3& a = positions[ia];
        const math::vec3& b = positions[ib];
        const math::vec3& c = positions[ic];
        // Un-normalized cross product = 2 * area * face-normal (area weighting falls out for free).
        const math::vec3 faceN = math::cross(b - a, c - a);
        normals[ia] += faceN;
        normals[ib] += faceN;
        normals[ic] += faceN;
    }
    for (auto& n : normals) {
        n = detail::safeNormalize3(n);
    }
    return normals;
}

// Per-vertex tangents (xyz = tangent direction, w = ±1 handedness) via Lengyel's method. Requires
// matching-length `positions`, `normals`, and `uvs`. A vertex with no usable UV gradient falls back to
// an arbitrary tangent orthogonal to its normal, with handedness +1.
inline std::vector<math::vec4> computeTangents(const std::vector<math::vec3>& positions,
                                               const std::vector<math::vec3>& normals,
                                               const std::vector<math::vec2>& uvs,
                                               const std::vector<std::uint32_t>& indices) {
    const std::size_t n = positions.size();
    std::vector<math::vec3> tan1(n, math::vec3(0.0f, 0.0f, 0.0f));
    std::vector<math::vec3> tan2(n, math::vec3(0.0f, 0.0f, 0.0f));

    const std::size_t triCount = indices.size() / 3;
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t i0 = indices[t * 3 + 0];
        const std::uint32_t i1 = indices[t * 3 + 1];
        const std::uint32_t i2 = indices[t * 3 + 2];
        if (i0 >= n || i1 >= n || i2 >= n || i0 >= uvs.size() || i1 >= uvs.size() || i2 >= uvs.size()) {
            continue;
        }
        const math::vec3& p0 = positions[i0];
        const math::vec3& p1 = positions[i1];
        const math::vec3& p2 = positions[i2];
        const math::vec2& w0 = uvs[i0];
        const math::vec2& w1 = uvs[i1];
        const math::vec2& w2 = uvs[i2];

        const math::vec3 e1 = p1 - p0;
        const math::vec3 e2 = p2 - p0;
        const float s1 = w1.x - w0.x, t1 = w1.y - w0.y;
        const float s2 = w2.x - w0.x, t2 = w2.y - w0.y;
        const float denom = s1 * t2 - s2 * t1;
        if (std::fabs(denom) < 1e-12f) {
            continue; // degenerate UVs — no gradient
        }
        const float r = 1.0f / denom;
        const math::vec3 sdir((t2 * e1.x - t1 * e2.x) * r, (t2 * e1.y - t1 * e2.y) * r,
                              (t2 * e1.z - t1 * e2.z) * r);
        const math::vec3 tdir((s1 * e2.x - s2 * e1.x) * r, (s1 * e2.y - s2 * e1.y) * r,
                              (s1 * e2.z - s2 * e1.z) * r);
        tan1[i0] += sdir;
        tan1[i1] += sdir;
        tan1[i2] += sdir;
        tan2[i0] += tdir;
        tan2[i1] += tdir;
        tan2[i2] += tdir;
    }

    std::vector<math::vec4> tangents(n, math::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec3& nrm = normals[i];
        math::vec3 t = tan1[i];
        // Gram-Schmidt: t' = normalize(t - n * dot(n, t)).
        const float ndt = math::dot(nrm, t);
        t = math::vec3(t.x - nrm.x * ndt, t.y - nrm.y * ndt, t.z - nrm.z * ndt);
        math::vec3 tn = detail::safeNormalize3(t);
        if (detail::length3(tn) < 1e-6f) {
            // No usable gradient: pick any axis orthogonal to the normal.
            const math::vec3 ref = std::fabs(nrm.z) < 0.9f ? math::vec3(0.0f, 0.0f, 1.0f)
                                                           : math::vec3(1.0f, 0.0f, 0.0f);
            tn = detail::safeNormalize3(math::cross(ref, nrm));
        }
        const float handed = (math::dot(math::cross(nrm, tn), tan2[i]) < 0.0f) ? -1.0f : 1.0f;
        tangents[i] = math::vec4(tn.x, tn.y, tn.z, handed);
    }
    return tangents;
}

} // namespace maz::render
