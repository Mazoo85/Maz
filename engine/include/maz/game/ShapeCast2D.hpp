#pragma once

#include "maz/game/PhysicsQuery2D.hpp"
#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::game {

// Swept-shape casting — continuous collision detection, the "move a shape and find the first thing it
// hits" query behind Godot's ShapeCast2D and PhysicsDirectSpaceState2D.cast_motion. PhysicsQuery2D
// already answers ZERO-radius questions (a ray, a segment, a point); this answers the FINITE-radius
// one: sweep a circle of radius `r` from A to B through a set of static colliders and report the first
// contact — the fraction of the motion travelled, the world contact point, the surface normal, and the
// caster's centre at that instant. That is what stops a fast projectile or a character-controller step
// from TUNNELLING through a thin wall in a single frame (a discrete overlap test at A and at B misses a
// wall that sits entirely between them). It is also the primitive under a "how far can I move before I
// touch something" probe.
//
// The obstacle set reuses PhysicsQuery2D's `QueryShape2D` (circles + oriented boxes) and its 32-bit
// collision MASK, so the same world description feeds rays, points, and now swept circles. The maths is
// the Minkowski sum: sweeping a circle of radius r against a shape is the same as sweeping a POINT (the
// circle's centre) against that shape GROWN by r — a circle of radius R+r, or a box rounded by r. So a
// circle-vs-circle sweep is a ray against an inflated circle, and a circle-vs-box sweep is a ray against
// the box's four r-offset faces plus four r-radius corner arcs. Pure geometry, header-only,
// deterministic — it unit-tests exactly (a hit leaves the caster exactly r from the surface) and drives
// a golden.
//
// Scope note (honest): this casts a CIRCLE (the common character/projectile probe). Casting an arbitrary
// oriented BOX or convex polygon along a motion — Godot's full ShapeCast2D with any CollisionShape2D —
// is the follow-up; so is returning Godot's separate safe/unsafe fractions for a start-in-contact shape
// (here a caster that already overlaps reports contact at fraction 0 with a push-out normal).

struct ShapeCastHit2D {
    bool hit = false;
    float fraction = 1.0f;            // 0..1 fraction of the motion travelled before first contact
    math::vec2 point{0.0f, 0.0f};     // world contact point on the obstacle surface
    math::vec2 normal{0.0f, 0.0f};    // unit surface normal, pointing back toward the caster
    math::vec2 safePos{0.0f, 0.0f};   // caster centre at first contact = from + motion * fraction
    int index = -1;                   // index into the queried shapes array (-1 if no hit)
    int id = -1;                      // hit shape's user id
    explicit operator bool() const { return hit; }
};

namespace detail {

// Swept circle (start P, unit dir D, radius r, max travel L) vs a circle (centre C, radius R). On hit,
// writes the entry distance along D to `t` and the outward surface normal (pointing toward the caster)
// to `n`. A caster already overlapping the target reports t = 0 with a push-out normal. Reduces to a ray
// vs the circle grown to R + r.
inline bool sweptCircleCircle(const math::vec2& P, const math::vec2& D, float r, const math::vec2& C,
                              float R, float L, float& t, math::vec2& n) {
    const math::vec2 m = P - C;
    const float sep2 = m.x * m.x + m.y * m.y;
    const float sum = R + r;
    if (sep2 <= sum * sum) { // already overlapping (or touching)
        t = 0.0f;
        const float len = std::sqrt(sep2);
        n = len > 1e-6f ? math::vec2(m.x / len, m.y / len) : math::vec2(-D.x, -D.y);
        return true;
    }
    return rayCircle(P, D, C, sum, L, t, n);
}

// Swept circle (start P, unit dir D, radius r, max travel L) vs an oriented box (centre C, half-extents
// `half`, orientation `angle`). Minkowski sum = the box's four faces pushed out by r joined by four
// quarter-circle corners of radius r. We test the ray against each candidate and keep the nearest entry.
// On hit, `t` is the entry distance and `n` the outward world normal (pointing toward the caster).
inline bool sweptCircleBox(const math::vec2& P, const math::vec2& D, float r, const math::vec2& C,
                           const math::vec2& half, float angle, float L, float& t, math::vec2& n) {
    const float ca = std::cos(angle), sa = std::sin(angle);
    // World -> local (rotate by -angle), box centred at the origin.
    const math::vec2 rel = P - C;
    const math::vec2 lo(rel.x * ca + rel.y * sa, -rel.x * sa + rel.y * ca);
    const math::vec2 ld(D.x * ca + D.y * sa, -D.x * sa + D.y * ca);

    // Start-overlap: closest point on the (un-grown) box to the caster centre, in local space.
    const float cx = std::fmax(-half.x, std::fmin(lo.x, half.x));
    const float cy = std::fmax(-half.y, std::fmin(lo.y, half.y));
    const math::vec2 dcl(lo.x - cx, lo.y - cy);
    const float dcl2 = dcl.x * dcl.x + dcl.y * dcl.y;
    if (dcl2 <= r * r) { // already overlapping
        t = 0.0f;
        math::vec2 ln;
        const float len = std::sqrt(dcl2);
        if (len > 1e-6f) {
            ln = math::vec2(dcl.x / len, dcl.y / len);
        } else {
            ln = math::vec2(-ld.x, -ld.y); // centre inside the box: push straight back
        }
        n = math::vec2(ln.x * ca - ln.y * sa, ln.x * sa + ln.y * ca);
        return true;
    }

    float best = L;
    math::vec2 bestN(0.0f, 0.0f);
    bool found = false;

    // Four faces: local axis a bounded at ±half[a], valid only where the tangential coordinate stays
    // within the face span [-half[other], +half[other]]. The face plane sits r outside the box.
    const float lop[2] = {lo.x, lo.y};
    const float ldp[2] = {ld.x, ld.y};
    const float hp[2] = {half.x, half.y};
    for (int a = 0; a < 2; ++a) {
        const int o = a ^ 1; // the other axis
        if (std::fabs(ldp[a]) < 1e-8f) {
            continue; // parallel to this pair of faces; corners/other axis cover it
        }
        for (int s = -1; s <= 1; s += 2) {
            const float plane = static_cast<float>(s) * (hp[a] + r);
            const float th = (plane - lop[a]) / ldp[a];
            if (th < 0.0f || th > best) {
                continue;
            }
            const float tang = lop[o] + ldp[o] * th; // tangential coordinate at the hit
            if (tang < -hp[o] || tang > hp[o]) {
                continue; // slid past the face's extent — a corner (below) handles it
            }
            best = th;
            bestN = (a == 0) ? math::vec2(static_cast<float>(s), 0.0f)
                             : math::vec2(0.0f, static_cast<float>(s));
            found = true;
        }
    }

    // Four corners: a circle of radius r centred at each box corner.
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            const math::vec2 corner(static_cast<float>(sx) * half.x, static_cast<float>(sy) * half.y);
            float th;
            math::vec2 cn;
            if (rayCircle(lo, ld, corner, r, best, th, cn) && th <= best) {
                best = th;
                bestN = cn; // rayCircle already returns the outward normal (corner -> hit point)
                found = true;
            }
        }
    }

    if (!found) {
        return false;
    }
    t = best;
    // Rotate the local normal back to world (rotation by +angle).
    n = math::vec2(bestN.x * ca - bestN.y * sa, bestN.x * sa + bestN.y * ca);
    return true;
}

} // namespace detail

// Sweep a circle of `radius` from `from` along `motion` (i.e. to `from + motion`) through `shapes` and
// return the FIRST contact. Only shapes whose `layer & mask` is non-zero are considered (Godot
// collision_mask semantics). If nothing is hit the result has `hit == false`, `fraction == 1`, and
// `safePos == from + motion`. A zero-length motion degenerates to a start-overlap test at `from`.
inline ShapeCastHit2D shapeCastCircle(const math::vec2& from, const math::vec2& motion, float radius,
                                      const std::vector<QueryShape2D>& shapes,
                                      uint32_t mask = 0xffffffffu) {
    ShapeCastHit2D out;
    const float L = std::sqrt(motion.x * motion.x + motion.y * motion.y);
    const math::vec2 D = L > 1e-8f ? math::vec2(motion.x / L, motion.y / L) : math::vec2(0.0f, 0.0f);

    float bestT = L; // in distance units along D; for L==0 this is 0 (only start-overlaps qualify)
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        const QueryShape2D& s = shapes[i];
        if ((s.layer & mask) == 0u) {
            continue;
        }
        float t = 0.0f;
        math::vec2 n(0.0f, 0.0f);
        bool hit = false;
        if (s.kind == QueryShape2D::Circle) {
            hit = detail::sweptCircleCircle(from, D, radius, s.pos, s.radius, bestT, t, n);
        } else {
            hit = detail::sweptCircleBox(from, D, radius, s.pos, s.half, s.angle, bestT, t, n);
        }
        if (hit && t <= bestT) {
            bestT = t;
            out.hit = true;
            out.index = static_cast<int>(i);
            out.id = s.id;
            out.normal = n;
            out.safePos = from + D * t;
            out.point = out.safePos - n * radius;
        }
    }

    if (out.hit) {
        out.fraction = L > 1e-8f ? bestT / L : 0.0f;
    } else {
        out.fraction = 1.0f;
        out.safePos = from + motion;
    }
    return out;
}

} // namespace maz::game
