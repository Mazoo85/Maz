#pragma once

#include "maz/math/Math.hpp"       // math::vec3, dot
#include "maz/render/MeshWeld.hpp" // detail::cellHash (spatial-hash key)
#include "maz/render/Shapes.hpp"   // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render MESH SYMMETRY-PLANE DETECTION — decide whether a mesh is MIRROR-SYMMETRIC and across which plane.
// Most game props and characters are built symmetric (a face, a car, a sword), and knowing the symmetry plane
// unlocks a lot: symmetric modelling/sculpt tools that mirror edits, half-mesh authoring then reflect (see the
// M547 mirror tool), UV/texture mirroring, "is this the left or right variant?" checks, and pivot/alignment
// fixes for importers that landed a model off-axis. This tests the three axis-aligned candidate planes through
// the mesh's centroid (normal along X, Y, or Z), reflects every vertex across each, and scores the plane by the
// fraction of vertices that land on an existing vertex within tolerance — then reports the best plane and each
// axis' score. A spatial hash makes the correspondence lookup near-linear. Header-only, pure CPU.
//
// Scope note (honest): only the three AXIS-ALIGNED planes through the centroid are tested — a model symmetric
// about a tilted or off-centre plane reads as non-symmetric here (fit an OBB first via math::FitObb and test in
// its frame). Scoring is by VERTEX correspondence, so an asymmetric tessellation of a symmetric SHAPE can score
// below 1 even though the surface is symmetric; raise `tolerance` or resample if that bites. The default
// tolerance scales with the bounding box (1e-3 of its diagonal).
namespace maz::render {

enum class SymmetryAxis : std::uint8_t { X = 0, Y = 1, Z = 2 };

struct SymmetryPlane {
    SymmetryAxis axis = SymmetryAxis::X; // the plane's normal axis
    float position = 0.0f;               // plane offset along that axis (the centroid coordinate)
    float score = 0.0f;                  // fraction of vertices with a mirror partner within tolerance (0..1)
    bool symmetric = false;              // score >= the acceptance threshold
};

struct SymmetryReport {
    SymmetryPlane best;         // the highest-scoring plane
    SymmetryPlane perAxis[3];   // X, Y, Z candidate planes
    bool anySymmetric = false;  // true if the best plane passed the threshold
};

// Detect axis-aligned mirror symmetry of `mesh`. `tolerance` < 0 auto-scales to 1e-3 of the bbox diagonal;
// `acceptScore` is the fraction of matched vertices required to call an axis symmetric.
inline SymmetryReport detectSymmetryPlanes(const shapes::MeshData& mesh, float tolerance = -1.0f,
                                           float acceptScore = 0.98f) {
    SymmetryReport rep;
    const std::size_t n = mesh.vertices.size();
    if (n == 0) return rep;

    std::vector<math::vec3> p(n);
    math::vec3 lo, hi, sum(0, 0, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const MeshVertex& v = mesh.vertices[i];
        p[i] = math::vec3(v.px, v.py, v.pz);
        sum += p[i];
        if (i == 0) { lo = hi = p[0]; }
        else {
            lo = math::vec3(std::min(lo.x, p[i].x), std::min(lo.y, p[i].y), std::min(lo.z, p[i].z));
            hi = math::vec3(std::max(hi.x, p[i].x), std::max(hi.y, p[i].y), std::max(hi.z, p[i].z));
        }
    }
    const math::vec3 centroid = sum * (1.0f / static_cast<float>(n));
    const math::vec3 ext = hi - lo;
    const float bboxDiag = std::sqrt(math::dot(ext, ext));
    const float tol = tolerance >= 0.0f ? tolerance : std::max(bboxDiag * 1e-3f, 1e-6f);
    const float cell = tol > 0.0f ? tol : 1e-6f;
    const float tol2 = tol * tol;

    // Spatial hash of the original vertices for fast "is there a vertex near here?" queries.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> grid;
    grid.reserve(n);
    auto cellOf = [&](float v) { return static_cast<long>(std::floor(static_cast<double>(v) / cell)); };
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(n); ++i) {
        grid[detail::cellHash(cellOf(p[i].x), cellOf(p[i].y), cellOf(p[i].z))].push_back(i);
    }
    auto hasVertexNear = [&](const math::vec3& q) {
        const long cx = cellOf(q.x), cy = cellOf(q.y), cz = cellOf(q.z);
        for (long dz = -1; dz <= 1; ++dz)
            for (long dy = -1; dy <= 1; ++dy)
                for (long dx = -1; dx <= 1; ++dx) {
                    const auto it = grid.find(detail::cellHash(cx + dx, cy + dy, cz + dz));
                    if (it == grid.end()) continue;
                    for (std::uint32_t vi : it->second) {
                        const math::vec3 d = p[vi] - q;
                        if (math::dot(d, d) <= tol2) return true;
                    }
                }
        return false;
    };

    auto scoreAxis = [&](int a) {
        std::size_t matched = 0;
        for (std::size_t i = 0; i < n; ++i) {
            math::vec3 m = p[i];
            if (a == 0) m.x = 2.0f * centroid.x - p[i].x;
            else if (a == 1) m.y = 2.0f * centroid.y - p[i].y;
            else m.z = 2.0f * centroid.z - p[i].z;
            if (hasVertexNear(m)) ++matched;
        }
        return static_cast<float>(matched) / static_cast<float>(n);
    };

    const float cpos[3] = {centroid.x, centroid.y, centroid.z};
    int bestA = 0;
    for (int a = 0; a < 3; ++a) {
        SymmetryPlane pl;
        pl.axis = static_cast<SymmetryAxis>(a);
        pl.position = cpos[a];
        pl.score = scoreAxis(a);
        pl.symmetric = pl.score >= acceptScore;
        rep.perAxis[a] = pl;
        if (pl.score > rep.perAxis[bestA].score) bestA = a;
    }
    rep.best = rep.perAxis[bestA];
    rep.anySymmetric = rep.best.symmetric;
    return rep;
}

} // namespace maz::render
