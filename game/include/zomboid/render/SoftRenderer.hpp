// Software (reference) rasterizer for the ZOMBOID sim.
//
// Draws a Sim frame into a CPU Framebuffer with the neon SEGA palette, matching
// the browser reference's look (tiles + detailing, entities, night darkness).
// It is engine-agnostic and deterministic, so it powers headless "does the world
// actually look right" checks and future golden-image render tests (Phase 12).
// The Vulkan sprite/tilemap batch reproduces the same draw intent on the GPU.
#pragma once

#include "zomboid/Sim.hpp"
#include "zomboid/render/Framebuffer.hpp"

namespace zb {

// Neon palette (0xRRGGBB), mirroring the NEON constants in js/game.js.
namespace neon {
constexpr Color pink = rgb(0xff2d95);
constexpr Color cyan = rgb(0x05d9e8);
constexpr Color purple = rgb(0x9d00ff);
constexpr Color green = rgb(0x39ff14);
constexpr Color yellow = rgb(0xffe600);
constexpr Color orange = rgb(0xff7b00);
constexpr Color blue = rgb(0x2d6bff);
constexpr Color red = rgb(0xff1744);
} // namespace neon

struct RenderOptions {
    int tilePx = 16;      // pixels per tile
    bool centerPlayer = true; // camera follows the player (else top-left origin)
    bool applyNight = true;   // apply the day/night darkness overlay
};

// Render the current game viewport (camera-follows-player) into fb.
void renderScene(const Sim& sim, Framebuffer& fb, const RenderOptions& opts = {});

// Render a whole-world overview (the "tactical map" look) into fb, scaling the
// entire Anchorage grid to fit. Good for verifying worldgen at a glance.
void renderWorldMap(const Sim& sim, Framebuffer& fb);

} // namespace zb
