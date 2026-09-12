#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::game {

// Parallax scrolling backgrounds — Godot's ParallaxBackground + ParallaxLayer. A staple of 2D games that
// Maz had no notion of: several background layers that scroll at DIFFERENT rates relative to the camera so
// the scene reads as having depth (distant mountains barely move, near foliage races past), each layer
// TILED/MIRRORED so a small motif covers an unbounded scroll. This is pure transform math — given the
// camera's scroll and a layer's motion scale, it produces the layer's on-screen offset and the tiling
// helpers to cover the viewport — so the app draws whatever art it likes at the returned positions. No
// renderer dependency; header-only; unit-tests headlessly.

// One background layer.
struct ParallaxLayer {
    math::vec2 motionScale{1.0f, 1.0f};  // fraction of the camera scroll this layer follows: 1 = locked to
                                         //   the world (foreground), 0 = fixed on screen (far backdrop)
    math::vec2 motionOffset{0.0f, 0.0f}; // constant offset added on top (e.g. an autoscroll base)
    math::vec2 mirroring{0.0f, 0.0f};    // repeat period in px per axis; 0 on an axis = no tiling there
};

// The layer's (unwrapped) top-left scroll offset for a given camera scroll. As the camera scrolls right
// (scroll.x grows), a layer with a smaller motionScale slides left more slowly → the parallax effect.
inline math::vec2 layerOffset(const ParallaxLayer& layer, math::vec2 cameraScroll) {
    return layer.motionOffset - cameraScroll * layer.motionScale;
}

// Positive modulo: result is in [0, period) for period > 0 (unlike std::fmod, which keeps the sign of a).
inline float pmod(float a, float period) {
    if (period <= 0.0f) {
        return a;
    }
    float r = std::fmod(a, period);
    if (r < 0.0f) {
        r += period;
    }
    return r;
}

// For a mirrored axis with `period` px, the coordinate of the FIRST tile — the largest multiple of the
// period placed so it sits at or just left of / above the viewport origin (result in [-period, 0)). Draw
// tiles at firstTile, firstTile+period, firstTile+2*period, … to seamlessly cover a viewport.
inline float firstTile(float offset, float period) {
    if (period <= 0.0f) {
        return offset;
    }
    return pmod(offset, period) - period;
}

// How many tiles of `period` px it takes to cover `extent` px starting from firstTile() (one extra so the
// partial tiles at both ends are always filled). Returns 1 for a non-tiled (period <= 0) axis.
inline int tileCount(float extent, float period) {
    if (period <= 0.0f) {
        return 1;
    }
    return static_cast<int>(std::ceil(extent / period)) + 1;
}

} // namespace maz::game
