#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cstddef>
#include <vector>

// maz::math general polygon boolean operations (Greiner–Hormann) — intersection, union and difference of
// two ARBITRARY simple polygons, including CONCAVE ones, producing possibly several output contours. This is
// the real Clipper-style boolean the engine's existing clipPolygonConvex (Geometry2D.hpp, Sutherland–Hodgman)
// explicitly leaves out of scope: that one only clips against a *convex* window, while this handles concave
// subject and concave clip and returns the full multi-contour result. Use it for destructible-terrain
// carving, merging painted regions, computing overlap area between two swept shapes, visibility/coverage
// masks and vector-boolean authoring tools. Godot exposes this via Geometry2D.clip_polygons /
// intersect_polygons / merge_polygons (its own Clipper backend); this is the header-only, std-only,
// deterministic equivalent for the maz engine.
//
// Inputs are simple polygons given as CCW or CW rings (they are internally normalised to CCW). The classic
// Greiner–Hormann algorithm assumes GENERIC position — no vertex of one polygon lying exactly on an edge of
// the other and no collinear overlapping edges. Such degeneracies are out of scope here (nudge a coordinate
// to resolve them), matching the original 1998 algorithm. Results are returned as a list of contours; holes
// are emitted as separate, oppositely-wound contours (standard even–odd fill).
namespace maz::math {

namespace detail {

enum class BoolOp { Intersection, Union, Difference };

struct GhNode {
    vec2 p{0.0f, 0.0f};
    float alpha = 0.0f; // parameter along the owning edge (intersection nodes only)
    bool intersect = false;
    bool entry = false;    // entry (true) / exit (false) classification for the trace
    bool processed = false;
    int next = -1;
    int prev = -1;
    int neighbour = -1; // paired intersection node in the other polygon's list
};

// Signed area (shoelace); positive for CCW (Y-up).
inline float ghSignedArea(const std::vector<vec2>& poly) {
    float a = 0.0f;
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        const vec2& p = poly[i];
        const vec2& q = poly[(i + 1) % n];
        a += p.x * q.y - q.x * p.y;
    }
    return 0.5f * a;
}

// Even–odd point-in-polygon on a single ring (winding-independent).
inline bool ghPointInRing(const vec2& pt, const std::vector<vec2>& ring) {
    bool inside = false;
    const std::size_t n = ring.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const vec2& a = ring[i];
        const vec2& b = ring[j];
        if (((a.y > pt.y) != (b.y > pt.y)) &&
            (pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y) + a.x)) {
            inside = !inside;
        }
    }
    return inside;
}

// Interior crossing of segment P1->P2 with Q1->Q2. Writes the two edge parameters; returns false for
// parallel edges or when the crossing falls on (or past) an endpoint of either segment.
inline bool ghSegCross(const vec2& P1, const vec2& P2, const vec2& Q1, const vec2& Q2,
                       float& aP, float& aQ) {
    const vec2 dp = P2 - P1;
    const vec2 dq = Q2 - Q1;
    const float denom = dp.x * dq.y - dp.y * dq.x;
    if (denom > -1e-9f && denom < 1e-9f) {
        return false; // parallel
    }
    const vec2 d = Q1 - P1;
    aP = (d.x * dq.y - d.y * dq.x) / denom;
    aQ = (d.x * dp.y - d.y * dp.x) / denom;
    const float e = 1e-6f;
    return aP > e && aP < 1.0f - e && aQ > e && aQ < 1.0f - e;
}

// Splice `newIdx` into the doubly linked list on the edge starting at `startIdx`, keeping intersection
// nodes ordered by ascending alpha (they all precede the edge's original end vertex).
inline void ghInsertSorted(std::vector<GhNode>& pool, int startIdx, int newIdx) {
    int prev = startIdx;
    int nxt = pool[static_cast<std::size_t>(prev)].next;
    while (pool[static_cast<std::size_t>(nxt)].intersect &&
           pool[static_cast<std::size_t>(nxt)].alpha < pool[static_cast<std::size_t>(newIdx)].alpha) {
        prev = nxt;
        nxt = pool[static_cast<std::size_t>(prev)].next;
    }
    pool[static_cast<std::size_t>(newIdx)].prev = prev;
    pool[static_cast<std::size_t>(newIdx)].next = nxt;
    pool[static_cast<std::size_t>(prev)].next = newIdx;
    pool[static_cast<std::size_t>(nxt)].prev = newIdx;
}

inline std::vector<vec2> ghToCcw(const std::vector<vec2>& poly) {
    if (ghSignedArea(poly) < 0.0f) {
        return std::vector<vec2>(poly.rbegin(), poly.rend());
    }
    return poly;
}

// Core Greiner–Hormann. Subject and clip are normalised to CCW; for a difference the clip ring is then
// REVERSED to CW so the shared trace machinery (which follows `next` on entry and `prev` on exit) carves
// the clip out of the subject with the correct winding. Entry/exit is marked with the intersection
// convention (entry = outside) for intersection/difference and the union convention (entry = inside) for
// union.
inline std::vector<std::vector<vec2>> ghClip(const std::vector<vec2>& subjectIn,
                                             const std::vector<vec2>& clipIn, BoolOp op) {
    const std::vector<vec2> subject = ghToCcw(subjectIn);
    const std::vector<vec2> clip = ghToCcw(clipIn);
    const int ns = static_cast<int>(subject.size());
    const int nc = static_cast<int>(clip.size());
    if (ns < 3 || nc < 3) {
        return {};
    }
    // Entry/exit convention per ring, both CCW: an intersection is marked "entry" (follow `next`) when the
    // ring is crossing OUTSIDE the other ("entry on inside" == false), or "entry" when INSIDE ("... on
    // inside" == true). Intersection A∩B follows each ring's arcs that are inside the other (entry on
    // outside, both false). Union A∪B follows the outside arcs (entry on inside, both true). Difference A−B
    // keeps the subject's OUTSIDE arcs (entry on inside, true) and stitches in the clip's INSIDE arcs
    // traversed backwards (entry on outside, false) — carving the clip out of the subject.
    const bool subjEntryOnInside = (op == BoolOp::Union) || (op == BoolOp::Difference);
    const bool clipEntryOnInside = (op == BoolOp::Union);

    std::vector<GhNode> pool;
    pool.reserve(static_cast<std::size_t>((ns + nc) * 3));
    std::vector<int> subjOrig(static_cast<std::size_t>(ns));
    std::vector<int> clipOrig(static_cast<std::size_t>(nc));

    auto addRing = [&pool](const std::vector<vec2>& ring, std::vector<int>& orig) {
        const int n = static_cast<int>(ring.size());
        const int base = static_cast<int>(pool.size());
        for (int i = 0; i < n; ++i) {
            GhNode nd;
            nd.p = ring[static_cast<std::size_t>(i)];
            nd.next = base + (i + 1) % n;
            nd.prev = base + (i - 1 + n) % n;
            pool.push_back(nd);
            orig[static_cast<std::size_t>(i)] = base + i;
        }
    };
    addRing(subject, subjOrig);
    addRing(clip, clipOrig);

    // --- Phase 1: find and insert all intersection vertices. ---
    int intersections = 0;
    for (int i = 0; i < ns; ++i) {
        const vec2 A = subject[static_cast<std::size_t>(i)];
        const vec2 B = subject[static_cast<std::size_t>((i + 1) % ns)];
        for (int j = 0; j < nc; ++j) {
            const vec2 C = clip[static_cast<std::size_t>(j)];
            const vec2 D = clip[static_cast<std::size_t>((j + 1) % nc)];
            float aP = 0.0f, aQ = 0.0f;
            if (!ghSegCross(A, B, C, D, aP, aQ)) {
                continue;
            }
            const vec2 ip = A + (B - A) * aP;
            const int si = static_cast<int>(pool.size());
            GhNode sNode;
            sNode.p = ip;
            sNode.alpha = aP;
            sNode.intersect = true;
            pool.push_back(sNode);
            const int ci = static_cast<int>(pool.size());
            GhNode cNode;
            cNode.p = ip;
            cNode.alpha = aQ;
            cNode.intersect = true;
            pool.push_back(cNode);
            pool[static_cast<std::size_t>(si)].neighbour = ci;
            pool[static_cast<std::size_t>(ci)].neighbour = si;
            ghInsertSorted(pool, subjOrig[static_cast<std::size_t>(i)], si);
            ghInsertSorted(pool, clipOrig[static_cast<std::size_t>(j)], ci);
            ++intersections;
        }
    }

    // --- No intersections: containment / disjoint special cases. ---
    if (intersections == 0) {
        const bool subjInClip = ghPointInRing(subject[0], clip);
        const bool clipInSubj = ghPointInRing(clip[0], subject);
        std::vector<std::vector<vec2>> out;
        if (op == BoolOp::Union) {
            if (subjInClip) {
                out.push_back(ghToCcw(clipIn));
            } else if (clipInSubj) {
                out.push_back(subject);
            } else {
                out.push_back(subject);
                out.push_back(ghToCcw(clipIn));
            }
        } else if (op == BoolOp::Difference) {
            if (subjInClip) {
                // subject entirely inside clip -> everything removed -> empty
            } else if (clipInSubj) {
                out.push_back(subject);
                out.push_back(std::vector<vec2>(clip.rbegin(), clip.rend())); // clip carves a CW hole
            } else {
                out.push_back(subject);
            }
        } else { // intersection
            if (subjInClip) {
                out.push_back(subject);
            } else if (clipInSubj) {
                out.push_back(clip);
            }
        }
        return out;
    }

    // --- Phase 2: mark entry/exit on each list. ---
    {
        const bool inside = ghPointInRing(subject[0], clip);
        bool entry = subjEntryOnInside ? inside : !inside;
        int cur = subjOrig[0];
        const int start = cur;
        do {
            if (pool[static_cast<std::size_t>(cur)].intersect) {
                pool[static_cast<std::size_t>(cur)].entry = entry;
                entry = !entry;
            }
            cur = pool[static_cast<std::size_t>(cur)].next;
        } while (cur != start);
    }
    {
        const bool inside = ghPointInRing(clip[0], subject);
        bool entry = clipEntryOnInside ? inside : !inside;
        int cur = clipOrig[0];
        const int start = cur;
        do {
            if (pool[static_cast<std::size_t>(cur)].intersect) {
                pool[static_cast<std::size_t>(cur)].entry = entry;
                entry = !entry;
            }
            cur = pool[static_cast<std::size_t>(cur)].next;
        } while (cur != start);
    }

    // --- Phase 3: trace result contours. ---
    std::vector<std::vector<vec2>> result;
    for (std::size_t startIdx = 0; startIdx < pool.size(); ++startIdx) {
        if (!pool[startIdx].intersect || pool[startIdx].processed) {
            continue;
        }
        std::vector<vec2> contour;
        int cur = static_cast<int>(startIdx);
        const int start = cur;
        contour.push_back(pool[static_cast<std::size_t>(cur)].p);
        do {
            pool[static_cast<std::size_t>(cur)].processed = true;
            const int nb = pool[static_cast<std::size_t>(cur)].neighbour;
            if (nb >= 0) {
                pool[static_cast<std::size_t>(nb)].processed = true;
            }
            if (pool[static_cast<std::size_t>(cur)].entry) {
                do {
                    cur = pool[static_cast<std::size_t>(cur)].next;
                    contour.push_back(pool[static_cast<std::size_t>(cur)].p);
                } while (!pool[static_cast<std::size_t>(cur)].intersect);
            } else {
                do {
                    cur = pool[static_cast<std::size_t>(cur)].prev;
                    contour.push_back(pool[static_cast<std::size_t>(cur)].p);
                } while (!pool[static_cast<std::size_t>(cur)].intersect);
            }
            cur = pool[static_cast<std::size_t>(cur)].neighbour;
        } while (cur != start);

        // Drop the trailing duplicate of the start point and any consecutive duplicates.
        std::vector<vec2> cleaned;
        cleaned.reserve(contour.size());
        for (const vec2& p : contour) {
            if (cleaned.empty()) {
                cleaned.push_back(p);
            } else {
                const vec2 d = p - cleaned.back();
                if (d.x * d.x + d.y * d.y > 1e-12f) {
                    cleaned.push_back(p);
                }
            }
        }
        if (cleaned.size() >= 2) {
            const vec2 d = cleaned.front() - cleaned.back();
            if (d.x * d.x + d.y * d.y <= 1e-12f) {
                cleaned.pop_back();
            }
        }
        if (cleaned.size() >= 3) {
            result.push_back(std::move(cleaned));
        }
    }
    return result;
}

} // namespace detail

// Intersection A ∩ B — the region covered by BOTH polygons. Returns one contour per connected overlap
// piece; empty when the polygons do not overlap.
inline std::vector<std::vector<vec2>> polygonIntersection(const std::vector<vec2>& a,
                                                          const std::vector<vec2>& b) {
    return detail::ghClip(a, b, detail::BoolOp::Intersection);
}

// Union A ∪ B — the region covered by EITHER polygon. Overlapping inputs merge into a single contour;
// disjoint inputs come back as two contours.
inline std::vector<std::vector<vec2>> polygonUnion(const std::vector<vec2>& a,
                                                   const std::vector<vec2>& b) {
    return detail::ghClip(a, b, detail::BoolOp::Union);
}

// Difference A − B — the part of A NOT covered by B. If B carves a hole out of A's interior the hole is
// returned as a second, oppositely-wound contour (even–odd fill).
inline std::vector<std::vector<vec2>> polygonDifference(const std::vector<vec2>& a,
                                                        const std::vector<vec2>& b) {
    return detail::ghClip(a, b, detail::BoolOp::Difference);
}

} // namespace maz::math
