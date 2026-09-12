#pragma once

#include "maz/math/Math.hpp" // vec2

#include <array>
#include <cmath>

// maz::math oriented 2D rectangle (OBB2) — a rectangle with a rotation, and the containment/overlap
// tests that go with it. The engine already has an axis-aligned Rect2 and a 3D oriented box (Obb), but
// not the 2D oriented rectangle games constantly need: a rotated pickup or trigger zone, a tilted camera
// bound, hit-testing a rotated UI panel or sprite, a swinging blade's hurtbox. Point-in-rect transforms
// the point into the box's local frame; rect-vs-rect uses the separating-axis theorem over the four
// edge normals (two per box). Pure vec2 math — exactly unit-testable (an axis-aligned OBB matches an
// AABB; a rotated one accepts/rejects points and boxes the AABB would get wrong).
namespace maz::math {

struct OrientedRect2 {
    vec2 center{0.0f, 0.0f};
    vec2 halfExtents{0.5f, 0.5f}; // half width/height along the local x/y axes
    float rotation = 0.0f;        // radians, counter-clockwise

    // Local axes (unit) after rotation.
    vec2 axisX() const { return vec2(std::cos(rotation), std::sin(rotation)); }
    vec2 axisY() const { return vec2(-std::sin(rotation), std::cos(rotation)); }

    // The four world-space corners, in order (--, +-, ++, -+) relative to the local axes.
    std::array<vec2, 4> corners() const {
        const vec2 ux = axisX();
        const vec2 uy = axisY();
        const vec2 ex = vec2(ux.x * halfExtents.x, ux.y * halfExtents.x);
        const vec2 ey = vec2(uy.x * halfExtents.y, uy.y * halfExtents.y);
        return {vec2(center.x - ex.x - ey.x, center.y - ex.y - ey.y),
                vec2(center.x + ex.x - ey.x, center.y + ex.y - ey.y),
                vec2(center.x + ex.x + ey.x, center.y + ex.y + ey.y),
                vec2(center.x - ex.x + ey.x, center.y - ex.y + ey.y)};
    }

    // Is a world-space point inside the rectangle? (transform into local frame and clamp-test)
    bool containsPoint(const vec2& p) const {
        const vec2 d = vec2(p.x - center.x, p.y - center.y);
        const vec2 ux = axisX();
        const vec2 uy = axisY();
        const float lx = d.x * ux.x + d.y * ux.y; // project onto local axes
        const float ly = d.x * uy.x + d.y * uy.y;
        const float eps = 1e-5f;
        return std::fabs(lx) <= halfExtents.x + eps && std::fabs(ly) <= halfExtents.y + eps;
    }

    // Projection "radius" of this box onto a (unit) axis.
    float projectedRadius(const vec2& axis) const {
        const vec2 ux = axisX();
        const vec2 uy = axisY();
        return halfExtents.x * std::fabs(axis.x * ux.x + axis.y * ux.y) +
               halfExtents.y * std::fabs(axis.x * uy.x + axis.y * uy.y);
    }
};

// Do two oriented rectangles overlap? Separating-axis test over the four edge normals.
inline bool orientedRectsOverlap(const OrientedRect2& a, const OrientedRect2& b) {
    const vec2 d = vec2(b.center.x - a.center.x, b.center.y - a.center.y);
    const std::array<vec2, 4> axes = {a.axisX(), a.axisY(), b.axisX(), b.axisY()};
    for (const vec2& axis : axes) {
        const float dist = std::fabs(d.x * axis.x + d.y * axis.y);
        if (dist > a.projectedRadius(axis) + b.projectedRadius(axis)) {
            return false; // found a separating axis
        }
    }
    return true;
}

} // namespace maz::math
