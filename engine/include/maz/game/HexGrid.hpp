#pragma once

#include "maz/math/Math.hpp" // vec2

#include <algorithm> // std::min, std::max
#include <cmath>
#include <cstddef>
#include <cstdlib> // std::abs
#include <vector>

// maz::game hex-grid math — axial-coordinate hexagon utilities (the redblobgames conventions), the
// backbone of hex strategy/board games and Godot's TileMap hexagon layout. A Hex is an axial (q, r)
// coordinate; helpers convert to/from pixel space for both pointy-top and flat-top orientations,
// measure hex distance, list the six neighbours, round a fractional hex to the nearest cell, and walk
// a straight line of hexes. Pure integer/float value math — header-only, deterministic, exactly
// unit-testable. Godot's TileMap only offers square/isometric/hex *rendering*; this is the coordinate
// algebra games actually compute with, so it is parity-plus.
namespace maz::game {

enum class HexOrientation { PointyTop, FlatTop };

struct Hex {
    int q = 0;
    int r = 0;
    Hex() = default;
    Hex(int q_, int r_) : q(q_), r(r_) {}
    bool operator==(const Hex& o) const { return q == o.q && r == o.r; }
    bool operator!=(const Hex& o) const { return !(*this == o); }
    Hex operator+(const Hex& o) const { return {q + o.q, r + o.r}; }
    Hex operator-(const Hex& o) const { return {q - o.q, r - o.r}; }
};

// Cube-coordinate implied s = -q - r; distance is the cube L-infinity/2 metric.
inline int hexDistance(const Hex& a, const Hex& b) {
    const int dq = a.q - b.q;
    const int dr = a.r - b.r;
    const int ds = dq + dr; // -(s) difference = dq+dr
    return (std::abs(dq) + std::abs(dr) + std::abs(ds)) / 2;
}

// The six axial neighbour directions (shared by both orientations).
inline const Hex* hexDirections() {
    static const Hex dirs[6] = {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1}};
    return dirs;
}
inline std::vector<Hex> hexNeighbors(const Hex& h) {
    const Hex* d = hexDirections();
    std::vector<Hex> out;
    out.reserve(6);
    for (int i = 0; i < 6; ++i) {
        out.push_back(h + d[i]);
    }
    return out;
}

// Round fractional axial coordinates to the nearest hex (via cube rounding with the largest-diff fix).
inline Hex hexRound(float qf, float rf) {
    const float sf = -qf - rf;
    float rq = std::round(qf), rr = std::round(rf), rs = std::round(sf);
    const float dq = std::fabs(rq - qf), dr = std::fabs(rr - rf), ds = std::fabs(rs - sf);
    if (dq > dr && dq > ds) {
        rq = -rr - rs;
    } else if (dr > ds) {
        rr = -rq - rs;
    }
    return Hex(static_cast<int>(rq), static_cast<int>(rr));
}

// Hex center in pixel space. `size` is the distance from centre to a corner.
inline math::vec2 hexToPixel(const Hex& h, float size, HexOrientation o = HexOrientation::PointyTop) {
    const float k = 1.7320508075688772f; // sqrt(3)
    const float q = static_cast<float>(h.q), r = static_cast<float>(h.r);
    if (o == HexOrientation::PointyTop) {
        return math::vec2(size * (k * q + k * 0.5f * r), size * (1.5f * r));
    }
    return math::vec2(size * (1.5f * q), size * (k * 0.5f * q + k * r));
}

// Nearest hex to a pixel position (inverse of hexToPixel, then rounded).
inline Hex pixelToHex(const math::vec2& p, float size, HexOrientation o = HexOrientation::PointyTop) {
    const float k = 1.7320508075688772f; // sqrt(3)
    float q, r;
    if (o == HexOrientation::PointyTop) {
        q = (k / 3.0f * p.x - 1.0f / 3.0f * p.y) / size;
        r = (2.0f / 3.0f * p.y) / size;
    } else {
        q = (2.0f / 3.0f * p.x) / size;
        r = (-1.0f / 3.0f * p.x + k / 3.0f * p.y) / size;
    }
    return hexRound(q, r);
}

// Scale a hex (or direction) by an integer factor — `dir * k`.
inline Hex hexScale(const Hex& h, int k) { return {h.q * k, h.r * k}; }

// The ring of hexes at EXACTLY `radius` from `center`, walked in order around the ring (6*radius hexes).
// radius 0 is the centre alone; a negative radius is empty. The staple for area-of-effect edges, aura
// outlines, spawn rings, and "everything exactly N tiles away".
inline std::vector<Hex> hexRing(const Hex& center, int radius) {
    std::vector<Hex> out;
    if (radius < 0) return out;
    if (radius == 0) {
        out.push_back(center);
        return out;
    }
    out.reserve(static_cast<std::size_t>(6 * radius));
    const Hex* d = hexDirections();
    // Start at the corner offset by direction 4 * radius so the walk closes the ring cleanly.
    Hex hex = center + hexScale(d[4], radius);
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < radius; ++j) {
            out.push_back(hex);
            hex = hex + d[i];
        }
    }
    return out;
}

// Every hex within `radius` of `center` (inclusive), i.e. hexDistance <= radius. Count is 1+3N(N+1). Order
// is by axial q then r (a compact scan). The staple for blast radii, movement/attack range, and vision area.
inline std::vector<Hex> hexRange(const Hex& center, int radius) {
    std::vector<Hex> out;
    if (radius < 0) return out;
    out.reserve(static_cast<std::size_t>(1 + 3 * radius * (radius + 1)));
    for (int q = -radius; q <= radius; ++q) {
        const int rlo = std::max(-radius, -q - radius);
        const int rhi = std::min(radius, -q + radius);
        for (int r = rlo; r <= rhi; ++r) out.push_back(center + Hex(q, r));
    }
    return out;
}

// The same set as hexRange, but ordered as an outward SPIRAL: centre first, then ring 1, ring 2, … This is
// the order you want for ripple/expanding animations, nearest-first flood reveals, and ring-by-ring iteration.
inline std::vector<Hex> hexSpiral(const Hex& center, int radius) {
    std::vector<Hex> out;
    if (radius < 0) return out;
    out.reserve(static_cast<std::size_t>(1 + 3 * radius * (radius + 1)));
    out.push_back(center);
    for (int k = 1; k <= radius; ++k) {
        const std::vector<Hex> ring = hexRing(center, k);
        out.insert(out.end(), ring.begin(), ring.end());
    }
    return out;
}

// The straight line of hexes from `a` to `b` inclusive (linear cube interpolation + rounding).
inline std::vector<Hex> hexLine(const Hex& a, const Hex& b) {
    const int n = hexDistance(a, b);
    std::vector<Hex> out;
    out.reserve(static_cast<std::size_t>(n) + 1);
    if (n == 0) {
        out.push_back(a);
        return out;
    }
    for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        const float q = static_cast<float>(a.q) + static_cast<float>(b.q - a.q) * t;
        const float r = static_cast<float>(a.r) + static_cast<float>(b.r - a.r) * t;
        out.push_back(hexRound(q, r));
    }
    return out;
}

} // namespace maz::game
