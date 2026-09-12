#pragma once

#include "maz/ui/Rect.hpp"

#include <array>

namespace maz::ui {

// ---- Nine-patch / StyleBox --------------------------------------------------------------------
// Godot draws every themed Panel/Button through a StyleBox — most powerfully a NINE-PATCH: a source
// image sliced into a 3x3 grid by border insets. When the box is drawn at an arbitrary size the four
// CORNERS keep their exact size, the four EDGES stretch along one axis, and the CENTER stretches both
// ways — so a bordered/rounded panel scales to any rectangle without distorting its corner art. This
// module is the pure mapping (source region -> destination region) behind that; it's dependency-free
// geometry (no GPU), so it unit-tests headless and a renderer just blits the 9 quads it returns.

// Per-edge inset thickness (the fixed border margins), in pixels.
struct Border {
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;

    Border() = default;
    Border(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
    explicit Border(float all) : left(all), top(all), right(all), bottom(all) {}
};

enum class Patch9 {
    TopLeft, Top, TopRight,
    Left, Center, Right,
    BottomLeft, Bottom, BottomRight,
};

// One of the nine cells: where to sample from the source and where to draw it in the destination.
struct Patch {
    Rect src;
    Rect dst;
    Patch9 cell = Patch9::Center;

    bool isCorner() const {
        return cell == Patch9::TopLeft || cell == Patch9::TopRight || cell == Patch9::BottomLeft ||
               cell == Patch9::BottomRight;
    }
    bool isCenter() const { return cell == Patch9::Center; }
};

// Slice `dst` into nine regions using `border` insets, sampling matching regions from `src`. Corners are
// the border size in both src and dst (never scaled); the center row/column absorb all the stretch. If
// `dst` is smaller than the borders the middle regions collapse to zero (clamped, never negative).
inline std::array<Patch, 9> ninePatch(const Rect& dst, const Border& border, const Rect& src) {
    auto clampNonNeg = [](float v) { return v < 0.0f ? 0.0f : v; };

    // Destination column edges (x) and row edges (y): corners fixed, middle stretches.
    const float dx0 = dst.x;
    const float dx1 = dst.x + border.left;
    const float dx2 = clampNonNeg(dst.right() - border.right);
    const float dx3 = dst.right();
    const float dy0 = dst.y;
    const float dy1 = dst.y + border.top;
    const float dy2 = clampNonNeg(dst.bottom() - border.bottom);
    const float dy3 = dst.bottom();

    // Source uses the same border insets; its middle is the stretchable core of the image.
    const float sx0 = src.x;
    const float sx1 = src.x + border.left;
    const float sx2 = clampNonNeg(src.right() - border.right);
    const float sx3 = src.right();
    const float sy0 = src.y;
    const float sy1 = src.y + border.top;
    const float sy2 = clampNonNeg(src.bottom() - border.bottom);
    const float sy3 = src.bottom();

    const float dcx[4] = {dx0, dx1, dx2, dx3};
    const float dcy[4] = {dy0, dy1, dy2, dy3};
    const float scx[4] = {sx0, sx1, sx2, sx3};
    const float scy[4] = {sy0, sy1, sy2, sy3};

    const Patch9 cells[9] = {
        Patch9::TopLeft, Patch9::Top, Patch9::TopRight,
        Patch9::Left, Patch9::Center, Patch9::Right,
        Patch9::BottomLeft, Patch9::Bottom, Patch9::BottomRight,
    };

    std::array<Patch, 9> out{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            Patch& p = out[static_cast<std::size_t>(row * 3 + col)];
            p.cell = cells[row * 3 + col];
            p.dst = Rect{dcx[col], dcy[row], clampNonNeg(dcx[col + 1] - dcx[col]),
                         clampNonNeg(dcy[row + 1] - dcy[row])};
            p.src = Rect{scx[col], scy[row], clampNonNeg(scx[col + 1] - scx[col]),
                         clampNonNeg(scy[row + 1] - scy[row])};
        }
    }
    return out;
}

} // namespace maz::ui
