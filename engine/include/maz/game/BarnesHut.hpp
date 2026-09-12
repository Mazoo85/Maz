#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game::barnesHutAccelerations — the Barnes-Hut quadtree method for n-body force fields. Computing the
// gravitational (or any inverse-square attraction) force on every body from every other is naively O(n^2) —
// hopeless past a few thousand bodies. Barnes-Hut buckets the bodies into a quadtree, records each node's
// total mass and centre of mass, and when a whole cluster is far enough away (its width / distance is below
// a threshold theta) treats it as one lumped mass instead of visiting each body — dropping the cost to
// O(n log n). This is what powers galaxy/gravity toys, large-scale particle attraction, dust/debris fields,
// and swarm forces at a scale a per-pair loop can't reach. Godot ships no n-body solver. The approximation
// is tunable: theta = 0 opens every node and reproduces the EXACT all-pairs force; larger theta trades
// accuracy for speed. Softening avoids the singularity when two bodies coincide. Header-only, std-only,
// deterministic.
namespace maz::game {

struct GravBody {
    math::vec2 pos{0.0f, 0.0f};
    float mass = 1.0f;
};

namespace detail {

struct BhNode {
    math::vec2 center{0.0f, 0.0f}; // cell centre
    float halfSize = 0.0f;         // half the cell width
    float mass = 0.0f;             // total mass in the cell
    math::vec2 com{0.0f, 0.0f};    // centre of mass
    bool leaf = true;
    int child[4] = {-1, -1, -1, -1};
    std::vector<int> bodies; // populated only on leaves
};

// Recursively partition `idx` into a quadtree; returns the node index in `nodes`.
inline int bhBuild(std::vector<BhNode>& nodes, const std::vector<GravBody>& b, std::vector<int> idx,
                   math::vec2 center, float halfSize, int depth) {
    const int self = static_cast<int>(nodes.size());
    nodes.push_back(BhNode{});
    {
        BhNode n;
        n.center = center;
        n.halfSize = halfSize;
        float m = 0.0f;
        math::vec2 com{0.0f, 0.0f};
        for (int i : idx) {
            m += b[static_cast<std::size_t>(i)].mass;
            com.x += b[static_cast<std::size_t>(i)].pos.x * b[static_cast<std::size_t>(i)].mass;
            com.y += b[static_cast<std::size_t>(i)].pos.y * b[static_cast<std::size_t>(i)].mass;
        }
        if (m > 0.0f) {
            com.x /= m;
            com.y /= m;
        }
        n.mass = m;
        n.com = com;
        nodes[static_cast<std::size_t>(self)] = n;
    }
    if (idx.size() <= 1 || depth >= 32) {
        nodes[static_cast<std::size_t>(self)].leaf = true;
        nodes[static_cast<std::size_t>(self)].bodies = std::move(idx);
        return self;
    }
    // Split into four quadrants.
    std::vector<int> quad[4];
    for (int i : idx) {
        const math::vec2 p = b[static_cast<std::size_t>(i)].pos;
        const int q = (p.x >= center.x ? 1 : 0) | (p.y >= center.y ? 2 : 0);
        quad[q].push_back(i);
    }
    const float h = halfSize * 0.5f;
    nodes[static_cast<std::size_t>(self)].leaf = false;
    for (int q = 0; q < 4; ++q) {
        if (quad[q].empty()) {
            continue;
        }
        const math::vec2 c{center.x + (q & 1 ? h : -h), center.y + (q & 2 ? h : -h)};
        const int cn = bhBuild(nodes, b, std::move(quad[q]), c, h, depth + 1);
        nodes[static_cast<std::size_t>(self)].child[q] = cn;
    }
    return self;
}

} // namespace detail

// Acceleration on every body from inverse-square attraction: a_i = g * sum_j!=i m_j (p_j-p_i)/(|r|^2+e^2)^1.5.
// `theta` is the opening angle (0 = exact all-pairs; typical 0.5-1.0). `softening` avoids the r->0 blowup.
inline std::vector<math::vec2> barnesHutAccelerations(const std::vector<GravBody>& bodies, float theta,
                                                      float g, float softening) {
    const std::size_t n = bodies.size();
    std::vector<math::vec2> acc(n, math::vec2{0.0f, 0.0f});
    if (n < 2) {
        return acc;
    }
    // Bounding square.
    math::vec2 mn = bodies[0].pos, mx = bodies[0].pos;
    for (const GravBody& b : bodies) {
        mn.x = std::fmin(mn.x, b.pos.x);
        mn.y = std::fmin(mn.y, b.pos.y);
        mx.x = std::fmax(mx.x, b.pos.x);
        mx.y = std::fmax(mx.y, b.pos.y);
    }
    const math::vec2 center{0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y)};
    float half = 0.5f * std::fmax(mx.x - mn.x, mx.y - mn.y);
    if (half < 1e-6f) {
        half = 1.0f;
    }
    std::vector<detail::BhNode> nodes;
    nodes.reserve(n * 2);
    std::vector<int> all(n);
    for (std::size_t i = 0; i < n; ++i) {
        all[i] = static_cast<int>(i);
    }
    const int root = detail::bhBuild(nodes, bodies, std::move(all), center, half, 0);

    const float eps2 = softening * softening;
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec2 p = bodies[i].pos;
        math::vec2 a{0.0f, 0.0f};
        int stack[128];
        int sp = 0;
        stack[sp++] = root;
        while (sp > 0) {
            const detail::BhNode& node = nodes[static_cast<std::size_t>(stack[--sp])];
            if (node.mass <= 0.0f) {
                continue;
            }
            if (node.leaf) {
                for (int j : node.bodies) {
                    if (static_cast<std::size_t>(j) == i) {
                        continue;
                    }
                    const math::vec2 d{bodies[static_cast<std::size_t>(j)].pos.x - p.x,
                                       bodies[static_cast<std::size_t>(j)].pos.y - p.y};
                    const float r2 = d.x * d.x + d.y * d.y + eps2;
                    const float inv = 1.0f / (r2 * std::sqrt(r2));
                    const float f = g * bodies[static_cast<std::size_t>(j)].mass * inv;
                    a.x += d.x * f;
                    a.y += d.y * f;
                }
                continue;
            }
            const math::vec2 d{node.com.x - p.x, node.com.y - p.y};
            const float dist = std::sqrt(d.x * d.x + d.y * d.y);
            const float s = 2.0f * node.halfSize;
            if (dist > 1e-9f && s / dist < theta) {
                const float r2 = d.x * d.x + d.y * d.y + eps2;
                const float inv = 1.0f / (r2 * std::sqrt(r2));
                const float f = g * node.mass * inv;
                a.x += d.x * f;
                a.y += d.y * f;
            } else {
                for (int c = 0; c < 4; ++c) {
                    if (node.child[c] >= 0 && sp < 128) {
                        stack[sp++] = node.child[c];
                    }
                }
            }
        }
        acc[i] = a;
    }
    return acc;
}

} // namespace maz::game
