#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <queue>
#include <vector>

// maz::render feature-preserving mesh simplification by QUADRIC-ERROR EDGE COLLAPSE (Garland–Heckbert QEM) —
// the higher-quality decimation an engine's importer runs to make LODs that keep their silhouette at
// aggressive triangle budgets, where the O(n) vertex-clustering path (MeshSimplify.hpp) would visibly round
// off edges. Each vertex carries a 4×4 error quadric (the summed squared distance to the planes of its
// incident triangles); collapsing an edge merges the two quadrics, and the cost of the collapse is the
// quadric evaluated at the optimal merged position (found by solving a 3×3 system, or falling back to the
// cheaper of the two endpoints / their midpoint when that system is singular). A min-heap always collapses
// the cheapest edge next, so flat regions decimate first and creases/boundaries — which have high quadric
// error — survive. Pure CPU vertex math (no GPU), so it unit-tests headlessly: fewer verts/tris toward the
// requested budget, every triangle non-degenerate + in range, the bounding box and a curved surface's peak
// preserved, and deterministic (same input → same output).
//
// Scope note (honest): manifold-oriented QEM with area-weighted quadrics and optimal-position placement.
// It complements — does not replace — simplifyClustering: clustering is the fast, hole-proof choice for
// collision proxies and far LODs; QEM is the silhouette-preserving choice for visible mid LODs. Attribute
// (normal/color/UV) handling is position-driven: normals are recomputed on the result; color/UV are carried
// from the surviving representative. Non-manifold input still simplifies but without manifold guarantees.
namespace maz::render {

namespace detail {

// A symmetric 4×4 error quadric stored as its 10 unique doubles. Error at v=(x,y,z,1) is vᵀQv.
struct Quadric {
    double m11 = 0, m12 = 0, m13 = 0, m14 = 0;
    double m22 = 0, m23 = 0, m24 = 0;
    double m33 = 0, m34 = 0;
    double m44 = 0;
    Quadric& operator+=(const Quadric& o) {
        m11 += o.m11; m12 += o.m12; m13 += o.m13; m14 += o.m14;
        m22 += o.m22; m23 += o.m23; m24 += o.m24;
        m33 += o.m33; m34 += o.m34; m44 += o.m44;
        return *this;
    }
    // Fundamental quadric of a plane (a,b,c,d) — the outer product ppᵀ — scaled by `w` (triangle area).
    static Quadric plane(double a, double b, double c, double d, double w) {
        Quadric q;
        q.m11 = w * a * a; q.m12 = w * a * b; q.m13 = w * a * c; q.m14 = w * a * d;
        q.m22 = w * b * b; q.m23 = w * b * c; q.m24 = w * b * d;
        q.m33 = w * c * c; q.m34 = w * c * d;
        q.m44 = w * d * d;
        return q;
    }
    // vᵀQv for v=(x,y,z,1).
    double error(double x, double y, double z) const {
        return m11 * x * x + 2 * m12 * x * y + 2 * m13 * x * z + 2 * m14 * x
             + m22 * y * y + 2 * m23 * y * z + 2 * m24 * y
             + m33 * z * z + 2 * m34 * z
             + m44;
    }
};

} // namespace detail

// Simplify `in` to at most `targetTriangles` triangles by quadric-error edge collapse. Returns a new mesh;
// `in` is untouched. Coincident positions are welded first (so a per-face-duplicated import mesh still forms
// shared edges). If `targetTriangles` already meets/exceeds the welded triangle count, no collapse happens.
inline shapes::MeshData simplifyQuadric(const shapes::MeshData& in, std::size_t targetTriangles) {
    using detail::Quadric;
    shapes::MeshData out;
    if (in.indices.size() < 3) return out;

    // --- 1. Weld coincident positions into unique vertices (1e-5 grid snap). ---
    struct Vec3 { double x, y, z; };
    auto keyOf = [](float x, float y, float z) {
        const double s = 100000.0; // 1e-5 quantum
        return std::array<std::int64_t, 3>{
            static_cast<std::int64_t>(std::llround(static_cast<double>(x) * s)),
            static_cast<std::int64_t>(std::llround(static_cast<double>(y) * s)),
            static_cast<std::int64_t>(std::llround(static_cast<double>(z) * s))};
    };
    std::map<std::array<std::int64_t, 3>, std::uint32_t> welded;
    std::vector<Vec3> pos;
    std::vector<MeshVertex> rep;  // representative attributes per unique vertex
    std::vector<std::uint32_t> remap(in.vertices.size(), 0);
    for (std::size_t i = 0; i < in.vertices.size(); ++i) {
        const MeshVertex& v = in.vertices[i];
        const auto k = keyOf(v.px, v.py, v.pz);
        auto it = welded.find(k);
        if (it == welded.end()) {
            const std::uint32_t id = static_cast<std::uint32_t>(pos.size());
            welded.emplace(k, id);
            pos.push_back({static_cast<double>(v.px), static_cast<double>(v.py), static_cast<double>(v.pz)});
            rep.push_back(v);
            remap[i] = id;
        } else {
            remap[i] = it->second;
        }
    }

    // Welded triangle list (drop any that are already degenerate after welding).
    std::vector<std::array<std::uint32_t, 3>> tris;
    tris.reserve(in.indices.size() / 3);
    for (std::size_t t = 0; t + 2 < in.indices.size(); t += 3) {
        const std::uint32_t a = remap[in.indices[t]], b = remap[in.indices[t + 1]], c = remap[in.indices[t + 2]];
        if (a == b || b == c || a == c) continue;
        tris.push_back({a, b, c});
    }
    const std::size_t nVerts = pos.size();
    if (tris.empty() || nVerts == 0) return out;

    // --- 2. Accumulate per-vertex quadrics from area-weighted triangle planes. ---
    std::vector<Quadric> Q(nVerts);
    for (const auto& tri : tris) {
        const Vec3& p0 = pos[tri[0]], & p1 = pos[tri[1]], & p2 = pos[tri[2]];
        const double ux = p1.x - p0.x, uy = p1.y - p0.y, uz = p1.z - p0.z;
        const double vx = p2.x - p0.x, vy = p2.y - p0.y, vz = p2.z - p0.z;
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len < 1e-20) continue;
        const double area = 0.5 * len;
        nx /= len; ny /= len; nz /= len;
        const double d = -(nx * p0.x + ny * p0.y + nz * p0.z);
        const Quadric qp = Quadric::plane(nx, ny, nz, d, area);
        Q[tri[0]] += qp; Q[tri[1]] += qp; Q[tri[2]] += qp;
    }

    // --- 3. Per-vertex triangle incidence + liveness/version bookkeeping. ---
    std::vector<std::vector<std::uint32_t>> vtris(nVerts); // triangle indices touching each vertex
    for (std::uint32_t ti = 0; ti < tris.size(); ++ti)
        for (std::uint32_t c = 0; c < 3; ++c) vtris[tris[ti][c]].push_back(ti);
    std::vector<char> alive(nVerts, 1);
    std::vector<char> triDead(tris.size(), 0);
    std::vector<std::uint64_t> version(nVerts, 0);
    std::size_t liveTris = tris.size();

    // --- 4. Solve for the best merged position of an edge and its collapse cost. ---
    auto bestPosition = [&](const Quadric& q, std::uint32_t a, std::uint32_t b,
                            double& ox, double& oy, double& oz) {
        // Optimal point solves the 3×3 system A·v = -[m14,m24,m34] where A is the quadric's top-left block.
        const double a11 = q.m11, a12 = q.m12, a13 = q.m13;
        const double a22 = q.m22, a23 = q.m23, a33 = q.m33;
        const double det = a11 * (a22 * a33 - a23 * a23)
                         - a12 * (a12 * a33 - a23 * a13)
                         + a13 * (a12 * a23 - a22 * a13);
        if (std::fabs(det) > 1e-12) {
            const double bx = -q.m14, by = -q.m24, bz = -q.m34;
            const double inv = 1.0 / det;
            // v = A⁻¹ b via cofactors (A symmetric).
            const double c11 = (a22 * a33 - a23 * a23);
            const double c12 = (a13 * a23 - a12 * a33);
            const double c13 = (a12 * a23 - a13 * a22);
            const double c22 = (a11 * a33 - a13 * a13);
            const double c23 = (a13 * a12 - a11 * a23);
            const double c33 = (a11 * a22 - a12 * a12);
            ox = inv * (c11 * bx + c12 * by + c13 * bz);
            oy = inv * (c12 * bx + c22 * by + c23 * bz);
            oz = inv * (c13 * bx + c23 * by + c33 * bz);
            return q.error(ox, oy, oz);
        }
        // Singular: pick the cheapest of endpoint a, endpoint b, and their midpoint.
        const Vec3& pa = pos[a]; const Vec3& pb = pos[b];
        const double mx = 0.5 * (pa.x + pb.x), my = 0.5 * (pa.y + pb.y), mz = 0.5 * (pa.z + pb.z);
        const double ea = q.error(pa.x, pa.y, pa.z), eb = q.error(pb.x, pb.y, pb.z), em = q.error(mx, my, mz);
        if (ea <= eb && ea <= em) { ox = pa.x; oy = pa.y; oz = pa.z; return ea; }
        if (eb <= ea && eb <= em) { ox = pb.x; oy = pb.y; oz = pb.z; return eb; }
        ox = mx; oy = my; oz = mz; return em;
    };

    // --- 5. Priority queue of candidate collapses (lazy invalidation by version stamps). ---
    struct Cand {
        double cost;
        std::uint32_t a, b;
        double tx, ty, tz;
        std::uint64_t va, vb;
    };
    struct Cheaper { bool operator()(const Cand& x, const Cand& y) const { return x.cost > y.cost; } };
    std::priority_queue<Cand, std::vector<Cand>, Cheaper> pq;

    auto pushEdge = [&](std::uint32_t a, std::uint32_t b) {
        if (a == b) return;
        Quadric qs = Q[a]; qs += Q[b];
        double tx, ty, tz;
        const double cost = bestPosition(qs, a, b, tx, ty, tz);
        pq.push({cost, a, b, tx, ty, tz, version[a], version[b]});
    };

    // Neighbors of a live root, derived from its (live) incident triangles.
    auto neighborsOf = [&](std::uint32_t a, std::vector<std::uint32_t>& nbrs) {
        nbrs.clear();
        for (std::uint32_t ti : vtris[a]) {
            if (triDead[ti]) continue;
            for (std::uint32_t c = 0; c < 3; ++c) {
                const std::uint32_t n = tris[ti][c];
                if (n != a) nbrs.push_back(n);
            }
        }
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    };

    // Seed the queue with every unique edge (a<b once).
    {
        std::vector<std::uint32_t> nbrs;
        for (std::uint32_t a = 0; a < nVerts; ++a) {
            neighborsOf(a, nbrs);
            for (std::uint32_t n : nbrs) if (a < n) pushEdge(a, n);
        }
    }

    // --- 6. Greedily collapse the cheapest valid edge until the triangle budget is met. ---
    std::vector<std::uint32_t> nbrs;
    while (liveTris > targetTriangles && !pq.empty()) {
        const Cand e = pq.top(); pq.pop();
        const std::uint32_t a = e.a, b = e.b;
        if (!alive[a] || !alive[b]) continue;                     // an endpoint was merged away
        if (version[a] != e.va || version[b] != e.vb) continue;    // stale cost
        if (a == b) continue;

        // Collapse b into a: a survives, moves to the target, and absorbs b's quadric.
        pos[a] = {e.tx, e.ty, e.tz};
        Q[a] += Q[b];

        // Retarget b's triangles onto a; drop those that become degenerate.
        for (std::uint32_t ti : vtris[b]) {
            if (triDead[ti]) continue;
            auto& tr = tris[ti];
            for (std::uint32_t c = 0; c < 3; ++c) if (tr[c] == b) tr[c] = a;
            if (tr[0] == tr[1] || tr[1] == tr[2] || tr[0] == tr[2]) {
                triDead[ti] = 1;
                --liveTris;
            } else {
                vtris[a].push_back(ti);
            }
        }
        alive[b] = 0;
        vtris[b].clear();
        ++version[a];

        // Re-cost every edge now incident to a.
        neighborsOf(a, nbrs);
        for (std::uint32_t n : nbrs) pushEdge(a, n);
    }

    // --- 7. Rebuild a compact mesh from the survivors; recompute smooth normals. ---
    std::vector<std::uint32_t> compact(nVerts, 0xFFFFFFFFu);
    for (std::uint32_t ti = 0; ti < tris.size(); ++ti) {
        if (triDead[ti]) continue;
        for (std::uint32_t c = 0; c < 3; ++c) {
            const std::uint32_t vid = tris[ti][c];
            if (compact[vid] == 0xFFFFFFFFu) {
                compact[vid] = static_cast<std::uint32_t>(out.vertices.size());
                MeshVertex mv = rep[vid];
                mv.px = static_cast<float>(pos[vid].x);
                mv.py = static_cast<float>(pos[vid].y);
                mv.pz = static_cast<float>(pos[vid].z);
                mv.nx = mv.ny = mv.nz = 0.0f;
                out.vertices.push_back(mv);
            }
        }
        out.indices.push_back(compact[tris[ti][0]]);
        out.indices.push_back(compact[tris[ti][1]]);
        out.indices.push_back(compact[tris[ti][2]]);
    }

    // Area-weighted smooth normals over the simplified surface.
    for (std::size_t t = 0; t + 2 < out.indices.size(); t += 3) {
        MeshVertex& A = out.vertices[out.indices[t]];
        MeshVertex& B = out.vertices[out.indices[t + 1]];
        MeshVertex& C = out.vertices[out.indices[t + 2]];
        const float ux = B.px - A.px, uy = B.py - A.py, uz = B.pz - A.pz;
        const float vx = C.px - A.px, vy = C.py - A.py, vz = C.pz - A.pz;
        const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        A.nx += nx; A.ny += ny; A.nz += nz;
        B.nx += nx; B.ny += ny; B.nz += nz;
        C.nx += nx; C.ny += ny; C.nz += nz;
    }
    for (MeshVertex& v : out.vertices) {
        const float l = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        if (l > 1e-12f) { v.nx /= l; v.ny /= l; v.nz /= l; } else { v.ny = 1.0f; }
    }
    return out;
}

} // namespace maz::render
