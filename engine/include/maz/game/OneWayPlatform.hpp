#pragma once

#include <cstddef>
#include <vector>

namespace maz::game {

// One-way platforms — Godot's `one_way_collision` on StaticBody2D / TileMap collision shapes. A one-way
// platform is solid only from ONE side: a character falling onto it lands, but a character jumping up from
// below passes straight through (and, standing on it, can drop through by tapping down). Maz's Physics2D
// collides solid boxes both ways; this adds the swept "solid-from-above" resolve a platformer needs. It is
// pure geometry over a horizontal surface: given a body's vertical span this frame (prevBottom -> curBottom)
// and its horizontal extent, `resolveOneWayPlatform` reports whether the body crossed the surface FROM ABOVE
// while descending (a landing) and the snapped resting height; a body moving up, or already below the
// surface, is never blocked. `resolveOneWayPlatforms` picks the topmost surface a falling body lands on this
// step. Header-only, no renderer/sim dependency, so it unit-tests headlessly; the app runs a fixed-step
// simulation and draws the settled result.

struct OneWayPlatform2D {
    float y;  // surface height (y-down world; a body's bottom rests here)
    float x0; // left edge
    float x1; // right edge
};

struct OneWayResult {
    bool landed;
    float y; // corrected body-bottom (== platform surface when landed, else the input curBottom)
};

// Swept one-way resolution for a body descending from `prevBottom` to `curBottom` this step, with horizontal
// span [bx0, bx1]. Lands only if the body is moving down (or level) AND crossed the surface from above AND
// horizontally overlaps the platform. `snapTolerance` lets a body already resting just below the surface (a
// tiny numerical overshoot) still count as landed rather than tunneling.
inline OneWayResult resolveOneWayPlatform(float prevBottom, float curBottom, float bx0, float bx1,
                                          const OneWayPlatform2D& p, float snapTolerance = 1.0f) {
    OneWayResult r{false, curBottom};
    if (curBottom < prevBottom - 1e-6f) {
        return r; // moving up -> pass through
    }
    if (bx1 <= p.x0 || bx0 >= p.x1) {
        return r; // no horizontal overlap
    }
    // The previous bottom must have been at or above the surface (within tolerance), and this bottom at or
    // below it — i.e. the surface was crossed downward this step.
    if (prevBottom <= p.y + snapTolerance && curBottom >= p.y) {
        r.landed = true;
        r.y = p.y;
    }
    return r;
}

// Resolve a descending body against many one-way platforms; returns the TOPMOST surface it lands on this
// step (smallest y in a y-down world), or {false, curBottom} if it lands on none.
inline OneWayResult resolveOneWayPlatforms(float prevBottom, float curBottom, float bx0, float bx1,
                                           const std::vector<OneWayPlatform2D>& platforms,
                                           float snapTolerance = 1.0f) {
    OneWayResult best{false, curBottom};
    for (const OneWayPlatform2D& p : platforms) {
        const OneWayResult r = resolveOneWayPlatform(prevBottom, curBottom, bx0, bx1, p, snapTolerance);
        if (r.landed && (!best.landed || r.y < best.y)) {
            best = r;
        }
    }
    return best;
}

} // namespace maz::game
