#pragma once

#include "maz/math/Math.hpp"           // math::vec3, cross, dot
#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology (M528)
#include "maz/render/Shapes.hpp"       // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render FEATURE-LINE EXTRACTION (ridge / valley crest lines) — find the SHARP FOLDS of a mesh, label each
// as a convex RIDGE or a concave VALLEY, and CHAIN them into connected polylines. Where M548 sharp-edge detection
// just answers "which edges are creased," this goes two steps further: it tells you which way each crease bends
// (a roof ridge vs a gutter valley) and stitches the loose creased edges into actual CURVES. Those curves are
// what stylized/NPR renderers stroke as ink outlines and interior "hard" lines, what retopo tools follow to lay
// clean edge loops, what auto-UV uses as natural seam candidates, and what a "select hard edges" editor command
// returns. Reuses the M528 half-edge topology; classification uses the sign of the fold relative to the outward
// face normals. Header-only, pure CPU.
//
// Scope note (honest): ridge/valley sign needs OUTWARD-consistent winding (an inside-out mesh flips the labels);
// only manifold interior edges (exactly two faces) are considered — boundary and non-manifold edges are skipped.
// Chaining produces maximal simple paths and breaks at junctions (a vertex where three-plus feature edges meet),
// so a branching crest network comes back as several polylines meeting at the junction, not one tangled path.
namespace maz::render {

enum class FeatureKind : std::uint8_t { Ridge, Valley }; // convex fold vs concave fold

struct FeatureEdge {
    std::uint32_t a = 0, b = 0;    // endpoint vertex indices (a < b)
    float sharpnessDegrees = 0.0f; // 180 − dihedral: 0 flat, larger = sharper crease
    FeatureKind kind = FeatureKind::Ridge;
};

struct FeatureLines {
    std::vector<FeatureEdge> edges;               // every crease past the threshold
    std::vector<std::vector<std::uint32_t>> lines; // chained vertex-index polylines
    std::size_t ridgeCount = 0;                   // convex creases
    std::size_t valleyCount = 0;                  // concave creases
};

// Extract feature lines of `mesh`. An edge is a feature when its faces fold past `sharpAngleDegrees` from flat
// (dihedral < 180 − sharpAngleDegrees). Each is labelled Ridge (convex) or Valley (concave) and edges are
// chained into polylines.
inline FeatureLines extractFeatureLines(const shapes::MeshData& mesh, float sharpAngleDegrees = 30.0f) {
    FeatureLines out;
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
    auto apex = [&](std::uint32_t t, std::uint32_t va, std::uint32_t vb) { // the triangle vertex off the edge
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t vi = mesh.indices[static_cast<std::size_t>(t) * 3 + static_cast<std::size_t>(k)];
            if (vi != va && vi != vb) return vi;
        }
        return va;
    };

    const double pi = 3.14159265358979323846;
    const float sharpThresholdDihedral = 180.0f - sharpAngleDegrees;

    for (std::uint32_t he = 0; he < topo.opposite.size(); ++he) {
        const std::uint32_t tw = topo.opposite[he];
        if (tw == MeshTopology::kNone || tw < he) continue; // boundary / non-manifold / already-owned
        const std::uint32_t t0 = he / 3, t1 = tw / 3;
        const std::uint32_t e = he % 3;
        std::uint32_t va = mesh.indices[static_cast<std::size_t>(t0) * 3 + e];
        std::uint32_t vb = mesh.indices[static_cast<std::size_t>(t0) * 3 + (e + 1) % 3];
        if (va > vb) { const std::uint32_t s = va; va = vb; vb = s; }

        const math::vec3 n0 = faceNormal(t0), n1 = faceNormal(t1);
        const float l0 = std::sqrt(math::dot(n0, n0)), l1 = std::sqrt(math::dot(n1, n1));
        if (l0 <= 1e-20f || l1 <= 1e-20f) continue; // degenerate face -> treat as flat, skip
        float c = math::dot(n0, n1) / (l0 * l1);
        c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
        const float angleBetween = static_cast<float>(std::acos(static_cast<double>(c)) * 180.0 / pi);
        const float dihedral = 180.0f - angleBetween;
        if (dihedral >= sharpThresholdDihedral) continue; // not creased enough

        // Convex (ridge) vs concave (valley): if t1's off-edge apex sits BELOW t0's outward plane, the surface
        // bulges out here -> ridge; if it sits above, the surface caves in -> valley.
        const std::uint32_t apex1 = apex(t1, va, vb);
        const float side = math::dot(n0, pos(apex1) - pos(va)); // n0 need not be unit; only the sign matters
        FeatureEdge fe;
        fe.a = va;
        fe.b = vb;
        fe.sharpnessDegrees = angleBetween;
        fe.kind = (side < 0.0f) ? FeatureKind::Ridge : FeatureKind::Valley;
        if (fe.kind == FeatureKind::Ridge) ++out.ridgeCount;
        else ++out.valleyCount;
        out.edges.push_back(fe);
    }

    // Chain feature edges into maximal simple polylines. Build a vertex -> incident-edge adjacency, then walk.
    const std::size_t en = out.edges.size();
    if (en == 0) return out;
    std::vector<std::vector<std::size_t>> incident; // per used vertex id, the feature-edge indices touching it
    std::uint32_t maxV = 0;
    for (const FeatureEdge& fe : out.edges) {
        maxV = fe.a > maxV ? fe.a : maxV;
        maxV = fe.b > maxV ? fe.b : maxV;
    }
    incident.assign(static_cast<std::size_t>(maxV) + 1, {});
    for (std::size_t i = 0; i < en; ++i) {
        incident[out.edges[i].a].push_back(i);
        incident[out.edges[i].b].push_back(i);
    }
    std::vector<bool> used(en, false);
    auto other = [&](std::size_t ei, std::uint32_t v) {
        return out.edges[ei].a == v ? out.edges[ei].b : out.edges[ei].a;
    };
    // Extend a chain from vertex `v` while exactly one unused feature edge continues (degree-2 interior); a
    // junction (more than one continuation) or a dead end stops the walk.
    auto walkFrom = [&](std::uint32_t start, std::size_t firstEdge, std::vector<std::uint32_t>& line) {
        std::uint32_t v = start;
        std::size_t ei = firstEdge;
        while (true) {
            used[ei] = true;
            const std::uint32_t nv = other(ei, v);
            line.push_back(nv);
            std::size_t next = en;
            int cont = 0;
            for (std::size_t cand : incident[nv]) {
                if (!used[cand]) { ++cont; next = cand; }
            }
            if (cont != 1) break; // junction or dead end
            v = nv;
            ei = next;
        }
    };
    // Start chains at natural endpoints/junctions (degree != 2) first so open curves come out whole.
    for (std::uint32_t v = 0; v <= maxV; ++v) {
        if (incident[v].size() == 2) continue;
        for (std::size_t ei : incident[v]) {
            if (used[ei]) continue;
            std::vector<std::uint32_t> line{v};
            walkFrom(v, ei, line);
            out.lines.push_back(std::move(line));
        }
    }
    // Any remaining unused edges belong to pure loops (every vertex degree 2); seed them anywhere.
    for (std::size_t i = 0; i < en; ++i) {
        if (used[i]) continue;
        std::vector<std::uint32_t> line{out.edges[i].a};
        walkFrom(out.edges[i].a, i, line);
        out.lines.push_back(std::move(line));
    }
    return out;
}

} // namespace maz::render
