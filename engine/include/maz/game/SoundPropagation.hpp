#pragma once

#include "maz/math/VectorInt.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

// maz::game sound propagation — the "how loud is the gunshot HERE, after it has travelled around the
// walls?" query that drives stealth and zombie-attraction AI. A single noise source (a footstep, a
// smashed window, a gunshot) emits a loudness; sound spreads outward across a grid and is ATTENUATED as
// it travels — a little per open tile, a lot through a thick wall — so the loudness heard at any cell is
// the source loudness minus the CHEAPEST accumulated attenuation along a path to it. Because it is a
// least-cost search (weighted Dijkstra with a min-heap), sound correctly ROUTES AROUND obstacles: a
// zombie behind a wall hears the shot only as loud as the go-around path allows, not the straight line
// through the wall. Cells whose best path drops below the audible floor are silent (0).
//
// This is deliberately distinct from the engine's neighbours: DijkstraMap (M?) is UNIT-COST integer BFS
// (pure step distance, no per-tile weighting); InfluenceMap is an iterative DIFFUSION that only
// approximates falloff and never gives a path-exact, wall-attenuated loudness. Sound propagation is the
// float-weighted, obstacle-aware field a survival game actually reads to decide who heard what. Godot
// ships no equivalent — games hand-roll it. Deterministic, header-only, std-only.
namespace maz::game {

using SoundCell = math::Vector2i;

// Result of a propagation: `loudness[y*width + x]` is the loudness heard at that cell (0 = inaudible).
struct SoundField {
    int width = 0;
    int height = 0;
    std::vector<float> loudness;

    float at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return 0.0f;
        }
        return loudness[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                        static_cast<std::size_t>(x)];
    }
    bool audible(int x, int y) const { return at(x, y) > 0.0f; }
};

// Propagate `sourceLoudness` from `source` across a `width` x `height` grid. `attenuation(cell)` returns
// the loudness LOST when sound passes through that cell per unit of travel (>= 0; make a wall a big value
// to muffle it, or huge to seal it). Entering a cell costs attenuation(cell) * stepLength, where a
// straight step is 1 and a diagonal step is sqrt(2) (diagonals only when `diagonal` is true). The heard
// loudness at a cell is sourceLoudness minus the least accumulated cost to reach it; anything at or below
// `audibleFloor` is stored as 0 and not expanded further. The source cell itself always hears
// sourceLoudness (its own attenuation is not charged).
inline SoundField propagateSound(int width, int height, SoundCell source, float sourceLoudness,
                                 const std::function<float(const SoundCell&)>& attenuation,
                                 bool diagonal = false, float audibleFloor = 0.0f) {
    SoundField field;
    field.width = std::max(0, width);
    field.height = std::max(0, height);
    field.loudness.assign(static_cast<std::size_t>(field.width) * static_cast<std::size_t>(field.height),
                          0.0f);
    if (field.width == 0 || field.height == 0) {
        return field;
    }
    if (source.x < 0 || source.y < 0 || source.x >= field.width || source.y >= field.height) {
        return field;
    }
    if (sourceLoudness <= audibleFloor) {
        return field;
    }

    auto idx = [&](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(field.width) +
               static_cast<std::size_t>(x);
    };

    // Cost = accumulated attenuation from the source; loudness = sourceLoudness - cost. We run Dijkstra on
    // cost (a min-heap on the smallest cost so far), which is equivalent to a max-heap on loudness.
    const float kInf = std::numeric_limits<float>::infinity();
    std::vector<float> cost(field.loudness.size(), kInf);
    // Min-heap of (cost, x, y); std::priority_queue is a max-heap, so negate via std::greater on the pair.
    using Node = std::pair<float, std::pair<int, int>>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    cost[idx(source.x, source.y)] = 0.0f;
    field.loudness[idx(source.x, source.y)] = sourceLoudness;
    pq.push({0.0f, {source.x, source.y}});

    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    const float step[8] = {1.0f, 1.0f, 1.0f, 1.0f,
                           1.41421356237309515f, 1.41421356237309515f,
                           1.41421356237309515f, 1.41421356237309515f};
    const int nn = diagonal ? 8 : 4;

    while (!pq.empty()) {
        const Node top = pq.top();
        pq.pop();
        const float c = top.first;
        const int cx = top.second.first;
        const int cy = top.second.second;
        if (c > cost[idx(cx, cy)]) {
            continue; // stale heap entry (a cheaper path was already settled)
        }
        for (int i = 0; i < nn; ++i) {
            const int nx = cx + dx[i];
            const int ny = cy + dy[i];
            if (nx < 0 || ny < 0 || nx >= field.width || ny >= field.height) {
                continue;
            }
            const float att = attenuation(SoundCell(nx, ny));
            const float edge = (att < 0.0f ? 0.0f : att) * step[i];
            const float nc = c + edge;
            const float heard = sourceLoudness - nc;
            if (heard <= audibleFloor) {
                continue; // too quiet here — and only gets quieter farther on, so don't expand
            }
            if (nc < cost[idx(nx, ny)]) {
                cost[idx(nx, ny)] = nc;
                field.loudness[idx(nx, ny)] = heard;
                pq.push({nc, {nx, ny}});
            }
        }
    }
    return field;
}

} // namespace maz::game
