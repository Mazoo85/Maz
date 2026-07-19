#pragma once

#include "maz/math/Rect2.hpp"     // Rect2::Side (shared with the float rect)
#include "maz/math/VectorInt.hpp" // Vector2i

#include <algorithm>
#include <cstdint>

// Rect2i — Godot's Rect2i: an axis-aligned rectangle in INTEGER coordinates (position = min corner +
// size), the whole-number companion to Rect2. Tile regions, texture-atlas sub-rects, pixel windows,
// and grid selections are all naturally integer, so this keeps them exact (no float drift on edges).
// Same API as Rect2 — hasPoint / intersects / intersection / merge / encloses / grow / expand / abs —
// with Godot's half-open convention (left/top inclusive, right/bottom exclusive). Header-only, pure.
namespace maz::math {

struct Rect2i {
    Vector2i position;
    Vector2i size;

    Rect2i() = default;
    Rect2i(Vector2i pos, Vector2i sz) : position(pos), size(sz) {}
    Rect2i(int x, int y, int w, int h) : position(x, y), size(w, h) {}

    int left() const { return position.x; }
    int top() const { return position.y; }
    int right() const { return position.x + size.x; }
    int bottom() const { return position.y + size.y; }
    Vector2i end() const { return position + size; } // the max corner (exclusive)
    Vector2i center() const { return position + size / 2; }
    std::int64_t area() const {
        return static_cast<std::int64_t>(size.x) * static_cast<std::int64_t>(size.y);
    }
    bool hasArea() const { return size.x > 0 && size.y > 0; }

    bool operator==(const Rect2i& o) const { return position == o.position && size == o.size; }
    bool operator!=(const Rect2i& o) const { return !(*this == o); }

    // Half-open: left/top inclusive, right/bottom exclusive (Godot's Rect2i.has_point).
    bool hasPoint(Vector2i p) const {
        return p.x >= position.x && p.y >= position.y && p.x < position.x + size.x &&
               p.y < position.y + size.y;
    }

    bool intersects(const Rect2i& b) const {
        return position.x < b.position.x + b.size.x && b.position.x < position.x + size.x &&
               position.y < b.position.y + b.size.y && b.position.y < position.y + size.y;
    }

    // Overlap rectangle; zero-size when the two are disjoint (Godot's Rect2i.intersection).
    Rect2i intersection(const Rect2i& b) const {
        if (!intersects(b)) {
            return Rect2i();
        }
        const int x0 = std::max(position.x, b.position.x);
        const int y0 = std::max(position.y, b.position.y);
        const int x1 = std::min(position.x + size.x, b.position.x + b.size.x);
        const int y1 = std::min(position.y + size.y, b.position.y + b.size.y);
        return Rect2i(Vector2i(x0, y0), Vector2i(x1 - x0, y1 - y0));
    }

    // Smallest rectangle containing both (Godot's Rect2i.merge).
    Rect2i merge(const Rect2i& b) const {
        const int x0 = std::min(position.x, b.position.x);
        const int y0 = std::min(position.y, b.position.y);
        const int x1 = std::max(position.x + size.x, b.position.x + b.size.x);
        const int y1 = std::max(position.y + size.y, b.position.y + b.size.y);
        return Rect2i(Vector2i(x0, y0), Vector2i(x1 - x0, y1 - y0));
    }

    bool encloses(const Rect2i& b) const {
        return b.position.x >= position.x && b.position.y >= position.y &&
               b.position.x + b.size.x <= position.x + size.x &&
               b.position.y + b.size.y <= position.y + size.y;
    }

    // Grow (positive) or shrink (negative) on every side (Godot's Rect2i.grow).
    Rect2i grow(int amount) const {
        return Rect2i(Vector2i(position.x - amount, position.y - amount),
                      Vector2i(size.x + amount * 2, size.y + amount * 2));
    }
    Rect2i growIndividual(int l, int t, int r, int b) const {
        return Rect2i(Vector2i(position.x - l, position.y - t),
                      Vector2i(size.x + l + r, size.y + t + b));
    }
    // Grow/shrink a single edge (Godot's Rect2i.grow_side), reusing Rect2's Side enum (L/T/R/B).
    Rect2i growSide(Rect2::Side side, int amount) const {
        using S = Rect2::Side;
        return growIndividual(side == S::Left ? amount : 0, side == S::Top ? amount : 0,
                              side == S::Right ? amount : 0, side == S::Bottom ? amount : 0);
    }

    // Grow the rectangle to include point `p` (Godot's Rect2i.expand).
    Rect2i expand(Vector2i p) const {
        const int x0 = std::min(position.x, p.x);
        const int y0 = std::min(position.y, p.y);
        const int x1 = std::max(position.x + size.x, p.x);
        const int y1 = std::max(position.y + size.y, p.y);
        return Rect2i(Vector2i(x0, y0), Vector2i(x1 - x0, y1 - y0));
    }

    // Normalise a rectangle that has a negative size on some axis (Godot's Rect2i.abs).
    Rect2i abs() const {
        return Rect2i(Vector2i(position.x + std::min(size.x, 0), position.y + std::min(size.y, 0)),
                      Vector2i(std::abs(size.x), std::abs(size.y)));
    }
};

} // namespace maz::math
