#pragma once

#include "maz/render/MeshTopology.hpp" // buildTopology (boundary-vertex flags)
#include "maz/render/Shapes.hpp"       // shapes::MeshData, MeshVertex

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render PER-VERTEX DISCRETE CURVATURE — estimate how sharply a triangle mesh bends at every vertex,
// producing two scalar fields: GAUSSIAN curvature K (the angle deficit — positive on domes, negative on
// saddles, zero on anything developable like a plane or a cylinder) and MEAN curvature |H| (the average of
// the two principal curvatures, a bend magnitude). These are the classic Meyer / Desbrun / Schroeder / Barr
// "Discrete Differential-Geometry Operators for Triangulated 2-Manifolds" (2003) estimates:
//   Gaussian K_i = (2*pi - sum of incident triangle angles at i) / A_mixed(i)
//   Mean     H_i = |(1 / (2*A_mixed)) * sum_j (cot a_ij + cot b_ij)(x_i - x_j)| / 2   (cotangent Laplacian)
// where A_mixed is the Voronoi/barycentric mixed area (obtuse-triangle safe). On a sphere of radius r these
// converge to K = 1/r^2 and H = 1/r; on a flat sheet both are 0. Uses: curvature-adaptive tessellation and
// LOD, feature / crease / ridge-valley detection, curvature-guided remeshing and texturing, wear / cavity
// shading masks, and "curvature" vertex-paint like a DCC tool. Reuses MeshTopology (M528) only to flag
// boundary vertices, where the closed-surface estimate does not apply. Pure CPU, header-only, deterministic.
//
// Scope note (honest): the mesh should be welded (a shared corner = one vertex) for the angle sum and the
// Laplacian to close up — feed it through weldVertices (MeshWeld) first if it was built face-by-face.
// Boundary vertices are flagged and left at zero (their estimate is unreliable), not silently reported.
namespace maz::render {

struct MeshCurvature {
    std::vector<float> gaussian;        // per-vertex Gaussian curvature K (signed)
    std::vector<float> mean;            // per-vertex mean curvature magnitude |H| (non-negative)
    std::vector<std::uint8_t> boundary; // 1 if the vertex lies on an open boundary (estimate not computed)
    std::size_t vertexCount = 0;
};

// Estimate Gaussian and mean curvature at every vertex of `mesh`. A watertight closed surface gives a valid
// estimate at every vertex; open boundaries are flagged (boundary[i]==1) and their curvatures left at 0.
inline MeshCurvature computeCurvature(const shapes::MeshData& mesh) {
    MeshCurvature out;
    const std::size_t vn = mesh.vertices.size();
    out.vertexCount = vn;
    out.gaussian.assign(vn, 0.0f);
    out.mean.assign(vn, 0.0f);
    out.boundary.assign(vn, 0);
    if (vn == 0 || mesh.indices.size() < 3) return out;

    using Vec3 = std::array<double, 3>;
    auto pos = [&](std::uint32_t i) -> Vec3 {
        const MeshVertex& v = mesh.vertices[i];
        return {static_cast<double>(v.px), static_cast<double>(v.py), static_cast<double>(v.pz)};
    };
    auto sub = [](const Vec3& a, const Vec3& b) -> Vec3 { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; };
    auto dot = [](const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    auto crossLen = [](const Vec3& a, const Vec3& b) {
        const double x = a[1] * b[2] - a[2] * b[1];
        const double y = a[2] * b[0] - a[0] * b[2];
        const double z = a[0] * b[1] - a[1] * b[0];
        return std::sqrt(x * x + y * y + z * z);
    };

    std::vector<double> angleSum(vn, 0.0);
    std::vector<double> mixedArea(vn, 0.0);
    std::vector<Vec3> laplace(vn, Vec3{0.0, 0.0, 0.0}); // sum_j (cot a + cot b)(x_i - x_j)

    const std::size_t triCount = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        if (ia >= vn || ib >= vn || ic >= vn) continue;
        const Vec3 Pa = pos(ia), Pb = pos(ib), Pc = pos(ic);

        const Vec3 ab = sub(Pb, Pa), ac = sub(Pc, Pa); // at a
        const Vec3 bc = sub(Pc, Pb);                    // shared
        const double dotA = dot(ab, ac);
        const double dotB = dot(sub(Pa, Pb), bc);       // ba . bc
        const double dotC = dot(sub(Pa, Pc), sub(Pb, Pc)); // ca . cb
        const double twoArea = crossLen(ab, ac);        // = 2 * triangle area (same at every corner)
        if (twoArea < 1e-20) continue;                  // degenerate sliver
        const double area = 0.5 * twoArea;

        // Interior angles from atan2(|cross|, dot) — robust across the full 0..pi range.
        angleSum[ia] += std::atan2(twoArea, dotA);
        angleSum[ib] += std::atan2(twoArea, dotB);
        angleSum[ic] += std::atan2(twoArea, dotC);

        // Cotangents of the three angles (cot = cos/sin = dot / |cross|).
        const double cotA = dotA / twoArea;
        const double cotB = dotB / twoArea;
        const double cotC = dotC / twoArea;

        // Cotangent-Laplacian: edge (i,j) is weighted by the cot of the angle opposite it.
        // edge ab opposite c -> cotC ; edge bc opposite a -> cotA ; edge ac opposite b -> cotB.
        const Vec3 dAB = ab; // Pb - Pa
        laplace[ia][0] -= cotC * dAB[0]; laplace[ia][1] -= cotC * dAB[1]; laplace[ia][2] -= cotC * dAB[2];
        laplace[ib][0] += cotC * dAB[0]; laplace[ib][1] += cotC * dAB[1]; laplace[ib][2] += cotC * dAB[2];
        const Vec3 dBC = bc; // Pc - Pb
        laplace[ib][0] -= cotA * dBC[0]; laplace[ib][1] -= cotA * dBC[1]; laplace[ib][2] -= cotA * dBC[2];
        laplace[ic][0] += cotA * dBC[0]; laplace[ic][1] += cotA * dBC[1]; laplace[ic][2] += cotA * dBC[2];
        const Vec3 dAC = ac; // Pc - Pa
        laplace[ia][0] -= cotB * dAC[0]; laplace[ia][1] -= cotB * dAC[1]; laplace[ia][2] -= cotB * dAC[2];
        laplace[ic][0] += cotB * dAC[0]; laplace[ic][1] += cotB * dAC[1]; laplace[ic][2] += cotB * dAC[2];

        // Mixed area (Meyer et al.): Voronoi region when the triangle is non-obtuse, else a safe split.
        const double lab2 = dot(ab, ab), lac2 = dot(ac, ac), lbc2 = dot(bc, bc);
        if (dotA >= 0.0 && dotB >= 0.0 && dotC >= 0.0) {
            mixedArea[ia] += (lab2 * cotC + lac2 * cotB) * 0.125;
            mixedArea[ib] += (lab2 * cotC + lbc2 * cotA) * 0.125;
            mixedArea[ic] += (lac2 * cotB + lbc2 * cotA) * 0.125;
        } else if (dotA < 0.0) {          // obtuse at a
            mixedArea[ia] += area * 0.5; mixedArea[ib] += area * 0.25; mixedArea[ic] += area * 0.25;
        } else if (dotB < 0.0) {          // obtuse at b
            mixedArea[ib] += area * 0.5; mixedArea[ia] += area * 0.25; mixedArea[ic] += area * 0.25;
        } else {                          // obtuse at c
            mixedArea[ic] += area * 0.5; mixedArea[ia] += area * 0.25; mixedArea[ib] += area * 0.25;
        }
    }

    // Flag boundary vertices — the closed-surface angle deficit does not apply there.
    const MeshTopology topo = buildTopology(mesh);
    for (std::size_t i = 0; i < vn && i < topo.boundaryVertex.size(); ++i)
        out.boundary[i] = topo.boundaryVertex[i];

    const double twoPi = 2.0 * 3.14159265358979323846;
    for (std::size_t i = 0; i < vn; ++i) {
        if (out.boundary[i]) continue;         // left at 0 (unreliable)
        const double A = mixedArea[i];
        if (A <= 1e-18) continue;
        out.gaussian[i] = static_cast<float>((twoPi - angleSum[i]) / A);
        const Vec3& L = laplace[i];
        const double kLen = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
        // |K(x_i)| = kLen / (2A) equals 2*|H|, so |H| = kLen / (4A).
        out.mean[i] = static_cast<float>(kLen / (4.0 * A));
    }
    return out;
}

} // namespace maz::render
