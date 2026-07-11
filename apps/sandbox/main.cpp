// Maz Engine — Sandbox
// A small top-down demo: a tile world you walk around with WASD, wall/water collision, and a
// camera that follows the player. Proves the engine can build an actual game from its sprite,
// input, camera, and tilemap primitives. Run --headless (or --frames N) for CI; --demo enables
// an autopilot that patrols the map (used for offscreen capture where there's no keyboard).

#include "maz/Engine.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

using namespace maz;

namespace {

// --- Tile ids (must match the generated tileset atlas order) ---
enum : game::TileId { TILE_GRASS = 0, TILE_PATH = 1, TILE_WATER = 2, TILE_WALL = 3 };
constexpr uint32_t kTilesetCount = 4;
constexpr uint32_t kTilePx = 16; // atlas cell size in pixels

void putPixel(std::vector<uint8_t>& px, uint32_t w, uint32_t x, uint32_t y, uint8_t r, uint8_t g,
              uint8_t b, uint8_t a = 255) {
    const size_t i = (static_cast<size_t>(y) * w + x) * 4;
    px[i + 0] = r;
    px[i + 1] = g;
    px[i + 2] = b;
    px[i + 3] = a;
}

// A horizontal atlas of kTilesetCount cells, each kTilePx square, with light per-pixel noise so
// surfaces read as textured rather than flat.
std::vector<uint8_t> makeTileset() {
    const uint32_t w = kTilePx * kTilesetCount;
    const uint32_t h = kTilePx;
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4, 255);
    struct RGB {
        uint8_t r, g, b;
    };
    const RGB base[kTilesetCount] = {
        {58, 138, 64},   // grass
        {170, 140, 92},  // path
        {46, 96, 180},   // water
        {96, 100, 110},  // wall
    };
    for (uint32_t t = 0; t < kTilesetCount; ++t) {
        for (uint32_t y = 0; y < kTilePx; ++y) {
            for (uint32_t x = 0; x < kTilePx; ++x) {
                const int n = static_cast<int>((x * 7u + y * 13u + t * 5u) % 5u) - 2; // -2..2
                const RGB c = base[t];
                auto clamp = [](int v) -> uint8_t {
                    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
                };
                putPixel(px, w, t * kTilePx + x, y, clamp(c.r + n * 6), clamp(c.g + n * 6),
                         clamp(c.b + n * 6));
            }
        }
    }
    return px;
}

// A 16x16 character: bright body, dark outline, two eyes.
std::vector<uint8_t> makePlayer() {
    const uint32_t s = 16;
    std::vector<uint8_t> px(static_cast<size_t>(s) * s * 4, 0); // transparent
    for (uint32_t y = 0; y < s; ++y) {
        for (uint32_t x = 0; x < s; ++x) {
            const bool border = x == 0 || y == 0 || x == s - 1 || y == s - 1;
            if (border) {
                putPixel(px, s, x, y, 20, 20, 28, 255);
            } else {
                putPixel(px, s, x, y, 240, 200, 70, 255); // body
            }
        }
    }
    putPixel(px, s, 5, 6, 20, 20, 28), putPixel(px, s, 6, 6, 20, 20, 28);
    putPixel(px, s, 10, 6, 20, 20, 28), putPixel(px, s, 11, 6, 20, 20, 28);
    return px;
}

void buildMap(game::Tilemap& map) {
    map.resize(48, 36, TILE_GRASS);
    map.setTileSize(32.0f);
    map.setSolid(TILE_WALL, true);
    map.setSolid(TILE_WATER, true);

    std::mt19937 rng(1337u); // fixed seed => reproducible layout
    std::uniform_int_distribution<int> pct(0, 99);

    for (int y = 0; y < static_cast<int>(map.height()); ++y) {
        for (int x = 0; x < static_cast<int>(map.width()); ++x) {
            const bool edge = x == 0 || y == 0 || x == static_cast<int>(map.width()) - 1 ||
                              y == static_cast<int>(map.height()) - 1;
            if (edge) {
                map.set(x, y, TILE_WALL);
            } else if (pct(rng) < 8) {
                map.set(x, y, TILE_WALL);
            } else if (pct(rng) < 4) {
                map.set(x, y, TILE_PATH);
            }
        }
    }
    // A small water pond.
    for (int y = 6; y < 12; ++y) {
        for (int x = 30; x < 38; ++x) {
            map.set(x, y, TILE_WATER);
        }
    }
    // Keep the spawn area (center) clear and walkable.
    for (int y = 16; y < 20; ++y) {
        for (int x = 22; x < 26; ++x) {
            map.set(x, y, TILE_GRASS);
        }
    }
}

// True if an axis-aligned box [x, x+size] x [y, y+size] overlaps any solid tile.
bool boxHitsSolid(const game::Tilemap& map, float x, float y, float size) {
    const int x0 = map.worldToTileX(x);
    const int y0 = map.worldToTileY(y);
    const int x1 = map.worldToTileX(x + size - 0.001f);
    const int y1 = map.worldToTileY(y + size - 0.001f);
    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            if (map.isSolidTile(tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

void tileUV(game::TileId id, render::SpriteDesc& s) {
    const float u0 = static_cast<float>(id) / static_cast<float>(kTilesetCount);
    const float u1 = static_cast<float>(id + 1) / static_cast<float>(kTilesetCount);
    s.uvMinX = u0;
    s.uvMaxX = u1;
    s.uvMinY = 0.0f;
    s.uvMaxY = 1.0f;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("Maz Engine sandbox (top-down demo) headless=%d frames=%d autopilot=%d",
                 cfg.headless, cfg.frames, autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Top-Down Demo";
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

    game::Tilemap map;
    buildMap(map);
    const float tile = map.tileSize();

    render::TextureHandle tileset = renderer->createTexture(kTilePx * kTilesetCount, kTilePx,
                                                            makeTileset().data());
    const auto playerPixels = makePlayer();
    render::TextureHandle playerTex = renderer->createTexture(16, 16, playerPixels.data());

    // Player state, in world pixels. Spawn on the cleared center tile.
    const float playerSize = 26.0f;
    float px = 24.0f * tile + (tile - playerSize) * 0.5f;
    float py = 18.0f * tile + (tile - playerSize) * 0.5f;
    const float speed = 220.0f;
    const float zoom = 2.0f;

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            float dx = 0.0f, dy = 0.0f;
            if (autopilot) {
                const float t = static_cast<float>(clock.elapsed());
                dx = std::cos(t * 0.7f);
                dy = std::sin(t * 1.1f);
            } else {
                if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT)) dx -= 1.0f;
                if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT)) dx += 1.0f;
                if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP)) dy -= 1.0f;
                if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN)) dy += 1.0f;
            }
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0001f) {
                dx = dx / len * speed * dt;
                dy = dy / len * speed * dt;
                // Move axis-separately so the player slides along walls instead of sticking.
                if (!boxHitsSolid(map, px + dx, py, playerSize)) px += dx;
                if (!boxHitsSolid(map, px, py + dy, playerSize)) py += dy;
            }
        }

        // Camera follows the player.
        render::Camera2D cam;
        cam.usePixelSpace = false;
        cam.zoom = zoom;
        cam.centerX = px + playerSize * 0.5f;
        cam.centerY = py + playerSize * 0.5f;
        renderer->setCamera2D(cam);

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            uint32_t bw = 0, bh = 0;
            window.drawableSize(bw, bh);
            const float halfW = static_cast<float>(bw) * 0.5f / zoom;
            const float halfH = static_cast<float>(bh) * 0.5f / zoom;

            // Only draw tiles inside the camera view (+1 margin).
            const int minX = map.worldToTileX(cam.centerX - halfW) - 1;
            const int maxX = map.worldToTileX(cam.centerX + halfW) + 1;
            const int minY = map.worldToTileY(cam.centerY - halfH) - 1;
            const int maxY = map.worldToTileY(cam.centerY + halfH) + 1;
            for (int ty = minY; ty <= maxY; ++ty) {
                for (int tx = minX; tx <= maxX; ++tx) {
                    if (!map.inBounds(tx, ty)) continue;
                    render::SpriteDesc s;
                    s.x = static_cast<float>(tx) * tile;
                    s.y = static_cast<float>(ty) * tile;
                    s.width = tile;
                    s.height = tile;
                    tileUV(map.at(tx, ty), s);
                    renderer->drawSprite(tileset, s);
                }
            }

            render::SpriteDesc p;
            p.x = px;
            p.y = py;
            p.width = playerSize;
            p.height = playerSize;
            renderer->drawSprite(playerTex, p);

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("shutting down after %d frames (%.2fs, renderer %s)", rendered, clock.elapsed(),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
