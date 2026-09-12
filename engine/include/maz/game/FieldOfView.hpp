#pragma once

#include "maz/math/VectorInt.hpp"

#include <functional>
#include <set>
#include <utility>
#include <vector>

// maz::game field of view — recursive shadowcasting, the standard roguelike "what can this tile see?"
// algorithm. Given an origin, a sight radius, and an opacity predicate (which tiles block vision), it
// returns every visible tile: the lit region of a torch, a guard's sight (occluded by walls), or the
// fog-of-war reveal as a unit moves. Unlike point-to-point line-of-sight (GridLine), this computes the
// WHOLE visible set in one sweep. It processes the eight octants around the origin, tracking the
// shadow each opaque tile casts as a slope range and recursing into the still-visible sub-ranges, so
// cost is proportional to the visible area, not the square of the radius. Opaque tiles are themselves
// visible (you see the wall you're up against), but tiles behind them are not. Pure integer grid math
// on a caller-supplied predicate, deterministic, header-only. Godot leaves FOV to the game, so this is
// a genuinely-useful utility with a testable output.
namespace maz::game {

using FovCell = math::Vector2i;

namespace detail {

// The eight octant transforms (xx, xy, yx, yy) — RogueBasin's canonical recursive-shadowcasting table.
inline const int kFovMult[4][8] = {
    {1, 0, 0, -1, -1, 0, 0, 1},
    {0, 1, -1, 0, 0, -1, 1, 0},
    {0, 1, 1, 0, 0, -1, -1, 0},
    {1, 0, 0, 1, -1, 0, 0, -1},
};

inline void castLight(int cx, int cy, int radius, int row, float startSlope, float endSlope, int xx,
                      int xy, int yx, int yy, const std::function<bool(const FovCell&)>& opaque,
                      std::set<std::pair<int, int>>& visible) {
    if (startSlope < endSlope) {
        return;
    }
    const int r2 = radius * radius;
    float nextStart = startSlope;
    for (int i = row; i <= radius; ++i) {
        bool blocked = false;
        const int dy = -i;
        for (int dx = -i; dx <= 0; ++dx) {
            const float lSlope = (static_cast<float>(dx) - 0.5f) / (static_cast<float>(dy) + 0.5f);
            const float rSlope = (static_cast<float>(dx) + 0.5f) / (static_cast<float>(dy) - 0.5f);
            if (startSlope < rSlope) {
                continue;
            }
            if (endSlope > lSlope) {
                break;
            }
            const int mapX = cx + dx * xx + dy * xy;
            const int mapY = cy + dx * yx + dy * yy;
            if (dx * dx + dy * dy <= r2) {
                visible.insert({mapX, mapY});
            }
            const FovCell here(mapX, mapY);
            if (blocked) {
                if (opaque(here)) {
                    nextStart = rSlope;
                    continue;
                }
                blocked = false;
                startSlope = nextStart;
            } else {
                if (opaque(here) && i < radius) {
                    blocked = true;
                    castLight(cx, cy, radius, i + 1, startSlope, lSlope, xx, xy, yx, yy, opaque,
                              visible);
                    nextStart = rSlope;
                }
            }
        }
        if (blocked) {
            break;
        }
    }
}

} // namespace detail

// Every tile visible from `origin` within `radius` (Euclidean: dx*dx + dy*dy <= radius*radius), where
// `opaque(cell)` is true for tiles that block sight. The origin is always included; opaque tiles that
// are reached are visible but do not reveal what lies behind them. `radius <= 0` yields just the origin.
inline std::vector<FovCell> computeFov(FovCell origin, int radius,
                                       const std::function<bool(const FovCell&)>& opaque) {
    std::set<std::pair<int, int>> visible;
    visible.insert({origin.x, origin.y});
    if (radius > 0) {
        for (int oct = 0; oct < 8; ++oct) {
            detail::castLight(origin.x, origin.y, radius, 1, 1.0f, 0.0f, detail::kFovMult[0][oct],
                              detail::kFovMult[1][oct], detail::kFovMult[2][oct],
                              detail::kFovMult[3][oct], opaque, visible);
        }
    }
    std::vector<FovCell> out;
    out.reserve(visible.size());
    for (const auto& p : visible) {
        out.push_back(FovCell(p.first, p.second));
    }
    return out;
}

} // namespace maz::game
