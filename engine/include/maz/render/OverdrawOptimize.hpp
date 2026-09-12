#pragma once

#include "maz/math/Math.hpp"       // math::vec3, cross, dot, normalize
#include "maz/render/Shapes.hpp"   // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

// maz::render OVERDRAW optimization — the load-time triangle reorder that pairs with vertex-cache
// optimization (VertexCacheOptimize.hpp) to make opaque geometry cheaper to shade. Vertex-cache order cuts
// redundant VERTEX-shader work; overdraw order cuts redundant FRAGMENT-shader work: with early-Z depth
// testing, a fragment behind an already-drawn one is rejected before shading, so drawing triangles roughly
// FRONT-TO-BACK means each pixel is shaded close to once instead of once per overlapping layer. This is the
// meshopt `optimizeOverdraw` / Godot importer idea in its verifiable core: `optimizeOverdraw` returns the
// mesh with its triangles reordered nearest-first along a view direction (a pure index permutation —
// positions and the triangle SET are untouched), and `simulateOverdraw` software-rasterizes the mesh under
// an orthographic view and counts depth-test-PASSING fragment writes, so a unit test can prove the reorder
// lowers overdraw. Pure CPU (no GPU), headless.
//
// Scope note (honest): a static front-to-back order for a given view direction (the standard early-Z win for
// a dominant camera — foliage cards, UI, top-down scenes). meshopt's view-independent cluster reordering
// that also preserves the vertex-cache ACMR within a threshold is the heavier follow-up; this is the direct,
// measurable core. Run it AFTER optimizeVertexCache when you want both wins for a known dominant view.
namespace maz::render {

// Overdraw measured by a tiny software rasterizer: `shadedFragments` = fragments that passed the depth test
// (would be shaded), `coveredPixels` = distinct pixels touched by any triangle, `overdraw` = their ratio
// (1.0 = perfect, each covered pixel shaded exactly once; higher = wasted fragment work).
struct OverdrawStats {
    std::uint32_t shadedFragments = 0;
    std::uint32_t coveredPixels = 0;
    float overdraw = 0.0f;
};

namespace detail {

// Right-handed orthonormal basis with `forward` = normalized view direction.
inline void overdrawBasis(const math::vec3& viewDir, math::vec3& right, math::vec3& up, math::vec3& forward) {
    forward = math::normalize(viewDir);
    const math::vec3 ref = (std::fabs(forward.y) < 0.99f) ? math::vec3(0, 1, 0) : math::vec3(1, 0, 0);
    right = math::normalize(math::cross(ref, forward));
    up = math::cross(forward, right);
}

} // namespace detail

// Software-rasterize `mesh` under an orthographic view along `viewDir` into a `resolution`×`resolution`
// depth buffer, drawing triangles in their current index order and counting depth-test-passing writes.
// Order-sensitive: front-to-back order yields fewer shaded fragments (later, occluded triangles fail the
// test) than back-to-front. Positions untouched.
inline OverdrawStats simulateOverdraw(const shapes::MeshData& mesh, const math::vec3& viewDir, int resolution) {
    OverdrawStats stats;
    if (mesh.indices.size() < 3 || resolution < 2) return stats;
    if (math::dot(viewDir, viewDir) < 1e-20f) return stats;

    math::vec3 right, up, forward;
    detail::overdrawBasis(viewDir, right, up, forward);

    // Project every vertex to (u, v, depth) and find the screen-space bounds.
    const std::size_t vn = mesh.vertices.size();
    std::vector<float> pu(vn), pv(vn), pd(vn);
    float minU = 1e30f, minV = 1e30f, maxU = -1e30f, maxV = -1e30f;
    for (std::size_t i = 0; i < vn; ++i) {
        const MeshVertex& v = mesh.vertices[i];
        const math::vec3 p(v.px, v.py, v.pz);
        pu[i] = math::dot(p, right);
        pv[i] = math::dot(p, up);
        pd[i] = math::dot(p, forward); // depth: smaller = nearer the eye
        minU = std::min(minU, pu[i]); maxU = std::max(maxU, pu[i]);
        minV = std::min(minV, pv[i]); maxV = std::max(maxV, pv[i]);
    }
    const float spanU = std::max(maxU - minU, 1e-6f);
    const float spanV = std::max(maxV - minV, 1e-6f);
    const float res = static_cast<float>(resolution);

    const std::size_t px = static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
    std::vector<float> depth(px, std::numeric_limits<float>::infinity());
    std::vector<std::uint8_t> covered(px, 0);

    auto toX = [&](float u) { return (u - minU) / spanU * (res - 1.0f); };
    auto toY = [&](float v) { return (v - minV) / spanV * (res - 1.0f); };

    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t ia = mesh.indices[t], ib = mesh.indices[t + 1], ic = mesh.indices[t + 2];
        if (ia >= vn || ib >= vn || ic >= vn) continue;
        const float ax = toX(pu[ia]), ay = toY(pv[ia]), ad = pd[ia];
        const float bx = toX(pu[ib]), by = toY(pv[ib]), bd = pd[ib];
        const float cx = toX(pu[ic]), cy = toY(pv[ic]), cd = pd[ic];

        const float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
        if (std::fabs(area) < 1e-9f) continue; // degenerate in screen space
        const float invArea = 1.0f / area;

        int x0 = static_cast<int>(std::floor(std::min({ax, bx, cx})));
        int x1 = static_cast<int>(std::ceil(std::max({ax, bx, cx})));
        int y0 = static_cast<int>(std::floor(std::min({ay, by, cy})));
        int y1 = static_cast<int>(std::ceil(std::max({ay, by, cy})));
        x0 = std::max(x0, 0); y0 = std::max(y0, 0);
        x1 = std::min(x1, resolution - 1); y1 = std::min(y1, resolution - 1);

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const float fx = static_cast<float>(x) + 0.5f, fy = static_cast<float>(y) + 0.5f;
                // Barycentric coordinates via edge functions.
                const float w0 = ((bx - fx) * (cy - fy) - (cx - fx) * (by - fy)) * invArea;
                const float w1 = ((cx - fx) * (ay - fy) - (ax - fx) * (cy - fy)) * invArea;
                const float w2 = 1.0f - w0 - w1;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue; // outside triangle
                const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(resolution)
                                      + static_cast<std::size_t>(x);
                if (!covered[idx]) { covered[idx] = 1; ++stats.coveredPixels; }
                const float d = w0 * ad + w1 * bd + w2 * cd;
                if (d < depth[idx]) { depth[idx] = d; ++stats.shadedFragments; } // passes early-Z (LESS)
            }
        }
    }
    stats.overdraw = stats.coveredPixels > 0
        ? static_cast<float>(stats.shadedFragments) / static_cast<float>(stats.coveredPixels)
        : 0.0f;
    return stats;
}

// Return `mesh` with its triangles reordered NEAREST-FIRST along `viewDir` (a stable sort by each triangle's
// centroid depth). Positions and the triangle set are untouched — only the index order changes — so with
// early-Z this minimizes shaded fragments for that view. A zero/degenerate viewDir returns a copy unchanged.
inline shapes::MeshData optimizeOverdraw(const shapes::MeshData& mesh, const math::vec3& viewDir) {
    shapes::MeshData out = mesh;
    const std::size_t triCount = mesh.indices.size() / 3;
    if (triCount < 2 || math::dot(viewDir, viewDir) < 1e-20f) return out;

    math::vec3 right, up, forward;
    detail::overdrawBasis(viewDir, right, up, forward);

    // Depth key per triangle = centroid projected onto `forward` (smaller = nearer).
    std::vector<float> key(triCount);
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3], ib = mesh.indices[t * 3 + 1], ic = mesh.indices[t * 3 + 2];
        const MeshVertex& a = mesh.vertices[ia];
        const MeshVertex& b = mesh.vertices[ib];
        const MeshVertex& c = mesh.vertices[ic];
        const math::vec3 centroid((a.px + b.px + c.px) / 3.0f,
                                  (a.py + b.py + c.py) / 3.0f,
                                  (a.pz + b.pz + c.pz) / 3.0f);
        key[t] = math::dot(centroid, forward);
    }

    std::vector<std::size_t> order(triCount);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t l, std::size_t r) { return key[l] < key[r]; });

    out.indices.clear();
    out.indices.reserve(mesh.indices.size());
    for (std::size_t t : order) {
        out.indices.push_back(mesh.indices[t * 3]);
        out.indices.push_back(mesh.indices[t * 3 + 1]);
        out.indices.push_back(mesh.indices[t * 3 + 2]);
    }
    return out;
}

} // namespace maz::render
