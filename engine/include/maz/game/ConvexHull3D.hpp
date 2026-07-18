#pragma once

#include "maz/math/Math.hpp"

#include <cstddef>
#include <set>
#include <utility>
#include <vector>

// maz::game 3D convex hull — the incremental hull of a point cloud, the geometry behind Godot's
// ConvexPolygonShape3D (create a tight convex collider from an arbitrary mesh's vertices) and useful
// for bounding volumes and simplified proxies. Given points, buildConvexHull returns the outward-
// facing triangular faces of their convex hull plus the subset of points that lie on it (interior
// points are dropped). The algorithm seeds a non-degenerate tetrahedron, then for each remaining
// point removes the faces it can "see" and stitches new faces from the horizon edges to it — every
// face is oriented outward against a fixed interior point, so the result is a watertight, convex,
// correctly-wound mesh. Pure math (no renderer/physics), deterministic, unit-tested; returns
// valid=false for fewer than 4 points or a degenerate (collinear/coplanar) set.
namespace maz::game {

struct HullFace {
    int a = 0;
    int b = 0;
    int c = 0;              // indices into the input point list, wound CCW when viewed from outside
    math::vec3 normal{0.0f, 0.0f, 0.0f};
};

struct ConvexHull3D {
    std::vector<int> vertices; // indices of input points that lie on the hull (unique)
    std::vector<HullFace> faces;
    bool valid = false;
};

namespace detail {

inline float hullDot(const math::vec3& a, const math::vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline math::vec3 hullCross(const math::vec3& a, const math::vec3& b) {
    return math::vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float hullLen(const math::vec3& a) { return std::sqrt(hullDot(a, a)); }

} // namespace detail

inline ConvexHull3D buildConvexHull(const std::vector<math::vec3>& pts, float eps = 1e-5f) {
    using detail::hullCross;
    using detail::hullDot;
    using detail::hullLen;

    ConvexHull3D hull;
    const int n = static_cast<int>(pts.size());
    if (n < 4) {
        return hull; // need a tetrahedron
    }

    // Signedness-clean accessor into the point list.
    auto P = [&](int idx) -> const math::vec3& { return pts[static_cast<std::size_t>(idx)]; };

    // --- Seed a non-degenerate tetrahedron via successively higher-dimensional extremes. ---
    int i0 = 0;
    int i1 = -1;
    float best = eps;
    for (int i = 1; i < n; ++i) {
        const float d = hullLen(P(i) - P(i0));
        if (d > best) {
            best = d;
            i1 = i;
        }
    }
    if (i1 < 0) {
        return hull; // all points coincide
    }

    int i2 = -1;
    best = eps;
    const math::vec3 e01 = P(i1) - P(i0);
    for (int i = 0; i < n; ++i) {
        if (i == i0 || i == i1) {
            continue;
        }
        const float area = hullLen(hullCross(e01, P(i) - P(i0)));
        if (area > best) {
            best = area;
            i2 = i;
        }
    }
    if (i2 < 0) {
        return hull; // all points collinear
    }

    int i3 = -1;
    best = eps;
    const math::vec3 planeN = hullCross(P(i1) - P(i0), P(i2) - P(i0));
    for (int i = 0; i < n; ++i) {
        if (i == i0 || i == i1 || i == i2) {
            continue;
        }
        const float dist = std::fabs(hullDot(planeN, P(i) - P(i0)));
        if (dist > best) {
            best = dist;
            i3 = i;
        }
    }
    if (i3 < 0) {
        return hull; // all points coplanar
    }

    // Interior reference point: centroid of the seed tetra is strictly inside the final hull.
    const math::vec3 interior = (P(i0) + P(i1) + P(i2) + P(i3)) * 0.25f;

    auto makeFace = [&](int a, int b, int c) {
        HullFace f;
        f.a = a;
        f.b = b;
        f.c = c;
        math::vec3 nrm = hullCross(P(b) - P(a), P(c) - P(a));
        const float l = hullLen(nrm);
        if (l > 0.0f) {
            nrm = nrm * (1.0f / l);
        }
        // Orient outward: normal should point away from the interior point.
        if (hullDot(nrm, P(a) - interior) < 0.0f) {
            std::swap(f.b, f.c);
            nrm = nrm * -1.0f;
        }
        f.normal = nrm;
        return f;
    };

    std::vector<HullFace> faces;
    faces.push_back(makeFace(i0, i1, i2));
    faces.push_back(makeFace(i0, i1, i3));
    faces.push_back(makeFace(i0, i2, i3));
    faces.push_back(makeFace(i1, i2, i3));

    auto seen = [&](int idx) { return idx == i0 || idx == i1 || idx == i2 || idx == i3; };

    // --- Incrementally add every remaining point. ---
    for (int i = 0; i < n; ++i) {
        if (seen(i)) {
            continue;
        }
        const math::vec3& p = P(i);

        // Faces this point can see (is in front of, beyond eps).
        std::vector<char> visible(faces.size(), 0);
        bool any = false;
        for (std::size_t fi = 0; fi < faces.size(); ++fi) {
            const HullFace& f = faces[fi];
            if (hullDot(f.normal, p - P(f.a)) > eps) {
                visible[fi] = 1;
                any = true;
            }
        }
        if (!any) {
            continue; // point is inside the current hull
        }

        // Directed edges of visible faces; a horizon edge (u,v) has no (v,u) among visible faces.
        std::set<std::pair<int, int>> visibleEdges;
        for (std::size_t fi = 0; fi < faces.size(); ++fi) {
            if (!visible[fi]) {
                continue;
            }
            const HullFace& f = faces[fi];
            visibleEdges.emplace(f.a, f.b);
            visibleEdges.emplace(f.b, f.c);
            visibleEdges.emplace(f.c, f.a);
        }

        std::vector<std::pair<int, int>> horizon;
        for (const auto& e : visibleEdges) {
            if (visibleEdges.find({e.second, e.first}) == visibleEdges.end()) {
                horizon.push_back(e);
            }
        }

        // Drop visible faces, then stitch a fan from each horizon edge to the new point.
        std::vector<HullFace> kept;
        kept.reserve(faces.size());
        for (std::size_t fi = 0; fi < faces.size(); ++fi) {
            if (!visible[fi]) {
                kept.push_back(faces[fi]);
            }
        }
        for (const auto& e : horizon) {
            kept.push_back(makeFace(e.first, e.second, i));
        }
        faces.swap(kept);
    }

    // --- Collect unique hull vertices. ---
    std::set<int> vset;
    for (const HullFace& f : faces) {
        vset.insert(f.a);
        vset.insert(f.b);
        vset.insert(f.c);
    }
    hull.vertices.assign(vset.begin(), vset.end());
    hull.faces = std::move(faces);
    hull.valid = true;
    return hull;
}

} // namespace maz::game
