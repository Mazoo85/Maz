// Maz Engine — "TILESET" (a TileSet resource + per-tile collision, toward Godot's TileMap/TileSet)
// A Tilemap is just a grid of numbers; a TileSet gives each number MEANING — which atlas cell to draw it
// with, and what collision shape it contributes. Here one grid mixes full solid GROUND/WALL tiles with
// thin half-height LEDGE tiles (a sub-cell Box collision Godot supports but a single "solid" bit cannot).
// The demo draws the tilemap coloured by each tile's atlas cell, overlays every tile's collision box from
// game::collectSolids (so you can see the ledges are only half-height), and drops probe balls down several
// columns with game::dropY — each resting exactly on the surface it lands on, full tile or mid-cell ledge.
// Everything is precomputed and drawn statically -> deterministic, golden-stable. Run --headless/--frames.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

void rectOutline(render::Renderer& r, float x, float y, float w, float h, float t, render::Color c) {
    fillRect(r, x, y, w, t, c);
    fillRect(r, x, y + h - t, w, t, c);
    fillRect(r, x, y, t, h, c);
    fillRect(r, x + w - t, y, t, h, c);
}

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    const int seg = 26;
    render::Point2 p[28];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

// Tile ids used in the level.
enum : game::TileId { EMPTY = 0, GROUND = 1, WALL = 2, LEDGE = 3 };

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TILESET (per-tile collision) starting");

    const int W = 22, H = 13;
    const float ts = 46.0f;

    game::Tilemap map;
    map.resize(static_cast<uint32_t>(W), static_cast<uint32_t>(H), EMPTY);
    map.setTileSize(ts);

    // Borders + floor.
    for (int y = 0; y < H; ++y) {
        map.set(0, y, WALL);
        map.set(W - 1, y, WALL);
    }
    for (int x = 0; x < W; ++x) {
        map.set(x, H - 1, GROUND);
        map.set(x, H - 2, GROUND);
    }
    // A raised solid platform (ground) mid-level.
    for (int x = 3; x <= 7; ++x) {
        map.set(x, 8, GROUND);
    }
    // A short wall pillar.
    map.set(14, 10, WALL);
    map.set(14, 11, WALL);
    // Floating half-height ledges (sub-cell Box collision).
    map.set(10, 6, LEDGE);
    map.set(11, 6, LEDGE);
    map.set(12, 6, LEDGE);
    map.set(16, 4, LEDGE);
    map.set(17, 4, LEDGE);

    // The TileSet: each id -> atlas cell + collision shape.
    game::TileSet set;
    game::TileDef ground;
    ground.collision = game::TileDef::Full;
    ground.atlasX = 0;
    ground.atlasY = 0;
    set.define(GROUND, ground);
    game::TileDef wall;
    wall.collision = game::TileDef::Full;
    wall.atlasX = 1;
    wall.atlasY = 0;
    set.define(WALL, wall);
    game::TileDef ledge;
    ledge.collision = game::TileDef::Box;
    ledge.boxMin = math::vec2(0.0f, 0.5f); // bottom half solid; top surface sits mid-cell
    ledge.boxMax = math::vec2(1.0f, 1.0f);
    ledge.atlasX = 2;
    ledge.atlasY = 0;
    set.define(LEDGE, ledge);

    // Precompute collision boxes + probe-ball rest positions.
    const std::vector<game::TileBox> solids = game::collectSolids(map, set);
    struct Ball {
        float x, restY;
    };
    std::vector<Ball> balls;
    const int cols[] = {2, 5, 11, 16, 19, 14};
    for (int c : cols) {
        const float wx = (static_cast<float>(c) + 0.5f) * ts;
        const float ry = game::dropY(map, set, wx, 0.0f, static_cast<float>(H) * ts);
        balls.push_back({wx, ry});
    }

    // Colour per atlas cell (stands in for an atlas texture).
    auto atlasColor = [](int ax, int ay) -> render::Color {
        if (ax == 0 && ay == 0) {
            return render::Color{0.55f, 0.4f, 0.28f, 1.0f}; // ground: earthy brown
        }
        if (ax == 1 && ay == 0) {
            return render::Color{0.42f, 0.45f, 0.5f, 1.0f}; // wall: slate gray
        }
        return render::Color{0.35f, 0.6f, 0.85f, 1.0f}; // ledge: blue
    };

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — TileSet & Per-Tile Collision";
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

    const float ox = 100.0f, oy = 70.0f; // screen offset for the tile-world origin

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TILESET & PER-TILE COLLISION",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 44.0f,
                          "one grid of ids -> a TileSet gives each an atlas cell + collision shape; "
                          "ledges are half-height (game::TileSet)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.36f);

            // Draw the tilemap: each non-empty cell filled with its atlas-cell colour. Ledges draw only
            // their solid (bottom) half so their sub-cell shape is visible.
            for (int cy = 0; cy < H; ++cy) {
                for (int cx = 0; cx < W; ++cx) {
                    const game::TileId id = map.at(cx, cy);
                    const game::TileDef* d = set.get(id);
                    if (!d) {
                        continue;
                    }
                    const float px = ox + static_cast<float>(cx) * ts;
                    const float py = oy + static_cast<float>(cy) * ts;
                    const render::Color col = atlasColor(d->atlasX, d->atlasY);
                    if (d->collision == game::TileDef::Box) {
                        const float by = py + d->boxMin.y * ts;
                        const float bh = (d->boxMax.y - d->boxMin.y) * ts;
                        fillRect(*renderer, px + 1.0f, by, ts - 2.0f, bh, col);
                    } else {
                        fillRect(*renderer, px + 1.0f, py + 1.0f, ts - 2.0f, ts - 2.0f, col);
                    }
                }
            }

            // Overlay every tile's collision box (from collectSolids) as a translucent yellow outline.
            for (const game::TileBox& b : solids) {
                rectOutline(*renderer, ox + b.min.x, oy + b.min.y, b.max.x - b.min.x, b.max.y - b.min.y,
                            2.0f, render::Color{1.0f, 0.9f, 0.35f, 0.85f});
            }

            // Probe balls resting on whatever surface they dropped onto.
            for (const Ball& b : balls) {
                const float rad = 12.0f;
                const math::vec2 c(ox + b.x, oy + b.restY - rad);
                fillCircle(*renderer, c, rad, render::Color{0.95f, 0.35f, 0.45f, 1.0f});
                fillCircle(*renderer, c, rad - 4.0f, render::Color{1.0f, 0.7f, 0.75f, 1.0f});
            }

            // Legend + counts.
            char buf[96];
            std::snprintf(buf, sizeof(buf), "solid tiles: %d   (collision boxes drawn in yellow)",
                          static_cast<int>(solids.size()));
            font.drawText(*renderer, 100.0f, static_cast<float>(H) * ts + 90.0f, buf,
                          render::Color{0.75f, 0.78f, 0.85f, 1}, 0.34f);
            font.drawText(*renderer, 100.0f, static_cast<float>(H) * ts + 116.0f,
                          "red balls dropped down columns rest on the tile they hit - note the ones on "
                          "half-height ledges sit mid-cell",
                          render::Color{0.66f, 0.7f, 0.8f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TILESET shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
