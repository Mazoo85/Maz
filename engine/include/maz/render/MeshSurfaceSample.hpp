#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render AREA-WEIGHTED SURFACE POINT SAMPLING — scatter N points uniformly across a mesh's SURFACE, so
// every unit of area is equally likely to be picked (a triangle twice as big gets twice as many points). This
// is the seed for scattering grass, rocks, foliage, or debris over terrain and props; for generating a point
// cloud from a mesh; for placing decals or spawn points; and as the input to blue-noise / Poisson-disk
// relaxation. Each point carries its world position, the face normal there (to orient what you place), and the
// triangle it landed on. Picking is exact: a per-triangle area CDF chooses the face (binary search on a uniform
// draw), then a standard barycentric warp (u=1−√r1, …) places the point uniformly inside it. Deterministic — the
// same `seed` gives the same points every run. Reuses only vec3 from maz::math; self-contained RNG. Header-only.
//
// Scope note (honest): uniform over AREA, not blue-noise — points can clump; feed these into a relaxation pass
// if you need even spacing. Degenerate (zero-area) triangles are never chosen. The normal is the geometric FACE
// normal (winding-derived), not the interpolated smooth normal.
namespace maz::render {

struct SurfacePoint {
    math::vec3 position{0, 0, 0};
    math::vec3 normal{0, 0, 0};
    std::uint32_t triangle = 0; // index of the triangle it was sampled from
};

// Sample `count` points area-uniformly over `mesh`. Empty/degenerate meshes yield an empty result.
inline std::vector<SurfacePoint> sampleSurfacePoints(const shapes::MeshData& mesh, std::size_t count,
                                                     std::uint64_t seed = 0) {
    std::vector<SurfacePoint> out;
    const std::size_t triN = mesh.indices.size() / 3;
    if (triN == 0 || mesh.vertices.empty() || count == 0) return out;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };

    // Per-triangle cumulative area (CDF). Degenerate triangles contribute zero, so they are never selected.
    std::vector<double> cdf(triN, 0.0);
    std::vector<math::vec3> faceN(triN, math::vec3(0, 0, 0));
    double total = 0.0;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        double a = 0.0;
        if (ia < mesh.vertices.size() && ib < mesh.vertices.size() && ic < mesh.vertices.size()) {
            const math::vec3 cr = math::cross(pos(ib) - pos(ia), pos(ic) - pos(ia));
            const double len = std::sqrt(static_cast<double>(math::dot(cr, cr)));
            a = 0.5 * len;
            if (len > 1e-20) faceN[t] = math::vec3(cr.x / static_cast<float>(len), cr.y / static_cast<float>(len),
                                                   cr.z / static_cast<float>(len));
        }
        total += a;
        cdf[t] = total;
    }
    if (total <= 1e-20) return out;

    // Self-contained deterministic RNG (LCG), returning a float in [0,1).
    std::uint64_t rng = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    auto nextf = [&rng]() {
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::uint32_t x = static_cast<std::uint32_t>(rng >> 40); // top 24 bits
        return static_cast<float>(x) * (1.0f / 16777216.0f);
    };

    out.reserve(count);
    for (std::size_t s = 0; s < count; ++s) {
        // Pick a triangle by area (binary search into the CDF).
        const double r = static_cast<double>(nextf()) * total;
        std::size_t t = static_cast<std::size_t>(std::upper_bound(cdf.begin(), cdf.end(), r) - cdf.begin());
        if (t >= triN) t = triN - 1;

        const math::vec3 a = pos(mesh.indices[t * 3 + 0]);
        const math::vec3 b = pos(mesh.indices[t * 3 + 1]);
        const math::vec3 c = pos(mesh.indices[t * 3 + 2]);
        // Uniform barycentric sample.
        const float r1 = nextf(), r2 = nextf();
        const float su = std::sqrt(r1);
        const float b0 = 1.0f - su, b1 = su * (1.0f - r2), b2 = su * r2;

        SurfacePoint sp;
        sp.position = math::vec3(a.x * b0 + b.x * b1 + c.x * b2, a.y * b0 + b.y * b1 + c.y * b2,
                                 a.z * b0 + b.z * b1 + c.z * b2);
        sp.normal = faceN[t];
        sp.triangle = static_cast<std::uint32_t>(t);
        out.push_back(sp);
    }
    return out;
}

} // namespace maz::render
