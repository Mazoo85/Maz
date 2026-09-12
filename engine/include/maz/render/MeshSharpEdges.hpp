#pragma once

#include "maz/math/Math.hpp"           // math::vec3, cross, dot
#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology (M528)
#include "maz/render/Shapes.hpp"       // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render DIHEDRAL-ANGLE / SHARP-EDGE DETECTION — for every interior edge, the dihedral angle between the
// two triangles that share it (0° = the faces fold flat back on themselves, 180° = perfectly flat/coplanar), and
// the list of edges whose faces bend more sharply than a threshold. Sharp edges are the CREASES of a model — a
// cube's twelve rims, the fold of a roof, the lip of a cup — and detecting them drives: wireframe/crease overlay
// in an editor, automatic bevel/chamfer selection, UV-seam and hard-normal suggestions (creases usually want a
// seam and a split normal), and feature-preserving simplification/smoothing that must not round a crease off.
// The angle is derived from the two face normals across the shared edge; boundary and non-manifold edges (not
// exactly two faces) are skipped. Reuses MeshTopology (M528) for the twin-face lookup. Header-only, std-only.
//
// Scope note (honest): "sharpness" is the deviation from flat — `sharpAngleDegrees` is the threshold on
// (180° − dihedral), so 0 flags nothing, 30 flags a moderate crease, 90 only right-angle-or-sharper folds. Uses
// geometric face normals (winding-consistent); a mesh with inconsistent winding may mis-sign an angle (reported
// unsigned). Weld first so a crease is one edge, not two boundary edges.
namespace maz::render {

struct SharpEdge {
    std::uint32_t a = 0, b = 0;     // the two endpoint vertex indices (a < b)
    float dihedralDegrees = 180.0f; // angle between the two faces (180 = flat)
    float sharpnessDegrees = 0.0f;  // 180 - dihedral (0 = flat, larger = sharper crease)
};

struct SharpEdgeResult {
    std::vector<SharpEdge> edges;   // all interior (2-face) edges with their dihedral angle
    std::vector<std::uint32_t> sharp; // indices into `edges` for edges past the threshold
    float minDihedral = 180.0f;     // sharpest interior edge (smallest dihedral) — 180 if none
    std::size_t interiorEdgeCount() const { return edges.size(); }
    std::size_t sharpCount() const { return sharp.size(); }
};

// Detect sharp edges of `mesh`; an edge is "sharp" when its faces deviate from flat by more than
// `sharpAngleDegrees` (i.e. dihedral < 180 − sharpAngleDegrees).
inline SharpEdgeResult detectSharpEdges(const shapes::MeshData& mesh, float sharpAngleDegrees = 30.0f) {
    SharpEdgeResult out;
    const MeshTopology topo = buildTopology(mesh);
    if (topo.triangleCount == 0) return out;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };
    auto faceNormal = [&](std::uint32_t t) {
        const std::uint32_t i0 = mesh.indices[static_cast<std::size_t>(t) * 3 + 0];
        const std::uint32_t i1 = mesh.indices[static_cast<std::size_t>(t) * 3 + 1];
        const std::uint32_t i2 = mesh.indices[static_cast<std::size_t>(t) * 3 + 2];
        return math::cross(pos(i1) - pos(i0), pos(i2) - pos(i0));
    };

    const double pi = 3.14159265358979323846;
    const float sharpThresholdDihedral = 180.0f - sharpAngleDegrees;

    // Visit each interior edge once: a half-edge whose twin exists and has a larger index owns the edge.
    for (std::uint32_t he = 0; he < topo.opposite.size(); ++he) {
        const std::uint32_t tw = topo.opposite[he];
        if (tw == MeshTopology::kNone || tw < he) continue; // boundary, non-manifold, or already-visited
        const std::uint32_t t0 = he / 3, t1 = tw / 3;
        const std::uint32_t e = he % 3;
        std::uint32_t va = mesh.indices[static_cast<std::size_t>(t0) * 3 + e];
        std::uint32_t vb = mesh.indices[static_cast<std::size_t>(t0) * 3 + (e + 1) % 3];
        if (va > vb) { const std::uint32_t s = va; va = vb; vb = s; }

        const math::vec3 n0 = faceNormal(t0), n1 = faceNormal(t1);
        const float l0 = std::sqrt(math::dot(n0, n0)), l1 = std::sqrt(math::dot(n1, n1));
        float dihedral = 180.0f; // degenerate faces -> treat as flat
        if (l0 > 1e-20f && l1 > 1e-20f) {
            float c = math::dot(n0, n1) / (l0 * l1);
            c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
            // Angle between normals is the SUPPLEMENT of the dihedral: coplanar faces (dihedral 180) have
            // parallel normals (angle 0). So dihedral = 180 - angleBetweenNormals.
            const float angleBetween = static_cast<float>(std::acos(static_cast<double>(c)) * 180.0 / pi);
            dihedral = 180.0f - angleBetween;
        }
        SharpEdge se;
        se.a = va; se.b = vb;
        se.dihedralDegrees = dihedral;
        se.sharpnessDegrees = 180.0f - dihedral;
        const std::uint32_t idx = static_cast<std::uint32_t>(out.edges.size());
        out.edges.push_back(se);
        if (dihedral < out.minDihedral) out.minDihedral = dihedral;
        if (dihedral < sharpThresholdDihedral) out.sharp.push_back(idx);
    }
    return out;
}

} // namespace maz::render
