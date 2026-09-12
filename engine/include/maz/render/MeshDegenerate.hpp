#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render DEGENERATE / SLIVER-TRIANGLE CLASSIFIER — find the badly-shaped triangles in a mesh and label each
// by DEFECT TYPE, returning their indices as a cleanup report. Bad triangles come from booleans, decimation,
// planar-cut operations, and sloppy imports; they wreck normals, lighting, physics, and simplification, so
// meshing tools flag them for removal or repair. Unlike the M537 TriangleQuality score (a single 0..1 number per
// triangle), this NAMES the problem so a repair step knows what to do: ZERO-AREA (collapsed — vertices coincident
// or collinear, must be deleted), CAP (one angle near 180° — a flat "sliver" that pokes across its neighbours,
// usually split or collapsed), and NEEDLE (one angle near 0° — a thin spike from a very short edge, usually
// collapsed along that edge). It also returns the mesh's worst (smallest/largest) angles and smallest area.
// Header-only, pure CPU; complements MeshCleanup (M541), which removes exact-duplicate and zero-area triangles.
//
// Scope note (honest): this REPORTS defects (kind + indices + extremes); it does not repair them — feed the
// indices to a collapse/split/delete pass (MeshCleanup already drops the zero-area ones). Classification order is
// zero-area, then cap, then needle, so a triangle that is both spiky and flat is reported as a cap. Thresholds
// (cap angle, needle angle) are tunable; the zero-area epsilon scales with the mesh's bounding box.
namespace maz::render {

enum class TriDefect : std::uint8_t { Ok = 0, ZeroArea = 1, Cap = 2, Needle = 3 };

struct DegenerateReport {
    std::vector<TriDefect> kind;          // per-triangle classification
    std::vector<std::uint32_t> zeroArea;  // triangle indices, by defect type
    std::vector<std::uint32_t> caps;
    std::vector<std::uint32_t> needles;
    std::size_t badCount = 0;             // zeroArea + caps + needles
    float minArea = 0.0f;                 // smallest triangle area in the mesh
    float minAngleDegrees = 180.0f;       // sharpest corner anywhere (small => a needle exists)
    float maxAngleDegrees = 0.0f;         // widest corner anywhere (near 180 => a cap exists)
};

// Classify every triangle. `capAngleDegrees` (default 150) flags caps; `needleAngleDegrees` (default 5) flags
// needles; the zero-area cut scales with the bounding box unless `areaEpsilon` >= 0 overrides it.
inline DegenerateReport analyzeDegenerate(const shapes::MeshData& mesh, float capAngleDegrees = 150.0f,
                                          float needleAngleDegrees = 5.0f, float areaEpsilon = -1.0f) {
    DegenerateReport rep;
    const std::size_t triN = mesh.indices.size() / 3;
    rep.kind.assign(triN, TriDefect::Ok);
    if (triN == 0) return rep;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };
    // Bounding-box diagonal sets the default zero-area epsilon (area ~ length², so scale by diag²).
    math::vec3 lo = pos(mesh.indices[0]), hi = lo;
    for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
        const math::vec3 p = pos(mesh.indices[i]);
        lo = math::vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
        hi = math::vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
    }
    const math::vec3 ext = hi - lo;
    const float diag = std::sqrt(math::dot(ext, ext));
    const float areaEps = areaEpsilon >= 0.0f ? areaEpsilon : std::max(diag * diag * 1e-10f, 1e-20f);

    const double rad2deg = 180.0 / 3.14159265358979323846;
    float minA = 1e30f;

    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0], ib = mesh.indices[t * 3 + 1], ic = mesh.indices[t * 3 + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
            rep.kind[t] = TriDefect::ZeroArea;
            rep.zeroArea.push_back(static_cast<std::uint32_t>(t));
            continue;
        }
        const math::vec3 A = pos(ia), B = pos(ib), C = pos(ic);
        const math::vec3 cr = math::cross(B - A, C - A);
        const float area = 0.5f * std::sqrt(math::dot(cr, cr));
        minA = std::min(minA, area);

        if (area <= areaEps) {
            rep.kind[t] = TriDefect::ZeroArea;
            rep.zeroArea.push_back(static_cast<std::uint32_t>(t));
            continue;
        }
        // Edge lengths (opposite each vertex) and interior angles via the law of cosines.
        const float la = std::sqrt(math::dot(C - B, C - B)); // opposite A
        const float lb = std::sqrt(math::dot(A - C, A - C)); // opposite B
        const float lc = std::sqrt(math::dot(B - A, B - A)); // opposite C
        auto angle = [&](float opp, float s1, float s2) {
            float c = (s1 * s1 + s2 * s2 - opp * opp) / (2.0f * s1 * s2);
            c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
            return static_cast<float>(std::acos(static_cast<double>(c)) * rad2deg);
        };
        const float angA = angle(la, lb, lc);
        const float angB = angle(lb, la, lc);
        const float angC = angle(lc, la, lb);
        const float maxAng = std::max(angA, std::max(angB, angC));
        const float minAng = std::min(angA, std::min(angB, angC));
        if (maxAng > rep.maxAngleDegrees) rep.maxAngleDegrees = maxAng;
        if (minAng < rep.minAngleDegrees) rep.minAngleDegrees = minAng;

        if (maxAng >= capAngleDegrees) {
            rep.kind[t] = TriDefect::Cap;
            rep.caps.push_back(static_cast<std::uint32_t>(t));
        } else if (minAng <= needleAngleDegrees) {
            rep.kind[t] = TriDefect::Needle;
            rep.needles.push_back(static_cast<std::uint32_t>(t));
        }
    }

    rep.minArea = (minA < 1e30f) ? minA : 0.0f;
    rep.badCount = rep.zeroArea.size() + rep.caps.size() + rep.needles.size();
    return rep;
}

} // namespace maz::render
