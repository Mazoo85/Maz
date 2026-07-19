#pragma once

#include "maz/game/NavMesh.hpp"
#include "maz/math/Geometry2D.hpp"

#include <cmath>
#include <vector>

// maz::game 3D navigation mesh — the query side of Godot's NavigationServer3D / NavigationRegion3D. The
// walkable world is described by convex 3D polygons (floors, ramps, platforms) that share edges; `findPath`
// returns a smoothed list of 3D waypoints from a start to a goal across them. Because a walkable surface is
// effectively 2D, this projects every polygon to the ground (XZ) plane and reuses the tested 2D `NavMesh`
// (M87) for the corridor A* + funnel string-pulling — no duplicated pathfinding — then lifts each waypoint's
// height back onto the polygon it lands on, so a path correctly climbs ramps and steps. Deterministic and
// GPU-free, so it unit-tests headlessly; a full pipeline would bake these polygons from level geometry
// (Recast-style voxelization), which is a documented follow-up.
//
// Scope note (honest): pathfinding + height reconstruction over supplied convex walkable polygons projected
// on XZ. It does not bake the navmesh from raw geometry, handle overlapping multi-level surfaces at the same
// XZ, or do dynamic obstacle avoidance; those are follow-ups.
namespace maz::game {

class NavMesh3D {
public:
    // Add a convex walkable polygon (its vertices, CCW when viewed from above).
    void addPolygon(const std::vector<math::vec3>& verts) { m_polys.push_back(verts); }

    // Build the projected 2D navmesh + adjacency. Call once after adding all polygons.
    void build() {
        m_nav = NavMesh{};
        for (const auto& poly : m_polys) {
            std::vector<math::vec2> flat;
            flat.reserve(poly.size());
            for (const math::vec3& v : poly) flat.push_back(math::vec2{v.x, v.z});
            m_nav.addPolygon(flat);
        }
        m_nav.build();
    }

    // Find a smoothed 3D path from `start` to `goal`. Returns an empty path if unreachable. Waypoint heights
    // are snapped onto the walkable surface (the caller's start/goal Y is used only where no polygon covers
    // that XZ point).
    std::vector<math::vec3> findPath(const math::vec3& start, const math::vec3& goal) const {
        const std::vector<math::vec2> flat =
            m_nav.findPath(math::vec2{start.x, start.z}, math::vec2{goal.x, goal.z});
        std::vector<math::vec3> path;
        path.reserve(flat.size());
        for (std::size_t i = 0; i < flat.size(); ++i) {
            float fallback = start.y;
            if (i + 1 == flat.size()) fallback = goal.y;
            path.push_back(math::vec3{flat[i].x, sampleHeight(flat[i], fallback), flat[i].y});
        }
        return path;
    }

    // Surface height at an XZ point: the height of the walkable polygon covering it, or `fallback` if none.
    float sampleHeight(math::vec2 xz, float fallback = 0.0f) const {
        for (const auto& poly : m_polys) {
            if (poly.size() < 3) continue;
            std::vector<math::vec2> flat;
            flat.reserve(poly.size());
            for (const math::vec3& v : poly) flat.push_back(math::vec2{v.x, v.z});
            if (math::pointInPolygon(xz, flat)) return planeHeight(poly, xz);
        }
        return fallback;
    }

    std::size_t polygonCount() const { return m_polys.size(); }

private:
    // Height of the polygon's supporting plane at XZ = (p.x, p.y-as-z). Assumes the polygon is planar.
    static float planeHeight(const std::vector<math::vec3>& poly, math::vec2 p) {
        const math::vec3& a = poly[0];
        const math::vec3& b = poly[1];
        const math::vec3& c = poly[2];
        const math::vec3 n = glm::cross(b - a, c - a);
        if (std::fabs(n.y) < 1e-8f) return a.y; // near-vertical polygon
        return a.y - (n.x * (p.x - a.x) + n.z * (p.y - a.z)) / n.y;
    }

    std::vector<std::vector<math::vec3>> m_polys;
    NavMesh m_nav;
};

} // namespace maz::game
