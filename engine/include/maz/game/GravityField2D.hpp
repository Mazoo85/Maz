#pragma once

#include "maz/math/Math.hpp"
#include "maz/math/Rect2.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace maz::game {

// Area gravity fields — Godot's Area2D gravity override. A rectangular zone can change the gravity a body
// feels while inside it: a DIRECTIONAL field pushes a fixed way (wind, an updraft, a sideways conveyor of
// force), a POINT field pulls toward (or pushes from) a centre with inverse-square falloff (a planet, a
// black hole, a magnet). Zones carry a PRIORITY and a mode — REPLACE (this zone's gravity overrides what
// lower zones set, like Godot's "Replace") or ADD (accumulate on top, like Godot's "Combine"). `gravityAt`
// walks the zones low-priority-first and returns the final gravity vector at a point, starting from the
// world's default. Pure geometry + vector math, header-only, deterministic — it unit-tests exactly and a
// fixed-step sim drives a golden.

enum class GravityType { Directional, Point };
enum class GravityMode { Replace, Add };

struct GravityArea2D {
    math::Rect2 region;                       // the zone bounds (a body is affected while inside)
    GravityType type = GravityType::Directional;
    GravityMode mode = GravityMode::Replace;
    int priority = 0;                         // higher priority is applied later (wins under Replace)

    // Directional: `direction` (need not be unit) scaled to `strength`.
    math::vec2 direction{0.0f, 1.0f};
    float strength = 980.0f;

    // Point: pull toward `center`; with unitDistance > 0 the pull is `strength` AT that distance and falls
    // off as inverse-square; unitDistance <= 0 means a constant `strength` regardless of distance.
    math::vec2 center{0.0f, 0.0f};
    float unitDistance = 0.0f;
};

// The gravity vector a single zone contributes at `p`.
inline math::vec2 zoneGravity(const GravityArea2D& a, math::vec2 p) {
    if (a.type == GravityType::Directional) {
        const float len = std::sqrt(a.direction.x * a.direction.x + a.direction.y * a.direction.y);
        if (len < 1e-6f) {
            return math::vec2(0.0f, 0.0f);
        }
        return math::vec2(a.direction.x / len * a.strength, a.direction.y / len * a.strength);
    }
    // Point field: direction toward the centre.
    math::vec2 d(a.center.x - p.x, a.center.y - p.y);
    const float dist = std::sqrt(d.x * d.x + d.y * d.y);
    if (dist < 1e-6f) {
        return math::vec2(0.0f, 0.0f); // at the centre there is no direction
    }
    float mag = a.strength;
    if (a.unitDistance > 0.0f) {
        const float ratio = a.unitDistance / dist;
        mag = a.strength * ratio * ratio; // inverse-square falloff, == strength at unitDistance
    }
    return math::vec2(d.x / dist * mag, d.y / dist * mag);
}

// The final gravity at `p`: start from `base`, then apply every zone containing `p` in ascending priority
// order — REPLACE zones overwrite the accumulated vector, ADD zones sum onto it. Ties keep input order.
inline math::vec2 gravityAt(const std::vector<GravityArea2D>& areas, math::vec2 p, math::vec2 base) {
    // Collect the affecting zones with their original index, then stable-sort by priority.
    std::vector<std::size_t> order;
    order.reserve(areas.size());
    for (std::size_t i = 0; i < areas.size(); ++i) {
        if (areas[i].region.hasPoint(p)) {
            order.push_back(i);
        }
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t l, std::size_t r) { return areas[l].priority < areas[r].priority; });

    math::vec2 g = base;
    for (std::size_t idx : order) {
        const GravityArea2D& a = areas[idx];
        const math::vec2 zg = zoneGravity(a, p);
        if (a.mode == GravityMode::Replace) {
            g = zg;
        } else {
            g = math::vec2(g.x + zg.x, g.y + zg.y);
        }
    }
    return g;
}

} // namespace maz::game
