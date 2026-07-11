// Maz Engine — "ASSETS" (resource-cache / asset-manager demo)
// Builds a mosaic of 240 tiles that only use 8 distinct colors. Every tile's texture is requested
// through a maz::core::ResourceCache keyed by color name, so the repeated colors are created on the
// GPU exactly once and every later request is a cache hit. The HUD reports requests vs. unique
// loads vs. hits, proving the dedup. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ASSETS (resource-cache demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Asset Cache";
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

    // A small named palette — these are the only distinct "assets" in play.
    struct Named {
        const char* name;
        uint8_t r, g, b;
    };
    const Named palette[8] = {
        {"crimson", 210, 70, 80},  {"leaf", 90, 190, 110}, {"sky", 90, 150, 230},
        {"gold", 225, 190, 70},    {"grape", 170, 90, 200}, {"teal", 70, 200, 200},
        {"coral", 235, 130, 90},   {"slate", 120, 130, 150}};

    // The cache: keyed by color name, valued by the GPU texture handle. The loader creates the
    // 1x1 texture; it runs at most once per distinct name.
    core::ResourceCache<std::string, render::TextureHandle> cache;
    auto loadColor = [&](const Named& n) {
        return [&, n]() {
            const uint8_t px[4] = {n.r, n.g, n.b, 255};
            return renderer->createTexture(1, 1, px);
        };
    };

    // Lay out the mosaic and acquire each tile's texture through the cache (populated once here).
    constexpr int kCols = 20, kRows = 12; // 240 tiles
    std::vector<render::TextureHandle> tiles(kCols * kRows);
    for (int y = 0; y < kRows; ++y) {
        for (int x = 0; x < kCols; ++x) {
            // A diagonal-band pattern so each color repeats many times across the grid.
            const int ci = ((x + y * 2) / 2) % 8;
            const Named& n = palette[ci];
            tiles[static_cast<size_t>(y * kCols + x)] = cache.acquire(n.name, loadColor(n));
        }
    }

    const int requests = kCols * kRows;
    const uint64_t uniqueLoads = cache.loads();
    const uint64_t hits = cache.hits();
    MAZ_LOG_INFO("ASSETS %d tile requests -> %llu unique textures loaded, %llu cache hits", requests,
                 static_cast<unsigned long long>(uniqueLoads), static_cast<unsigned long long>(hits));

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
        const float sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float top = 88.0f;
            const float tileW = sw / static_cast<float>(kCols);
            const float tileH = (sh - top - 20.0f) / static_cast<float>(kRows);
            const float gap = 3.0f;
            for (int y = 0; y < kRows; ++y) {
                for (int x = 0; x < kCols; ++x) {
                    render::SpriteDesc s;
                    s.x = static_cast<float>(x) * tileW + gap * 0.5f;
                    s.y = top + static_cast<float>(y) * tileH + gap * 0.5f;
                    s.width = tileW - gap;
                    s.height = tileH - gap;
                    renderer->drawSprite(tiles[static_cast<size_t>(y * kCols + x)], s);
                }
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RESOURCE CACHE (ASSET DEDUP)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[160];
            const int saved = requests > 0
                                  ? static_cast<int>(100.0 * static_cast<double>(hits) /
                                                     static_cast<double>(requests) + 0.5)
                                  : 0;
            std::snprintf(buf, sizeof(buf),
                          "%d tile requests  ->  %llu GPU textures loaded  +  %llu cache hits (%d%% saved)",
                          requests, static_cast<unsigned long long>(uniqueLoads),
                          static_cast<unsigned long long>(hits), saved);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.75f, 0.95f, 0.8f, 1}, 0.48f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ASSETS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
