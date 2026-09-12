#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

namespace maz::game {

// General weighted-graph A* over arbitrary 3D points — Godot's AStar3D, the 3D twin of AStar2D (M169).
// Place points at any position with any integer id, connect them however you like (flight lanes, jump
// links, teleporters, 3D waypoint webs), then query the least-cost route. Each point's `weightScale`
// multiplies the cost of moving INTO it; edges may be one-way. Traversal cost between connected points
// is their Euclidean distance × the destination's weight scale; the heuristic is straight-line distance
// (admissible when every weight >= 1). Ordered map + ordered neighbour sets -> identical graphs give
// identical paths. Deterministic, header-only, GPU-free — unit-tests exactly.
//
// Honest scope vs Godot's AStar3D: covers the graph, weights, one-/two-way links, id/point paths, and
// closest-point / closest-position-in-segment queries. It does NOT override _compute_cost /
// _estimate_cost via subclassing (the cost model is fixed to weighted Euclidean).

class AStar3D {
public:
    static constexpr int64_t kInvalidId = -1;

    // --- points ------------------------------------------------------------------------------------
    void addPoint(int64_t id, math::vec3 position, float weightScale = 1.0f) {
        if (weightScale < 0.0f) {
            weightScale = 0.0f;
        }
        Point& p = m_points[id];
        p.position = position;
        p.weightScale = weightScale;
    }

    bool hasPoint(int64_t id) const { return m_points.find(id) != m_points.end(); }
    std::size_t pointCount() const { return m_points.size(); }

    std::vector<int64_t> getPointIds() const {
        std::vector<int64_t> ids;
        ids.reserve(m_points.size());
        for (const auto& kv : m_points) {
            ids.push_back(kv.first);
        }
        return ids;
    }

    math::vec3 getPointPosition(int64_t id) const {
        auto it = m_points.find(id);
        return it == m_points.end() ? math::vec3(0.0f) : it->second.position;
    }

    void setPointPosition(int64_t id, math::vec3 position) {
        auto it = m_points.find(id);
        if (it != m_points.end()) {
            it->second.position = position;
        }
    }

    float getPointWeightScale(int64_t id) const {
        auto it = m_points.find(id);
        return it == m_points.end() ? 0.0f : it->second.weightScale;
    }

    void setPointWeightScale(int64_t id, float weightScale) {
        auto it = m_points.find(id);
        if (it != m_points.end()) {
            it->second.weightScale = weightScale < 0.0f ? 0.0f : weightScale;
        }
    }

    void removePoint(int64_t id) {
        auto it = m_points.find(id);
        if (it == m_points.end()) {
            return;
        }
        m_points.erase(it);
        for (auto& kv : m_points) {
            kv.second.neighbors.erase(id);
        }
    }

    void clear() { m_points.clear(); }

    // --- connections -------------------------------------------------------------------------------
    void connectPoints(int64_t id, int64_t toId, bool bidirectional = true) {
        auto a = m_points.find(id);
        auto b = m_points.find(toId);
        if (a == m_points.end() || b == m_points.end() || id == toId) {
            return;
        }
        a->second.neighbors.insert(toId);
        if (bidirectional) {
            b->second.neighbors.insert(id);
        }
    }

    void disconnectPoints(int64_t id, int64_t toId, bool bidirectional = true) {
        auto a = m_points.find(id);
        if (a != m_points.end()) {
            a->second.neighbors.erase(toId);
        }
        if (bidirectional) {
            auto b = m_points.find(toId);
            if (b != m_points.end()) {
                b->second.neighbors.erase(id);
            }
        }
    }

    bool arePointsConnected(int64_t id, int64_t toId) const {
        auto a = m_points.find(id);
        if (a == m_points.end()) {
            return false;
        }
        return a->second.neighbors.find(toId) != a->second.neighbors.end();
    }

    std::vector<int64_t> getPointConnections(int64_t id) const {
        std::vector<int64_t> out;
        auto it = m_points.find(id);
        if (it != m_points.end()) {
            out.assign(it->second.neighbors.begin(), it->second.neighbors.end());
        }
        return out;
    }

    // --- queries -----------------------------------------------------------------------------------
    int64_t getClosestPoint(math::vec3 position) const {
        int64_t best = kInvalidId;
        float bestD2 = 0.0f;
        for (const auto& kv : m_points) {
            const math::vec3 d = kv.second.position - position;
            const float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
            if (best == kInvalidId || d2 < bestD2) {
                best = kv.first;
                bestD2 = d2;
            }
        }
        return best;
    }

    math::vec3 getClosestPositionInSegment(math::vec3 position) const {
        bool found = false;
        math::vec3 best(0.0f);
        float bestD2 = 0.0f;
        for (const auto& kv : m_points) {
            const math::vec3 a = kv.second.position;
            for (int64_t nid : kv.second.neighbors) {
                auto nit = m_points.find(nid);
                if (nit == m_points.end()) {
                    continue;
                }
                const math::vec3 b = nit->second.position;
                const math::vec3 p = closestOnSegment(position, a, b);
                const math::vec3 d = p - position;
                const float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                if (!found || d2 < bestD2) {
                    found = true;
                    best = p;
                    bestD2 = d2;
                }
            }
        }
        if (found) {
            return best;
        }
        const int64_t cp = getClosestPoint(position);
        return cp == kInvalidId ? math::vec3(0.0f) : getPointPosition(cp);
    }

    // --- pathfinding -------------------------------------------------------------------------------
    std::vector<int64_t> getIdPath(int64_t fromId, int64_t toId) const {
        std::vector<int64_t> path;
        if (!hasPoint(fromId) || !hasPoint(toId)) {
            return path;
        }
        if (fromId == toId) {
            path.push_back(fromId);
            return path;
        }

        const math::vec3 goalPos = m_points.at(toId).position;
        std::unordered_map<int64_t, float> gScore;
        std::unordered_map<int64_t, int64_t> came;
        std::unordered_map<int64_t, char> closed;
        gScore[fromId] = 0.0f;

        using Node = std::pair<float, int64_t>;
        auto cmp = [](const Node& l, const Node& r) {
            if (l.first != r.first) {
                return l.first > r.first;
            }
            return l.second > r.second;
        };
        std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);
        open.push({heuristic(m_points.at(fromId).position, goalPos), fromId});

        while (!open.empty()) {
            const int64_t current = open.top().second;
            open.pop();
            if (current == toId) {
                return reconstruct(came, current);
            }
            if (closed[current]) {
                continue;
            }
            closed[current] = 1;

            const Point& cp = m_points.at(current);
            for (int64_t n : cp.neighbors) {
                if (closed[n]) {
                    continue;
                }
                const Point& np = m_points.at(n);
                const float step = distance(cp.position, np.position) * np.weightScale;
                const float tentative = gScore[current] + step;
                auto git = gScore.find(n);
                if (git == gScore.end() || tentative < git->second) {
                    gScore[n] = tentative;
                    came[n] = current;
                    open.push({tentative + heuristic(np.position, goalPos), n});
                }
            }
        }
        return path;
    }

    std::vector<math::vec3> getPointPath(int64_t fromId, int64_t toId) const {
        const std::vector<int64_t> ids = getIdPath(fromId, toId);
        std::vector<math::vec3> pts;
        pts.reserve(ids.size());
        for (int64_t id : ids) {
            pts.push_back(m_points.at(id).position);
        }
        return pts;
    }

private:
    struct Point {
        math::vec3 position{0.0f, 0.0f, 0.0f};
        float weightScale = 1.0f;
        std::set<int64_t> neighbors;
    };

    static float distance(math::vec3 a, math::vec3 b) {
        const math::vec3 d = b - a;
        return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    }

    static float heuristic(math::vec3 a, math::vec3 goal) { return distance(a, goal); }

    static math::vec3 closestOnSegment(math::vec3 p, math::vec3 a, math::vec3 b) {
        const math::vec3 ab = b - a;
        const float len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
        if (len2 < 1e-12f) {
            return a;
        }
        float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y + (p.z - a.z) * ab.z) / len2;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return math::vec3(a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t);
    }

    std::vector<int64_t> reconstruct(const std::unordered_map<int64_t, int64_t>& came,
                                     int64_t current) const {
        std::vector<int64_t> path;
        path.push_back(current);
        auto it = came.find(current);
        while (it != came.end()) {
            current = it->second;
            path.push_back(current);
            it = came.find(current);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    std::map<int64_t, Point> m_points;
};

} // namespace maz::game
