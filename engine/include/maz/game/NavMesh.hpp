#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace maz::game {

// Navigation-mesh pathfinding: polygon-based navigation, the step up from a uniform grid (NavGrid)
// and the analogue of Godot's NavigationServer / NavigationPolygon. The walkable area is described by
// a set of CONVEX polygon cells that share edges; build() finds those shared edges (portals) to form
// a cell graph. findPath() then A*-searches the graph for the corridor of cells between two points and
// runs the "simple stupid funnel" algorithm over the corridor's portals to string-pull a short, smooth
// path that hugs corners — instead of the staircase a grid produces. Header-only, dependency-free
// (2D math only), so it unit-tests without a GPU.

struct NavPoly {
    std::vector<math::vec2> verts; // convex, counter-clockwise winding
};

class NavMesh {
public:
    // Add a convex polygon cell (CCW). Call build() once all cells are added.
    void addPolygon(const std::vector<math::vec2>& verts) { m_polys.push_back(NavPoly{verts}); }

    // Compute cell adjacency by matching shared edges (two polygons that share an undirected edge are
    // neighbors, and that edge is the portal between them).
    void build() {
        m_adj.assign(m_polys.size(), {});
        for (size_t i = 0; i < m_polys.size(); ++i) {
            for (size_t j = i + 1; j < m_polys.size(); ++j) {
                math::vec2 a, b;
                if (sharedEdge(m_polys[i], m_polys[j], a, b)) {
                    m_adj[i].push_back(Edge{j, a, b});
                    m_adj[j].push_back(Edge{i, a, b});
                }
            }
        }
    }

    size_t polyCount() const { return m_polys.size(); }
    const NavPoly& poly(size_t i) const { return m_polys[i]; }

    // Index of the cell containing p, or -1 (npos) if none.
    size_t cellAt(math::vec2 p) const {
        for (size_t i = 0; i < m_polys.size(); ++i)
            if (pointInPoly(m_polys[i], p)) return i;
        return kNone;
    }

    static constexpr size_t kNone = static_cast<size_t>(-1);

    // Smooth path from start to goal (inclusive). Empty if either point is outside the mesh or the
    // cells are disconnected.
    std::vector<math::vec2> findPath(math::vec2 start, math::vec2 goal) const {
        const size_t s = cellAt(start), g = cellAt(goal);
        if (s == kNone || g == kNone) return {};
        if (s == g) return {start, goal};

        const std::vector<size_t> corridor = aStarCells(s, g);
        if (corridor.empty()) return {};

        // Build the portal list along the corridor, oriented (left,right) for the funnel.
        std::vector<math::vec2> portalLeft, portalRight;
        portalLeft.push_back(start);
        portalRight.push_back(start);
        for (size_t k = 0; k + 1 < corridor.size(); ++k) {
            math::vec2 a, b;
            if (!portalBetween(corridor[k], corridor[k + 1], a, b)) return {};
            // Orient (left,right) relative to the travel direction from this cell to the next. The
            // funnel (Mononen's convention) wants the clockwise-side vertex as `left`: the endpoint
            // with a negative cross of (travelDir, vertex-fromCentroid).
            const math::vec2 fc = centroid(m_polys[corridor[k]]);
            const math::vec2 tc = centroid(m_polys[corridor[k + 1]]);
            const math::vec2 d = tc - fc;
            const float crossA = d.x * (a.y - fc.y) - d.y * (a.x - fc.x);
            if (crossA < 0.0f) {
                portalLeft.push_back(a);
                portalRight.push_back(b);
            } else {
                portalLeft.push_back(b);
                portalRight.push_back(a);
            }
        }
        portalLeft.push_back(goal);
        portalRight.push_back(goal);

        return funnel(portalLeft, portalRight);
    }

private:
    struct Edge {
        size_t to;
        math::vec2 a, b; // portal endpoints
    };

    std::vector<NavPoly> m_polys;
    std::vector<std::vector<Edge>> m_adj;

    static constexpr float kEps = 1e-4f;

    static float triArea2(math::vec2 a, math::vec2 b, math::vec2 c) {
        return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    }
    static bool near(math::vec2 a, math::vec2 b) {
        return std::fabs(a.x - b.x) < kEps && std::fabs(a.y - b.y) < kEps;
    }
    static float len(math::vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

    static math::vec2 centroid(const NavPoly& p) {
        math::vec2 c{0.0f, 0.0f};
        for (const math::vec2& v : p.verts) c += v;
        const float n = static_cast<float>(p.verts.size());
        return n > 0.0f ? math::vec2{c.x / n, c.y / n} : c;
    }

    // Point-in-convex-polygon (CCW): p is inside if it is left-of-or-on every directed edge.
    static bool pointInPoly(const NavPoly& poly, math::vec2 p) {
        const size_t n = poly.verts.size();
        if (n < 3) return false;
        for (size_t i = 0; i < n; ++i) {
            const math::vec2& a = poly.verts[i];
            const math::vec2& b = poly.verts[(i + 1) % n];
            if (triArea2(a, b, p) < -kEps) return false;
        }
        return true;
    }

    // Do two polygons share an (undirected) edge? If so return its endpoints.
    static bool sharedEdge(const NavPoly& p, const NavPoly& q, math::vec2& a, math::vec2& b) {
        const size_t np = p.verts.size(), nq = q.verts.size();
        for (size_t i = 0; i < np; ++i) {
            const math::vec2 p0 = p.verts[i], p1 = p.verts[(i + 1) % np];
            for (size_t j = 0; j < nq; ++j) {
                const math::vec2 q0 = q.verts[j], q1 = q.verts[(j + 1) % nq];
                if ((near(p0, q0) && near(p1, q1)) || (near(p0, q1) && near(p1, q0))) {
                    a = p0;
                    b = p1;
                    return true;
                }
            }
        }
        return false;
    }

    bool portalBetween(size_t from, size_t to, math::vec2& a, math::vec2& b) const {
        for (const Edge& e : m_adj[from]) {
            if (e.to == to) {
                a = e.a;
                b = e.b;
                return true;
            }
        }
        return false;
    }

    // A* over the cell graph; returns the sequence of cell indices from s to g (inclusive).
    std::vector<size_t> aStarCells(size_t s, size_t g) const {
        const size_t n = m_polys.size();
        const float inf = 1e30f;
        std::vector<float> gScore(n, inf), fScore(n, inf);
        std::vector<size_t> came(n, kNone);
        std::vector<char> closed(n, 0);
        const math::vec2 goalC = centroid(m_polys[g]);
        gScore[s] = 0.0f;
        fScore[s] = len(centroid(m_polys[s]) - goalC);

        while (true) {
            // Pick the open node with the lowest fScore (small graphs -> linear scan is fine).
            size_t cur = kNone;
            float best = inf;
            for (size_t i = 0; i < n; ++i) {
                if (!closed[i] && gScore[i] < inf && fScore[i] < best) {
                    best = fScore[i];
                    cur = i;
                }
            }
            if (cur == kNone) return {};
            if (cur == g) break;
            closed[cur] = 1;
            const math::vec2 curC = centroid(m_polys[cur]);
            for (const Edge& e : m_adj[cur]) {
                if (closed[e.to]) continue;
                const math::vec2 mid{(e.a.x + e.b.x) * 0.5f, (e.a.y + e.b.y) * 0.5f};
                const float tentative =
                    gScore[cur] + len(mid - curC) + len(centroid(m_polys[e.to]) - mid);
                if (tentative < gScore[e.to]) {
                    came[e.to] = cur;
                    gScore[e.to] = tentative;
                    fScore[e.to] = tentative + len(centroid(m_polys[e.to]) - goalC);
                }
            }
        }

        std::vector<size_t> path;
        for (size_t c = g; c != kNone; c = came[c]) path.push_back(c);
        // reverse
        for (size_t i = 0, j = path.size(); i < j / 2; ++i) {
            const size_t t = path[i];
            path[i] = path[j - 1 - i];
            path[j - 1 - i] = t;
        }
        return path;
    }

    // Simple stupid funnel (Mikko Mononen) over left/right portal chains.
    static std::vector<math::vec2> funnel(const std::vector<math::vec2>& left,
                                          const std::vector<math::vec2>& right) {
        std::vector<math::vec2> pts;
        const size_t n = left.size();
        if (n == 0) return pts;

        math::vec2 apex = left[0], portalL = left[0], portalR = right[0];
        size_t apexI = 0, leftI = 0, rightI = 0;
        pts.push_back(apex);

        for (size_t i = 1; i < n; ++i) {
            const math::vec2 l = left[i], r = right[i];

            // Update right vertex.
            if (triArea2(apex, portalR, r) <= 0.0f) {
                if (near(apex, portalR) || triArea2(apex, portalL, r) > 0.0f) {
                    portalR = r;
                    rightI = i;
                } else {
                    // Right over left -> insert left as a corner and restart from it.
                    if (!near(pts.back(), portalL)) pts.push_back(portalL);
                    apex = portalL;
                    apexI = leftI;
                    portalL = apex;
                    portalR = apex;
                    leftI = apexI;
                    rightI = apexI;
                    i = apexI;
                    continue;
                }
            }
            // Update left vertex.
            if (triArea2(apex, portalL, l) >= 0.0f) {
                if (near(apex, portalL) || triArea2(apex, portalR, l) < 0.0f) {
                    portalL = l;
                    leftI = i;
                } else {
                    if (!near(pts.back(), portalR)) pts.push_back(portalR);
                    apex = portalR;
                    apexI = rightI;
                    portalL = apex;
                    portalR = apex;
                    leftI = apexI;
                    rightI = apexI;
                    i = apexI;
                    continue;
                }
            }
        }
        if (pts.empty() || !near(pts.back(), left[n - 1])) pts.push_back(left[n - 1]);
        return pts;
    }
};

} // namespace maz::game
