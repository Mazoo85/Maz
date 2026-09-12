#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::game {

// 2D kinematic character controller — Godot's CharacterBody2D.move_and_slide, the single most-used
// movement primitive for platformers and top-down games. Rigid bodies (Physics2D) are driven by forces;
// a *kinematic* character is driven directly by a velocity and must not tunnel through or stick into the
// static world. moveAndSlide sweeps an axis-aligned body along its motion, stops at the first contact,
// and SLIDES the leftover motion along the surface — repeating for a few iterations so a body can round a
// corner or run along a wall in one call. Each contact is classified against an `up` direction into
// floor / wall / ceiling (via a max floor angle), so gameplay can ask is_on_floor()/is_on_wall(). Pure
// geometry over a list of static AABBs — no allocation beyond the solids the caller owns — so it
// unit-tests exactly and drives a deterministic golden. (Discrete resolution against oriented/rotated
// shapes and moving platforms remain future work; this is AABB-vs-AABB swept collision.)

struct Aabb2 {
    math::vec2 min{0.0f, 0.0f};
    math::vec2 max{0.0f, 0.0f};

    static Aabb2 fromCenterHalf(math::vec2 center, math::vec2 half) {
        return Aabb2{center - half, center + half};
    }
    math::vec2 center() const { return (min + max) * 0.5f; }
    math::vec2 halfExtents() const { return (max - min) * 0.5f; }
    bool overlaps(const Aabb2& o) const {
        return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y;
    }
};

struct SweptHit {
    bool hit = false;
    float t = 1.0f;               // fraction of `motion` travelled before contact, in [0, 1]
    math::vec2 normal{0.0f, 0.0f}; // surface normal at the contact (axis-aligned)
};

// Sweep an AABB (center `pos`, half-extents `half`) along `motion` against one static box. Uses the
// Minkowski trick: expand the solid by the body's half-extents and cast the body CENTRE as a ray through
// the expanded box (slab test). Returns the earliest entry time and the face normal.
inline SweptHit sweptAabb(math::vec2 pos, math::vec2 half, math::vec2 motion, const Aabb2& solid) {
    SweptHit r;
    if (std::fabs(motion.x) < 1e-9f && std::fabs(motion.y) < 1e-9f) {
        return r; // not moving -> no swept contact
    }
    const Aabb2 e{solid.min - half, solid.max + half};
    float tEntry = 0.0f;
    float tExit = 1.0f;
    math::vec2 normal{0.0f, 0.0f};
    for (int a = 0; a < 2; ++a) {
        const float o = pos[a];
        const float d = motion[a];
        const float lo = e.min[a];
        const float hi = e.max[a];
        if (std::fabs(d) < 1e-9f) {
            if (o < lo || o > hi) {
                return r; // moving parallel to this slab and already outside it
            }
        } else {
            float t1 = (lo - o) / d;
            float t2 = (hi - o) / d;
            float sign = -1.0f;
            if (t1 > t2) {
                std::swap(t1, t2);
                sign = 1.0f;
            }
            if (t1 > tEntry) {
                tEntry = t1;
                normal = math::vec2(0.0f, 0.0f);
                normal[a] = sign;
            }
            if (t2 < tExit) {
                tExit = t2;
            }
            if (tEntry > tExit) {
                return r;
            }
        }
    }
    if (tEntry > tExit || tEntry < 0.0f || tEntry > 1.0f) {
        return r;
    }
    r.hit = true;
    r.t = tEntry;
    r.normal = normal;
    return r;
}

struct SlideResult {
    math::vec2 position{0.0f, 0.0f};
    math::vec2 velocity{0.0f, 0.0f}; // velocity with blocked components removed (post-slide)
    bool onFloor = false;
    bool onWall = false;
    bool onCeiling = false;
    math::vec2 floorNormal{0.0f, 0.0f};
    int slides = 0; // how many contacts were resolved this call
};

// Move a body (centre `pos`, half-extents `half`) by `velocity * dt` through `solids`, sliding along
// contacts. `up` is the up direction (screen-space default (0,-1)); a contact whose normal is within
// `floorMaxAngleRad` of `up` is floor, within that of `-up` is ceiling, else wall.
inline SlideResult moveAndSlide(math::vec2 pos, math::vec2 half, math::vec2 velocity, float dt,
                                const std::vector<Aabb2>& solids,
                                math::vec2 up = math::vec2(0.0f, -1.0f),
                                float floorMaxAngleRad = 0.7853982f, int maxSlides = 4) {
    SlideResult res;
    res.velocity = velocity;
    math::vec2 motion = velocity * dt;
    const float floorCos = std::cos(floorMaxAngleRad);
    const float skin = 0.001f;

    for (int iter = 0; iter < maxSlides; ++iter) {
        if (std::fabs(motion.x) < 1e-9f && std::fabs(motion.y) < 1e-9f) {
            break;
        }
        SweptHit best;
        float bestT = 2.0f;
        for (const Aabb2& s : solids) {
            const SweptHit h = sweptAabb(pos, half, motion, s);
            if (h.hit && h.t < bestT) {
                best = h;
                bestT = h.t;
            }
        }
        if (!best.hit) {
            pos += motion;
            break;
        }
        ++res.slides;
        pos += motion * best.t;      // advance to just before contact
        pos += best.normal * skin;   // nudge out of the surface to avoid re-catching it

        const math::vec2 n = best.normal;
        const float d = n.x * up.x + n.y * up.y;
        if (d >= floorCos) {
            res.onFloor = true;
            res.floorNormal = n;
        } else if (d <= -floorCos) {
            res.onCeiling = true;
        } else {
            res.onWall = true;
        }

        // Slide the leftover motion (and the reported velocity) along the surface.
        math::vec2 remaining = motion * (1.0f - best.t);
        remaining -= n * (remaining.x * n.x + remaining.y * n.y);
        motion = remaining;
        res.velocity -= n * (res.velocity.x * n.x + res.velocity.y * n.y);
    }

    res.position = pos;
    return res;
}

} // namespace maz::game
