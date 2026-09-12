#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game squad formations — arrange a group of units into a recognisable shape around an anchor (the
// leader or a target point), oriented to a facing direction. This is the geometry every RTS squad, party of
// followers, tactical fireteam, or escort mission needs: give it a count and a shape and it hands back the
// slot each unit should move toward (feed those to Steering/arrive or a pathfinder). `formationSlots` returns
// LOCAL offsets with forward = +Y and right = +X (anchor at the origin); `formationPositions` rotates those
// by a facing vector and translates them to the anchor, so the whole formation turns as the leader turns.
// Shapes: Line (abreast), Column (single file behind), Wedge (arrowhead V), Box (a centred grid), Circle
// (a defensive ring). Godot ships no formation helper. Header-only, pure, deterministic.
namespace maz::game {

enum class FormationShape { Line, Column, Wedge, Box, Circle };

// Local slot offsets for `count` units, anchor at the origin, forward = +Y, right = +X, neighbour gap
// `spacing`. Slot 0 is the leader/front reference. Returns empty for count <= 0.
inline std::vector<math::vec2> formationSlots(FormationShape shape, int count, float spacing) {
    std::vector<math::vec2> out;
    if (count <= 0) return out;
    out.reserve(static_cast<std::size_t>(count));
    const float pi = 3.14159265358979323846f;

    switch (shape) {
    case FormationShape::Line: // abreast, centred left-right
        for (int i = 0; i < count; ++i) {
            const float x = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * spacing;
            out.push_back(math::vec2(x, 0.0f));
        }
        break;
    case FormationShape::Column: // single file receding behind the leader
        for (int i = 0; i < count; ++i)
            out.push_back(math::vec2(0.0f, -static_cast<float>(i) * spacing));
        break;
    case FormationShape::Wedge: // arrowhead: leader at the tip, ranks fanning back symmetrically
        out.push_back(math::vec2(0.0f, 0.0f));
        for (int i = 1; i < count; ++i) {
            const int row = (i + 1) / 2;                    // 1,1,2,2,3,3,…
            const float side = (i % 2 == 1) ? -1.0f : 1.0f; // left then right
            out.push_back(math::vec2(side * static_cast<float>(row) * spacing,
                                     -static_cast<float>(row) * spacing));
        }
        break;
    case FormationShape::Box: { // a centred grid, row-major, filling front to back
        int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
        if (cols < 1) cols = 1;
        for (int i = 0; i < count; ++i) {
            const int r = i / cols;
            const int c = i % cols;
            const float x = (static_cast<float>(c) - static_cast<float>(cols - 1) * 0.5f) * spacing;
            out.push_back(math::vec2(x, -static_cast<float>(r) * spacing));
        }
        break;
    }
    case FormationShape::Circle: { // an evenly-spaced ring; radius grows so neighbours stay ~spacing apart
        const float radius = count > 1 ? spacing * static_cast<float>(count) / (2.0f * pi) : 0.0f;
        for (int i = 0; i < count; ++i) {
            const float ang = static_cast<float>(i) / static_cast<float>(count) * 2.0f * pi;
            out.push_back(math::vec2(std::cos(ang) * radius, std::sin(ang) * radius));
        }
        break;
    }
    }
    return out;
}

// World-space slot positions: the local layout rotated so its +Y aligns with `facing` and translated to
// `anchor`. A zero-length facing defaults to +Y (no rotation). Right is `(facing.y, -facing.x)`.
inline std::vector<math::vec2> formationPositions(const math::vec2& anchor, const math::vec2& facing,
                                                  FormationShape shape, int count, float spacing) {
    math::vec2 fwd = facing;
    const float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y);
    if (len > 1e-8f) {
        fwd.x /= len;
        fwd.y /= len;
    } else {
        fwd = math::vec2(0.0f, 1.0f);
    }
    const math::vec2 right(fwd.y, -fwd.x); // +Y forward -> +X right (clockwise perpendicular)

    const std::vector<math::vec2> local = formationSlots(shape, count, spacing);
    std::vector<math::vec2> out;
    out.reserve(local.size());
    for (const math::vec2& l : local)
        out.push_back(math::vec2(anchor.x + right.x * l.x + fwd.x * l.y, anchor.y + right.y * l.x + fwd.y * l.y));
    return out;
}

} // namespace maz::game
