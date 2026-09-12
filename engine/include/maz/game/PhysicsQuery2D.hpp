#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::game {

// 2D physics-space queries — the "ask the world a spatial question" side of a 2D physics engine
// (Godot's PhysicsDirectSpaceState2D): cast a RAY and get the first collider it hits, test whether a
// POINT lands inside any collider, and cast a bounded SEGMENT. These are the primitives behind
// line-of-sight checks, hitscan weapons, ground/wall probes, and mouse picking — Maz had rigid-body
// dynamics + contact generation (Physics2D) but no way to *query* the collider set without stepping the
// simulation. This is pure geometry against static shape descriptions, so it has no renderer or
// simulation dependency and unit-tests exhaustively.
//
// Shapes are circles or ORIENTED boxes (a box with a rotation), matching Physics2D's Body2D shapes.
// Every query takes a 32-bit collision MASK; a shape is only considered when `shape.layer & mask`
// is non-zero (Godot collision_mask semantics), so callers can probe "only walls" or "only enemies".

struct QueryShape2D {
    enum Kind { Circle, Box };
    int kind = Circle;
    math::vec2 pos{0.0f, 0.0f};       // center
    float radius = 0.5f;              // Circle
    math::vec2 half{0.5f, 0.5f};      // Box half-extents
    float angle = 0.0f;               // Box orientation, radians CCW
    uint32_t layer = 0xffffffffu;     // which layers this shape occupies (tested against a query mask)
    int id = -1;                      // user handle returned on a hit
};

struct RayHit2D {
    bool hit = false;
    float t = 0.0f;                   // distance along the (normalized) ray direction to the hit
    math::vec2 point{0.0f, 0.0f};     // world-space contact point
    math::vec2 normal{0.0f, 0.0f};    // surface normal at the hit, pointing back toward the ray origin
    int index = -1;                   // index into the queried shapes array
    int id = -1;                      // hit shape's user id
    explicit operator bool() const { return hit; }
};

namespace detail {

// Ray (origin O, unit dir D) vs circle (center C, radius r), within [0, tMax]. On hit, writes the
// entry distance to `t` and the outward normal to `n`. An origin already inside the circle hits at t=0.
inline bool rayCircle(const math::vec2& O, const math::vec2& D, const math::vec2& C, float r, float tMax,
                      float& t, math::vec2& n) {
    const math::vec2 m = O - C;
    const float c = m.x * m.x + m.y * m.y - r * r;
    if (c <= 0.0f) { // origin inside (or on) the circle
        t = 0.0f;
        const float len = std::sqrt(m.x * m.x + m.y * m.y);
        n = len > 1e-6f ? math::vec2(m.x / len, m.y / len) : math::vec2(-D.x, -D.y);
        return true;
    }
    const float b = m.x * D.x + m.y * D.y;
    if (b > 0.0f) {
        return false; // origin outside and pointing away
    }
    const float disc = b * b - c;
    if (disc < 0.0f) {
        return false;
    }
    const float th = -b - std::sqrt(disc);
    if (th < 0.0f || th > tMax) {
        return false;
    }
    t = th;
    const math::vec2 p = O + D * th;
    const math::vec2 d = p - C;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    n = len > 1e-6f ? math::vec2(d.x / len, d.y / len) : math::vec2(-D.x, -D.y);
    return true;
}

// Ray vs oriented box: rotate the ray into the box's local frame (angle = 0, centered at origin) and
// run a slab test against [-half, half]; the normal comes from whichever local axis bounded the entry,
// rotated back to world. An origin already inside the box hits at t=0.
inline bool rayBox(const math::vec2& O, const math::vec2& D, const math::vec2& C, const math::vec2& half,
                   float angle, float tMax, float& t, math::vec2& n) {
    const float ca = std::cos(angle), sa = std::sin(angle);
    // World->local is rotation by -angle.
    const math::vec2 rel = O - C;
    const math::vec2 lo(rel.x * ca + rel.y * sa, -rel.x * sa + rel.y * ca);
    const math::vec2 ld(D.x * ca + D.y * sa, -D.x * sa + D.y * ca);

    // Inside test: local origin within the box.
    if (std::fabs(lo.x) <= half.x && std::fabs(lo.y) <= half.y) {
        t = 0.0f;
        n = math::vec2(-D.x, -D.y);
        return true;
    }

    float tmin = 0.0f, tmax = tMax;
    math::vec2 localN(0.0f, 0.0f);
    const float lod[2] = {ld.x, ld.y};
    const float lop[2] = {lo.x, lo.y};
    const float h[2] = {half.x, half.y};
    for (int a = 0; a < 2; ++a) {
        if (std::fabs(lod[a]) < 1e-8f) {
            if (lop[a] < -h[a] || lop[a] > h[a]) {
                return false; // parallel to this slab and outside it
            }
            continue;
        }
        const float inv = 1.0f / lod[a];
        float t1 = (-h[a] - lop[a]) * inv;
        float t2 = (h[a] - lop[a]) * inv;
        float sign = -1.0f;
        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
            sign = 1.0f;
        }
        if (t1 > tmin) {
            tmin = t1;
            localN = (a == 0) ? math::vec2(sign, 0.0f) : math::vec2(0.0f, sign);
        }
        if (t2 < tmax) {
            tmax = t2;
        }
        if (tmin > tmax) {
            return false;
        }
    }
    if (tmin < 0.0f || tmin > tMax) {
        return false;
    }
    t = tmin;
    // Rotate the local normal back to world (rotation by +angle).
    n = math::vec2(localN.x * ca - localN.y * sa, localN.x * sa + localN.y * ca);
    return true;
}

} // namespace detail

// Does `p` lie inside `s`?  (Godot intersect_point per-shape test.)
inline bool pointInShape(const math::vec2& p, const QueryShape2D& s) {
    if (s.kind == QueryShape2D::Circle) {
        const math::vec2 d = p - s.pos;
        return d.x * d.x + d.y * d.y <= s.radius * s.radius;
    }
    const float ca = std::cos(s.angle), sa = std::sin(s.angle);
    const math::vec2 rel = p - s.pos;
    const math::vec2 local(rel.x * ca + rel.y * sa, -rel.x * sa + rel.y * ca);
    return std::fabs(local.x) <= s.half.x && std::fabs(local.y) <= s.half.y;
}

// Cast a ray from `origin` in direction `dir` (need not be normalized) up to `maxDist`, returning the
// NEAREST hit among `shapes` whose layer intersects `mask`. Godot PhysicsDirectSpaceState2D.intersect_ray.
inline RayHit2D queryRay(const math::vec2& origin, const math::vec2& dir,
                         const std::vector<QueryShape2D>& shapes, float maxDist = 1e30f,
                         uint32_t mask = 0xffffffffu) {
    RayHit2D best;
    const float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (dl < 1e-8f) {
        return best;
    }
    const math::vec2 D(dir.x / dl, dir.y / dl);
    float bestT = maxDist;
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        const QueryShape2D& s = shapes[i];
        if ((s.layer & mask) == 0u) {
            continue;
        }
        float t = 0.0f;
        math::vec2 n(0.0f, 0.0f);
        bool h = false;
        if (s.kind == QueryShape2D::Circle) {
            h = detail::rayCircle(origin, D, s.pos, s.radius, bestT, t, n);
        } else {
            h = detail::rayBox(origin, D, s.pos, s.half, s.angle, bestT, t, n);
        }
        if (h && t <= bestT) {
            bestT = t;
            best.hit = true;
            best.t = t;
            best.point = origin + D * t;
            best.normal = n;
            best.index = static_cast<int>(i);
            best.id = s.id;
        }
    }
    return best;
}

// Cast a bounded segment from `a` to `b`. Convenience wrapper over queryRay with maxDist = |b - a|.
inline RayHit2D querySegment(const math::vec2& a, const math::vec2& b,
                             const std::vector<QueryShape2D>& shapes, uint32_t mask = 0xffffffffu) {
    const math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    return queryRay(a, d, shapes, len, mask);
}

// Collect the indices of every shape containing `p` (whose layer intersects `mask`). Godot intersect_point.
inline std::vector<int> queryPoint(const math::vec2& p, const std::vector<QueryShape2D>& shapes,
                                   uint32_t mask = 0xffffffffu) {
    std::vector<int> out;
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        if ((shapes[i].layer & mask) != 0u && pointInShape(p, shapes[i])) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

} // namespace maz::game
