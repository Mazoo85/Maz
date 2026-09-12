// Maz Engine — "CAVE" (procedural generation + tilemap autotiling, toward Godot's TileMap terrains)
// A cavern is grown by cellular automata (game::CellularCave): random fill, then smoothing passes that
// keep a cell solid where its neighbours are mostly solid. Each wall cell is then AUTOTILED — its
// 4-bit edge mask (game::autotileMask4: which N/E/S/W neighbours are also wall) drives how it is drawn,
// inset on the sides that face open floor so the walls round off into smooth cave borders (the same
// bitmask a Godot terrain tileset keys on to pick a border tile). Deterministic under its seed, so the
// render is golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 q[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(q, 4, c);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CAVE (procedural gen + autotiling) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Cave (procgen + autotile)";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // Generate a cave (fixed seed -> deterministic, golden-stable).
    const int cols = 64, rows = 34;
    const uint64_t seed = 0x5EEDF00Du;
    const std::vector<uint8_t> grid = game::CellularCave::generate(cols, rows, seed, 0.52f, 4);

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);
    const float cell = 17.0f;
    const float gridW = cols * cell, gridH = rows * cell;
    const float ox = (sw - gridW) * 0.5f;
    const float oy = (sh - gridH) * 0.5f + 14.0f;
    const float pad = cell * 0.28f; // how far walls inset from open floor

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.06f, 0.08f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Dark floor bed under the whole grid.
            fillRect(*renderer, ox, oy, gridW, gridH, render::Color{0.10f, 0.11f, 0.15f, 1.0f});

            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    if (grid[game::CellularCave::idx(cols, x, y)] == 0) {
                        continue; // floor: leave the dark bed showing
                    }
                    const uint8_t m = game::autotileMask4(grid, cols, rows, x, y);
                    // Inset each side that faces open floor (its mask bit is clear).
                    const float li = (m & 0x8) ? 0.0f : pad; // W
                    const float ri = (m & 0x2) ? 0.0f : pad; // E
                    const float ti = (m & 0x1) ? 0.0f : pad; // N
                    const float bi = (m & 0x4) ? 0.0f : pad; // S
                    const float px = ox + static_cast<float>(x) * cell + li;
                    const float py = oy + static_cast<float>(y) * cell + ti;
                    const float pw = cell - li - ri;
                    const float ph = cell - ti - bi;

                    // Shade by how enclosed the cell is (deeper rock = darker), for a bit of depth.
                    const int neigh = game::CellularCave::countWalls(grid, cols, rows, x, y);
                    const float t = static_cast<float>(neigh) / 8.0f;
                    const render::Color wall{0.30f + 0.16f * t, 0.34f + 0.20f * t, 0.44f + 0.24f * t,
                                             1.0f};
                    fillRect(*renderer, px, py, pw, ph, wall);
                }
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PROCEDURAL CAVE + AUTOTILING",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "cellular-automata cave (seeded) + 4-bit edge-mask autotiled wall borders",
                          render::Color{0.75f, 0.85f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CAVE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
