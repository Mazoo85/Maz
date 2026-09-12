#pragma once

#include "maz/math/Geometry3D.hpp" // math::Aabb3 (slab ray/box test)
#include "maz/math/Math.hpp"       // math::vec3, cross, dot
#include "maz/render/Shapes.hpp"   // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

// maz::render MESH-TRIANGLE RAY BVH — a bounding-volume hierarchy built over ONE mesh's own triangles so a ray
// query resolves in ~O(log tris) node visits instead of testing every triangle. This is the acceleration
// structure every offline baker, picker, and CPU ray path wants: MeshAmbientOcclusion (M533) shoots a hemisphere
// of rays per vertex brute-force O(verts · rays · tris) and names "a BVH acceleration" as its documented
// follow-up; MeshContainment (M546), MeshSdf (M534), and MeshVoxelize (M540) all ray-parity against the full
// triangle list. This is that follow-up: build once, then `intersect` (nearest forward hit, with barycentrics)
// and `occluded` (any hit up to a distance — the early-out a shadow/AO ray needs) walk only the boxes the ray
// pierces. Distinct from game::Bvh, which indexes a whole LEVEL's boxes/triangles for broadphase; this indexes a
// single mesh's triangles for exact ray casts against it. Median-split on the longest centroid axis, front-to-back
// child ordering so the nearest hit is found early and far subtrees prune. Pure CPU, header-only, headless.
//
// The same tree also answers `closestPoint` (M548) — the nearest point ON the surface to an arbitrary query,
// the accelerated form of render::closestPointOnMesh — by pruning any node box already farther than the best
// point found. All queries take an optional `trianglesTested` out-param — the count of per-triangle tests the
// query did — so callers (and the unit tests) can measure the pruning directly against the brute-force count.
namespace maz::render {

namespace detail {

// Möller–Trumbore ray/triangle returning hit distance AND barycentrics (u, v); w = 1-u-v. Same intersection as
// aoRayTri (MeshAmbientOcclusion) but keeps the barycentrics for the hit point / attribute interpolation.
inline bool bvhRayTri(const math::vec3& o, const math::vec3& d, const math::vec3& a, const math::vec3& b,
                      const math::vec3& c, float tMax, float& t, float& u, float& v) {
    const math::vec3 e1 = b - a, e2 = c - a;
    const math::vec3 p = math::cross(d, e2);
    const float det = math::dot(e1, p);
    if (std::fabs(det) < 1e-9f) return false; // parallel or degenerate
    const float inv = 1.0f / det;
    const math::vec3 tv = o - a;
    u = math::dot(tv, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const math::vec3 q = math::cross(tv, e1);
    v = math::dot(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = math::dot(e2, q) * inv;
    return t > 1e-4f && t < tMax;
}

} // namespace detail

// Result of a nearest-hit ray query. `hit` false means the ray missed within the distance limit.
struct MeshRayHit {
    bool hit = false;
    float t = 0.0f;               // hit distance along the ray direction
    std::uint32_t triangle = 0;   // triangle index (indices[3*triangle .. +2]) that was hit
    float u = 0.0f, v = 0.0f;     // barycentrics; the third weight is 1 - u - v
    math::vec3 point{0.0f};       // world-space hit point (origin + t * dir)
};

// Result of a closest-point query. `valid` false means the mesh had no triangles.
struct MeshPointHit {
    bool valid = false;
    math::vec3 point{0.0f};       // nearest point on the mesh surface
    float distance = 0.0f;        // unsigned distance from the query to that point
    std::uint32_t triangle = 0;   // index of the winning triangle
    math::vec3 normal{0.0f};      // that triangle's unit geometric normal (zero if degenerate)
};

class MeshRayBvh {
public:
    MeshRayBvh() = default;

    // Build over a mesh's triangles. `dir` need not be normalized for queries, but `t`/distances are then in
    // units of |dir|; pass a normalized direction for world-space distances. Empty/degenerate input yields a
    // valid but empty BVH whose queries always miss.
    explicit MeshRayBvh(const shapes::MeshData& mesh) { build(mesh); }

    void build(const shapes::MeshData& mesh) {
        nodes_.clear();
        order_.clear();
        triA_.clear();
        triB_.clear();
        triC_.clear();

        const std::size_t triN = mesh.indices.size() / 3;
        if (triN == 0 || mesh.vertices.empty()) return;

        triA_.reserve(triN);
        triB_.reserve(triN);
        triC_.reserve(triN);
        std::vector<math::vec3> centroid(triN);
        const std::size_t vn = mesh.vertices.size();
        for (std::size_t i = 0; i < triN; ++i) {
            const std::uint32_t i0 = mesh.indices[3 * i];
            const std::uint32_t i1 = mesh.indices[3 * i + 1];
            const std::uint32_t i2 = mesh.indices[3 * i + 2];
            // Guard against out-of-range indices: collapse to a point at the origin (never hit).
            const math::vec3 a = i0 < vn ? vpos(mesh, i0) : math::vec3(0.0f);
            const math::vec3 b = i1 < vn ? vpos(mesh, i1) : math::vec3(0.0f);
            const math::vec3 c = i2 < vn ? vpos(mesh, i2) : math::vec3(0.0f);
            triA_.push_back(a);
            triB_.push_back(b);
            triC_.push_back(c);
            centroid[i] = (a + b + c) * (1.0f / 3.0f);
        }

        order_.resize(triN);
        for (std::size_t i = 0; i < triN; ++i) order_[i] = static_cast<std::uint32_t>(i);

        nodes_.reserve(triN * 2);
        buildRange(0, triN, centroid);
    }

    bool empty() const { return nodes_.empty(); }
    std::size_t triangleCount() const { return triA_.size(); }
    std::size_t nodeCount() const { return nodes_.size(); }

    // Nearest forward hit of the ray (origin + t*dir, t > 0) within [0, maxDist). `dir` is used as given.
    MeshRayHit intersect(const math::vec3& origin, const math::vec3& dir,
                         float maxDist = std::numeric_limits<float>::infinity(),
                         std::size_t* trianglesTested = nullptr) const {
        MeshRayHit best;
        best.t = maxDist;
        std::size_t tested = 0;
        if (!nodes_.empty()) {
            std::uint32_t stack[64];
            int sp = 0;
            stack[sp++] = 0;
            while (sp > 0) {
                const Node& n = nodes_[stack[--sp]];
                const math::Aabb3 box(n.bmin, n.bmax);
                const auto entry = box.intersectRay(origin, dir, best.t);
                if (!entry) continue; // whole box is farther than the current best, or missed
                if (n.count > 0) {    // leaf
                    for (std::uint32_t k = 0; k < n.count; ++k) {
                        const std::uint32_t tri = order_[n.start + k];
                        float t, u, v;
                        ++tested;
                        if (detail::bvhRayTri(origin, dir, triA_[tri], triB_[tri], triC_[tri], best.t, t, u, v)) {
                            best.hit = true;
                            best.t = t;
                            best.triangle = tri;
                            best.u = u;
                            best.v = v;
                        }
                    }
                } else { // internal: push the farther child first so the nearer pops (and prunes) first
                    const std::uint32_t l = n.left, r = n.right;
                    const auto le = nodes_[l].entry(origin, dir, best.t);
                    const auto re = nodes_[r].entry(origin, dir, best.t);
                    if (le && re) {
                        if (*le <= *re) { stack[sp++] = r; stack[sp++] = l; }
                        else { stack[sp++] = l; stack[sp++] = r; }
                    } else if (le) {
                        stack[sp++] = l;
                    } else if (re) {
                        stack[sp++] = r;
                    }
                }
            }
        }
        if (trianglesTested) *trianglesTested = tested;
        if (best.hit) best.point = origin + dir * best.t;
        return best;
    }

    // Any hit of the ray within (0, maxDist) — the early-out a shadow/occlusion ray wants. Returns as soon as one
    // triangle blocks the ray, so it visits fewer triangles than intersect on average.
    bool occluded(const math::vec3& origin, const math::vec3& dir, float maxDist,
                  std::size_t* trianglesTested = nullptr) const {
        std::size_t tested = 0;
        bool blocked = false;
        if (!nodes_.empty()) {
            std::uint32_t stack[64];
            int sp = 0;
            stack[sp++] = 0;
            while (sp > 0 && !blocked) {
                const Node& n = nodes_[stack[--sp]];
                const math::Aabb3 box(n.bmin, n.bmax);
                if (!box.intersectRay(origin, dir, maxDist)) continue;
                if (n.count > 0) {
                    for (std::uint32_t k = 0; k < n.count && !blocked; ++k) {
                        const std::uint32_t tri = order_[n.start + k];
                        float t, u, v;
                        ++tested;
                        if (detail::bvhRayTri(origin, dir, triA_[tri], triB_[tri], triC_[tri], maxDist, t, u, v))
                            blocked = true;
                    }
                } else {
                    stack[sp++] = n.left;
                    stack[sp++] = n.right;
                }
            }
        }
        if (trianglesTested) *trianglesTested = tested;
        return blocked;
    }

    // Closest point ON the mesh surface to `query` — the "snap to surface / how deep am I" query, the same
    // answer as render::closestPointOnMesh but pruned through the BVH: a node whose box is already farther than
    // the best point found is skipped whole. Best-first (nearer child visited first) so the bound tightens early.
    // Returns the surface point, unsigned distance, winning triangle, and that triangle's face normal.
    MeshPointHit closestPoint(const math::vec3& query, std::size_t* trianglesTested = nullptr) const {
        MeshPointHit best;
        float bestD2 = std::numeric_limits<float>::infinity();
        std::size_t tested = 0;
        if (!nodes_.empty()) {
            std::uint32_t stack[64];
            int sp = 0;
            stack[sp++] = 0;
            while (sp > 0) {
                const Node& n = nodes_[stack[--sp]];
                if (aabbDist2(n.bmin, n.bmax, query) >= bestD2) continue; // whole box is farther than best
                if (n.count > 0) {                                        // leaf
                    for (std::uint32_t k = 0; k < n.count; ++k) {
                        const std::uint32_t tri = order_[n.start + k];
                        ++tested;
                        const math::vec3 cp =
                            math::closestPointOnTriangle(query, triA_[tri], triB_[tri], triC_[tri]);
                        const math::vec3 d = cp - query;
                        const float d2 = math::dot(d, d);
                        if (d2 < bestD2) {
                            bestD2 = d2;
                            best.valid = true;
                            best.point = cp;
                            best.triangle = tri;
                        }
                    }
                } else { // internal: visit the nearer child first so the bound tightens before the farther one
                    const float dl = aabbDist2(nodes_[n.left].bmin, nodes_[n.left].bmax, query);
                    const float dr = aabbDist2(nodes_[n.right].bmin, nodes_[n.right].bmax, query);
                    if (dl <= dr) { stack[sp++] = n.right; stack[sp++] = n.left; }
                    else { stack[sp++] = n.left; stack[sp++] = n.right; }
                }
            }
        }
        if (trianglesTested) *trianglesTested = tested;
        if (best.valid) {
            best.distance = std::sqrt(bestD2);
            const math::vec3 nrm =
                math::cross(triB_[best.triangle] - triA_[best.triangle], triC_[best.triangle] - triA_[best.triangle]);
            const float l = std::sqrt(math::dot(nrm, nrm));
            best.normal = l > 1e-20f ? nrm / l : math::vec3(0.0f);
        }
        return best;
    }

private:
    // Squared distance from `q` to the axis-aligned box [lo, hi] (0 if inside).
    static float aabbDist2(const math::vec3& lo, const math::vec3& hi, const math::vec3& q) {
        float s = 0.0f;
        for (int a = 0; a < 3; ++a) {
            const float v = q[a];
            if (v < lo[a]) { const float e = lo[a] - v; s += e * e; }
            else if (v > hi[a]) { const float e = v - hi[a]; s += e * e; }
        }
        return s;
    }

    struct Node {
        math::vec3 bmin{0.0f};
        math::vec3 bmax{0.0f};
        std::uint32_t left = 0;  // index of left child when internal (count == 0)
        std::uint32_t right = 0; // index of right child when internal (count == 0)
        std::uint32_t start = 0; // first triangle in order_ when leaf
        std::uint32_t count = 0; // triangle count when leaf; 0 marks an internal node

        std::optional<float> entry(const math::vec3& o, const math::vec3& d, float tMax) const {
            return math::Aabb3(bmin, bmax).intersectRay(o, d, tMax);
        }
    };

    static math::vec3 vpos(const shapes::MeshData& m, std::uint32_t i) {
        return math::vec3(m.vertices[i].px, m.vertices[i].py, m.vertices[i].pz);
    }

    void triBounds(std::uint32_t tri, math::vec3& lo, math::vec3& hi) const {
        const math::vec3& a = triA_[tri];
        const math::vec3& b = triB_[tri];
        const math::vec3& c = triC_[tri];
        lo = math::vec3(std::min(a.x, std::min(b.x, c.x)), std::min(a.y, std::min(b.y, c.y)),
                        std::min(a.z, std::min(b.z, c.z)));
        hi = math::vec3(std::max(a.x, std::max(b.x, c.x)), std::max(a.y, std::max(b.y, c.y)),
                        std::max(a.z, std::max(b.z, c.z)));
    }

    // Build a node covering order_[begin, end) and return its index. Recurses; leaf when <= kLeaf triangles.
    std::uint32_t buildRange(std::size_t begin, std::size_t end, std::vector<math::vec3>& centroid) {
        const std::uint32_t self = static_cast<std::uint32_t>(nodes_.size());
        nodes_.push_back(Node{});

        // Bounds of this node (over triangle AABBs) and of the centroids (for the split axis).
        math::vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
        math::vec3 clo(1e30f, 1e30f, 1e30f), chi(-1e30f, -1e30f, -1e30f);
        for (std::size_t i = begin; i < end; ++i) {
            math::vec3 tlo, thi;
            triBounds(order_[i], tlo, thi);
            lo = math::vec3(std::min(lo.x, tlo.x), std::min(lo.y, tlo.y), std::min(lo.z, tlo.z));
            hi = math::vec3(std::max(hi.x, thi.x), std::max(hi.y, thi.y), std::max(hi.z, thi.z));
            const math::vec3& ct = centroid[order_[i]];
            clo = math::vec3(std::min(clo.x, ct.x), std::min(clo.y, ct.y), std::min(clo.z, ct.z));
            chi = math::vec3(std::max(chi.x, ct.x), std::max(chi.y, ct.y), std::max(chi.z, ct.z));
        }

        const std::size_t n = end - begin;
        constexpr std::size_t kLeaf = 4;
        if (n <= kLeaf) {
            nodes_[self].bmin = lo;
            nodes_[self].bmax = hi;
            nodes_[self].start = static_cast<std::uint32_t>(begin);
            nodes_[self].count = static_cast<std::uint32_t>(n);
            return self;
        }

        // Split on the longest centroid axis at its midpoint (median-ish; robust and allocation-free).
        const math::vec3 ext = chi - clo;
        int axis = 0;
        if (ext.y > ext.x) axis = 1;
        if (ext.z > (axis == 0 ? ext.x : ext.y)) axis = 2;
        const float mid = (clo[axis] + chi[axis]) * 0.5f;

        std::size_t l = begin, r = end;
        while (l < r) {
            if (centroid[order_[l]][axis] < mid) {
                ++l;
            } else {
                --r;
                std::swap(order_[l], order_[r]);
            }
        }
        std::size_t split = l;
        // Degenerate split (all centroids on one side): fall back to a median count split so we always make
        // progress instead of recursing forever.
        if (split == begin || split == end) split = begin + n / 2;

        const std::uint32_t leftIdx = buildRange(begin, split, centroid);
        const std::uint32_t rightIdx = buildRange(split, end, centroid);
        nodes_[self].bmin = lo;
        nodes_[self].bmax = hi;
        nodes_[self].left = leftIdx;
        nodes_[self].right = rightIdx;
        nodes_[self].count = 0;
        return self;
    }

    std::vector<Node> nodes_;
    std::vector<std::uint32_t> order_; // triangle indices grouped by leaf
    std::vector<math::vec3> triA_, triB_, triC_;
};

} // namespace maz::render
