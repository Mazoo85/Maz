#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render TRIANGLE-QUALITY / sliver analysis — the mesh-QA pass that flags badly-SHAPED triangles: long
// thin slivers and needles shade poorly, self-shadow, crawl under rasterization, and wreck physics and
// simplification. Every meshing tool reports a "triangle quality / minimum angle" histogram for exactly this.
// Per triangle it computes the normalized shape quality q = 4·sqrt(3)·area / (a²+b²+c²) — the standard mean-
// ratio metric that is 1 for a perfect equilateral triangle and falls toward 0 as a triangle degenerates into
// a sliver — plus its smallest interior angle (degrees, the number artists eyeball). It summarizes the worst
// triangle, the sliver count under a threshold, and the degenerate count. Pure CPU, header-only, headless.
namespace maz::render {

struct TriangleQualityStats {
    std::vector<float> quality;       // per triangle in [0,1]: 1 = equilateral, ->0 = sliver (0 if degenerate)
    std::vector<float> minAngleDeg;   // per triangle: smallest interior angle in degrees (0 if degenerate)
    float minQuality = 0.0f;          // over non-degenerate triangles
    float avgQuality = 0.0f;
    float minAngleOverall = 0.0f;     // smallest min-angle across all non-degenerate triangles
    std::uint32_t worstTriangle = 0;  // index of the lowest-quality non-degenerate triangle
    std::uint32_t sliverCount = 0;    // triangles with quality below `sliverThreshold`
    std::uint32_t degenerateCount = 0;
    std::uint32_t triangleCount = 0;
};

// Analyze the shape quality of every triangle. `sliverThreshold` (0..1) is the quality below which a
// non-degenerate triangle is counted as a sliver.
inline TriangleQualityStats analyzeTriangleQuality(const shapes::MeshData& mesh, float sliverThreshold = 0.1f) {
    TriangleQualityStats s;
    s.triangleCount = static_cast<std::uint32_t>(mesh.indices.size() / 3);
    s.quality.assign(s.triangleCount, 0.0f);
    s.minAngleDeg.assign(s.triangleCount, 0.0f);
    if (s.triangleCount == 0) return s;

    const float sqrt3 = 1.7320508075688772f;
    const float radToDeg = 57.29577951308232f;
    float mn = 1e30f, sum = 0.0f, mnAngle = 1e30f;
    std::uint32_t valid = 0;

    for (std::uint32_t t = 0; t < s.triangleCount; ++t) {
        const MeshVertex& A = mesh.vertices[mesh.indices[t * 3]];
        const MeshVertex& B = mesh.vertices[mesh.indices[t * 3 + 1]];
        const MeshVertex& C = mesh.vertices[mesh.indices[t * 3 + 2]];
        const float abx = B.px - A.px, aby = B.py - A.py, abz = B.pz - A.pz;
        const float acx = C.px - A.px, acy = C.py - A.py, acz = C.pz - A.pz;
        const float bcx = C.px - B.px, bcy = C.py - B.py, bcz = C.pz - B.pz;
        // Squared edge lengths.
        const float lab = abx * abx + aby * aby + abz * abz;
        const float lac = acx * acx + acy * acy + acz * acz;
        const float lbc = bcx * bcx + bcy * bcy + bcz * bcz;
        // Area from the cross product.
        const float cx = aby * acz - abz * acy, cy = abz * acx - abx * acz, cz = abx * acy - aby * acx;
        const float area = 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
        const float sumSq = lab + lac + lbc;
        if (area < 1e-12f || sumSq < 1e-20f) { ++s.degenerateCount; continue; }

        const float q = std::min(1.0f, 4.0f * sqrt3 * area / sumSq);
        s.quality[t] = q;

        // Smallest interior angle via the law of cosines on the three corners.
        const float la = std::sqrt(lbc), lb = std::sqrt(lac), lc = std::sqrt(lab); // opposite a=BC, b=CA, c=AB
        auto ang = [&](float opp, float s1, float s2) {
            float cosv = (s1 * s1 + s2 * s2 - opp * opp) / (2.0f * s1 * s2);
            cosv = std::max(-1.0f, std::min(1.0f, cosv));
            return std::acos(cosv);
        };
        const float angA = ang(la, lb, lc), angB = ang(lb, la, lc), angC = ang(lc, la, lb);
        const float minAng = std::min(angA, std::min(angB, angC)) * radToDeg;
        s.minAngleDeg[t] = minAng;

        if (q < mn) { mn = q; s.worstTriangle = t; }
        sum += q;
        mnAngle = std::min(mnAngle, minAng);
        if (q < sliverThreshold) ++s.sliverCount;
        ++valid;
    }

    if (valid > 0) {
        s.minQuality = mn;
        s.avgQuality = sum / static_cast<float>(valid);
        s.minAngleOverall = mnAngle;
    }
    return s;
}

} // namespace maz::render
