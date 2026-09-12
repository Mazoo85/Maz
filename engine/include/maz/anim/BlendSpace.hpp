#pragma once

#include "maz/math/Math.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::anim {

// Animation blend spaces — Godot's AnimationTree BlendSpace1D / BlendSpace2D. A blend space places
// animations (referenced here by an integer `id`, e.g. a clip index) at positions in a 1-D or 2-D
// parameter plane; querying a point returns the small set of animations to mix and the weight of each
// (summing to 1). Feed those weights + the sampled poses into anim::blendPosesWeighted to get one
// blended pose. Pure geometry — no GPU, no clips — so it unit-tests headlessly and stays deterministic.

// 1-D blend space: samples on a line, blended linearly between the two neighbours (Godot BlendSpace1D).
class BlendSpace1D {
public:
    struct Weight {
        int id;
        float weight;
    };

    // Add a sample at parameter `pos` carrying payload `id`. Points are kept sorted by pos.
    void addPoint(float pos, int id) {
        Point p{pos, id};
        auto it = m_pts.begin();
        while (it != m_pts.end() && it->pos < pos) {
            ++it;
        }
        m_pts.insert(it, p);
    }

    size_t size() const { return m_pts.size(); }

    // Weights for query `x`: one sample when x is at/beyond an end (clamped), otherwise the two
    // straddling samples with linear weights. Empty when the space has no points.
    std::vector<Weight> weights(float x) const {
        std::vector<Weight> out;
        if (m_pts.empty()) {
            return out;
        }
        if (x <= m_pts.front().pos) {
            out.push_back({m_pts.front().id, 1.0f});
            return out;
        }
        if (x >= m_pts.back().pos) {
            out.push_back({m_pts.back().id, 1.0f});
            return out;
        }
        for (size_t i = 1; i < m_pts.size(); ++i) {
            if (x <= m_pts[i].pos) {
                const float t0 = m_pts[i - 1].pos, t1 = m_pts[i].pos;
                const float u = (t1 > t0) ? (x - t0) / (t1 - t0) : 0.0f;
                // Exactly on a sample -> just that sample; otherwise the two neighbours.
                if (u <= 0.0f) {
                    out.push_back({m_pts[i - 1].id, 1.0f});
                } else if (u >= 1.0f) {
                    out.push_back({m_pts[i].id, 1.0f});
                } else {
                    out.push_back({m_pts[i - 1].id, 1.0f - u});
                    out.push_back({m_pts[i].id, u});
                }
                return out;
            }
        }
        out.push_back({m_pts.back().id, 1.0f});
        return out;
    }

private:
    struct Point {
        float pos;
        int id;
    };
    std::vector<Point> m_pts;
};

// 2-D blend space: samples in a plane, blended by barycentric weights over a triangulation the caller
// supplies (Godot auto-triangulates; here triangles are added explicitly, which is exact and testable).
// A query inside a triangle returns its three corners with barycentric weights; a query outside every
// triangle is clamped to the nearest triangle (barycentric coords clamped to >= 0 and renormalized).
class BlendSpace2D {
public:
    struct Weight {
        int id;
        float weight;
    };

    // Add a sample point; returns its index (use it when adding triangles).
    int addPoint(math::vec2 pos, int id) {
        m_pts.push_back(Point{pos, id});
        return static_cast<int>(m_pts.size() - 1);
    }
    // Add a blend triangle by point indices (any winding).
    void addTriangle(int a, int b, int c) { m_tris.push_back({a, b, c}); }

    size_t pointCount() const { return m_pts.size(); }
    size_t triangleCount() const { return m_tris.size(); }

    std::vector<Weight> weights(math::vec2 p) const {
        std::vector<Weight> out;
        if (m_pts.empty()) {
            return out;
        }
        if (m_tris.empty()) {
            out.push_back({m_pts[0].id, 1.0f});
            return out;
        }
        // Prefer a triangle that actually contains p; otherwise keep the one p is "least outside" of.
        float bestExcess = 1e30f;
        std::array<float, 3> bestBary{1.0f, 0.0f, 0.0f};
        const std::array<int, 3>* bestTri = &m_tris[0];
        for (const std::array<int, 3>& tri : m_tris) {
            float u, v, w;
            barycentric(p, m_pts[static_cast<size_t>(tri[0])].pos,
                        m_pts[static_cast<size_t>(tri[1])].pos,
                        m_pts[static_cast<size_t>(tri[2])].pos, u, v, w);
            const float excess = negPart(u) + negPart(v) + negPart(w);
            if (excess <= 1e-6f) { // inside this triangle
                bestTri = &tri;
                bestBary = {u, v, w};
                bestExcess = 0.0f;
                break;
            }
            if (excess < bestExcess) {
                bestExcess = excess;
                bestBary = {u, v, w};
                bestTri = &tri;
            }
        }
        // Clamp negatives (projects an outside point onto the triangle) and renormalize to sum 1.
        float u = bestBary[0] < 0.0f ? 0.0f : bestBary[0];
        float v = bestBary[1] < 0.0f ? 0.0f : bestBary[1];
        float w = bestBary[2] < 0.0f ? 0.0f : bestBary[2];
        const float s = u + v + w;
        if (s > 1e-8f) {
            u /= s;
            v /= s;
            w /= s;
        } else {
            u = 1.0f;
            v = 0.0f;
            w = 0.0f;
        }
        out.push_back({m_pts[static_cast<size_t>((*bestTri)[0])].id, u});
        out.push_back({m_pts[static_cast<size_t>((*bestTri)[1])].id, v});
        out.push_back({m_pts[static_cast<size_t>((*bestTri)[2])].id, w});
        return out;
    }

    // Barycentric coordinates (u,v,w) of p in triangle (a,b,c); u for a, v for b, w for c.
    static void barycentric(math::vec2 p, math::vec2 a, math::vec2 b, math::vec2 c, float& u, float& v,
                            float& w) {
        const math::vec2 v0 = b - a, v1 = c - a, v2 = p - a;
        const float d00 = glm::dot(v0, v0);
        const float d01 = glm::dot(v0, v1);
        const float d11 = glm::dot(v1, v1);
        const float d20 = glm::dot(v2, v0);
        const float d21 = glm::dot(v2, v1);
        const float denom = d00 * d11 - d01 * d01;
        if (std::fabs(denom) < 1e-12f) { // degenerate triangle
            u = 1.0f;
            v = 0.0f;
            w = 0.0f;
            return;
        }
        v = (d11 * d20 - d01 * d21) / denom;
        w = (d00 * d21 - d01 * d20) / denom;
        u = 1.0f - v - w;
    }

private:
    static float negPart(float x) { return x < 0.0f ? -x : 0.0f; }

    struct Point {
        math::vec2 pos;
        int id;
    };
    std::vector<Point> m_pts;
    std::vector<std::array<int, 3>> m_tris;
};

} // namespace maz::anim
