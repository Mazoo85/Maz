#pragma once

#include "maz/math/Math.hpp"        // math::vec3, dot
#include "maz/render/MeshWeld.hpp"  // weldVertices, WeldedMesh (the tool this feeds)
#include "maz/render/Shapes.hpp"    // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// maz::render WELD-TOLERANCE AUTO-DETECT — look at the spacing of a mesh's vertices and SUGGEST a good weld
// distance, so you don't have to guess the epsilon that M525 weldVertices needs. Importers routinely duplicate
// the vertices along every UV seam or smoothing split — sometimes at the exact same spot, sometimes a hair
// apart — and welding them back together is what makes a mesh watertight for physics, simplification, and
// normal smoothing. But pick the epsilon too small and the seams stay split; too large and you collapse genuine
// detail. This measures every vertex's nearest neighbour, finds the natural GAP between the tight cluster of
// duplicate/seam pairs and the much larger spacing of real geometry, and returns an epsilon that sits safely in
// that gap — plus a small report so you can see why. Header-only, pure CPU.
//
// Scope note (honest): nearest-neighbour distances are computed pairwise (O(n²)) — intended for import-time
// analysis of moderate meshes (up to a few thousand vertices); for very large meshes bucket or decimate first.
// The suggestion is a heuristic: when duplicates and real geometry are clearly separated it is reliable
// (`bimodal` = true); when they are not (a uniformly sampled surface with no duplicates), it returns a tiny
// conservative epsilon that welds nothing, and you should weld only if you know duplicates exist.
namespace maz::render {

struct WeldSuggestion {
    float suggestedEpsilon = 0.0f;         // recommended distance to pass to weldVertices
    float minGap = 0.0f;                    // smallest positive nearest-neighbour distance
    float medianGap = 0.0f;                 // median nearest-neighbour distance (~real vertex spacing)
    std::size_t exactDuplicateVertices = 0; // vertices sitting on top of another (gap ≈ 0)
    std::size_t weldableVertices = 0;       // vertices with a neighbour within suggestedEpsilon
    bool bimodal = false;                    // a clear duplicate-vs-real gap was found
};

// Analyse `mesh` and suggest a weld epsilon. Empty/one-vertex meshes yield a zeroed suggestion.
inline WeldSuggestion suggestWeldTolerance(const shapes::MeshData& mesh) {
    WeldSuggestion s;
    const std::size_t n = mesh.vertices.size();
    if (n < 2) return s;

    std::vector<math::vec3> p(n);
    for (std::size_t i = 0; i < n; ++i) {
        const MeshVertex& v = mesh.vertices[i];
        p[i] = math::vec3(v.px, v.py, v.pz);
    }
    // Bounding-box diagonal sets the scale for the "nothing to weld" fallback epsilon.
    math::vec3 lo = p[0], hi = p[0];
    for (const math::vec3& q : p) {
        lo = math::vec3(std::min(lo.x, q.x), std::min(lo.y, q.y), std::min(lo.z, q.z));
        hi = math::vec3(std::max(hi.x, q.x), std::max(hi.y, q.y), std::max(hi.z, q.z));
    }
    const math::vec3 ext = hi - lo;
    const float bboxDiag = std::sqrt(math::dot(ext, ext));

    // Nearest-neighbour distance for every vertex (pairwise).
    std::vector<float> nn(n, std::numeric_limits<float>::max());
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const math::vec3 d = p[i] - p[j];
            const float dist2 = math::dot(d, d);
            if (dist2 < nn[i] * nn[i]) nn[i] = std::sqrt(dist2);
            if (dist2 < nn[j] * nn[j]) nn[j] = std::sqrt(dist2);
        }
    }

    std::vector<float> positives; // nearest-neighbour gaps that are not (near) zero
    positives.reserve(n);
    for (float g : nn) {
        if (g < 1e-9f) ++s.exactDuplicateVertices;
        else positives.push_back(g);
    }
    std::sort(positives.begin(), positives.end());
    if (!positives.empty()) {
        s.minGap = positives.front();
        s.medianGap = positives[positives.size() / 2];
    } else {
        // Every vertex has an exact twin — weld just above zero.
        s.medianGap = bboxDiag > 0.0f ? bboxDiag : 1.0f;
    }

    const float realScale = s.medianGap > 0.0f ? s.medianGap : (bboxDiag > 0.0f ? bboxDiag : 1.0f);
    // "Small" gaps are those far below the real vertex spacing — candidate duplicates.
    float dupMax = 0.0f;
    std::size_t smallCount = 0;
    for (float g : positives) {
        if (g < 0.5f * realScale) {
            dupMax = std::max(dupMax, g);
            ++smallCount;
        }
    }

    if (s.exactDuplicateVertices > 0 || smallCount > 0) {
        // Sit the epsilon a few multiples above the widest duplicate gap, but always below the real spacing.
        const float floorEps = realScale * 1e-4f;             // catches exact duplicates
        float eps = std::max(dupMax * 4.0f, floorEps);
        eps = std::min(eps, 0.5f * realScale);                // never large enough to collapse real detail
        s.suggestedEpsilon = eps;
        s.bimodal = true;
    } else {
        // No duplicate cluster: suggest something that welds nothing (half the smallest real gap).
        s.suggestedEpsilon = (s.minGap > 0.0f) ? s.minGap * 0.5f : realScale * 1e-4f;
        s.bimodal = false;
    }

    for (float g : nn) {
        if (g <= s.suggestedEpsilon) ++s.weldableVertices;
    }
    return s;
}

// Convenience: analyse the mesh and immediately weld it at the suggested epsilon, returning the welded result.
inline WeldedMesh autoWeld(const shapes::MeshData& mesh) {
    const WeldSuggestion s = suggestWeldTolerance(mesh);
    std::vector<math::vec3> p(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const MeshVertex& v = mesh.vertices[i];
        p[i] = math::vec3(v.px, v.py, v.pz);
    }
    return weldVertices(p, mesh.indices, s.suggestedEpsilon);
}

} // namespace maz::render
