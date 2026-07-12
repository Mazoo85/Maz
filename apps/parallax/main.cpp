// Maz Engine — "PARALLAX" (multi-layer scrolling backgrounds, toward Godot's ParallaxBackground)
// A parallax background is several layers that scroll at DIFFERENT rates so the scene reads as deep. You
// can't feel scrolling in a still image, so this shows the SAME five-layer scene in three stacked strips,
// each at a different camera scroll (0, 460, 920). Look down the columns: the far mountains barely shift
// between strips while the near trees sweep a long way — that difference IS the parallax. Every layer is
// also MIRRORED (its motif tiles seamlessly across the width) so a finite scene covers any scroll. Each
// layer's on-screen offset + tile positions come from game::Parallax; the render is deterministic (fixed
// scrolls, drawn statically) so it's golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

void fillTri(render::Renderer& r, math::vec2 a, math::vec2 b, math::vec2 c, render::Color col) {
    const render::Point2 p[3] = {{a.x, a.y}, {b.x, b.y}, {c.x, c.y}};
    r.drawConvexPolygon(p, 3, col);
}

// A soft round-ish blob (hexagon) — used for clouds and tree canopies without per-pixel circles.
void blob(render::Renderer& r, math::vec2 c, float rx, float ry, render::Color col) {
    render::Point2 p[8];
    p[0] = {c.x, c.y};
    const int seg = 7;
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i] = {c.x + std::cos(a) * rx, c.y + std::sin(a) * ry};
    }
    r.drawConvexPolygon(p, 8, col);
}

struct Layers {
    game::ParallaxLayer clouds, mountains, hills, trees;
};

// Draw one strip of the scene at camera scroll `scroll`. All horizontal placement flows through
// game::Parallax so the layers scroll by their motionScale and tile by their mirroring period.
void drawStrip(render::Renderer& r, render::TextureHandle white, const Layers& L, float sx, float sy,
               float sw, float sh, float scroll) {
    const float groundY = sy + sh - 22.0f;

    // Sky (two bands) — a motionScale-0 backdrop that never moves.
    fillRect(r, white, sx, sy, sw, sh, render::Color{0.36f, 0.55f, 0.78f, 1.0f});
    fillRect(r, white, sx, sy, sw, sh * 0.45f, render::Color{0.29f, 0.45f, 0.72f, 1.0f});

    // Sun — pinned to the sky (scale 0), so it sits at the same screen spot in every strip.
    const float sunX = sx + game::layerOffset({math::vec2(0, 0), math::vec2(150, 0), {}}, math::vec2(scroll, 0)).x;
    blob(r, math::vec2(sunX, sy + 46.0f), 26.0f, 26.0f, render::Color{1.0f, 0.9f, 0.55f, 1.0f});

    auto tiled = [&](const game::ParallaxLayer& layer, float period, auto motif) {
        const float off = game::layerOffset(layer, math::vec2(scroll, 0)).x;
        const float ft = game::firstTile(off, period);
        const int n = game::tileCount(sw, period);
        for (int k = 0; k < n; ++k) {
            motif(sx + ft + static_cast<float>(k) * period);
        }
    };

    // Clouds — very slow (scale ~0.1), period 360.
    tiled(L.clouds, 360.0f, [&](float x) {
        blob(r, math::vec2(x + 90.0f, sy + 40.0f), 34.0f, 15.0f, render::Color{0.92f, 0.94f, 0.98f, 0.9f});
        blob(r, math::vec2(x + 128.0f, sy + 34.0f), 24.0f, 13.0f, render::Color{0.92f, 0.94f, 0.98f, 0.9f});
    });

    // Far mountains — scale 0.15, period 300, snow-capped triangles.
    tiled(L.mountains, 300.0f, [&](float x) {
        const float peakX = x + 150.0f;
        const float topY = groundY - 118.0f;
        fillTri(r, {x + 10.0f, groundY}, {peakX, topY}, {x + 290.0f, groundY},
                render::Color{0.42f, 0.46f, 0.56f, 1.0f});
        fillTri(r, {peakX - 26.0f, topY + 30.0f}, {peakX, topY}, {peakX + 26.0f, topY + 30.0f},
                render::Color{0.86f, 0.9f, 0.96f, 1.0f});
    });

    // Hills — scale 0.4, period 220, rounded green humps.
    tiled(L.hills, 220.0f, [&](float x) {
        blob(r, math::vec2(x + 110.0f, groundY + 6.0f), 130.0f, 64.0f, render::Color{0.28f, 0.5f, 0.34f, 1.0f});
    });

    // Trees — near, scale 0.85, period 130, trunk + canopy.
    tiled(L.trees, 130.0f, [&](float x) {
        const float tx = x + 65.0f;
        fillRect(r, white, tx - 6.0f, groundY - 40.0f, 12.0f, 44.0f, render::Color{0.35f, 0.24f, 0.16f, 1.0f});
        blob(r, math::vec2(tx, groundY - 54.0f), 30.0f, 30.0f, render::Color{0.2f, 0.44f, 0.24f, 1.0f});
        blob(r, math::vec2(tx, groundY - 74.0f), 20.0f, 20.0f, render::Color{0.24f, 0.5f, 0.28f, 1.0f});
    });

    // Ground strip (foreground).
    fillRect(r, white, sx, groundY, sw, sy + sh - groundY, render::Color{0.22f, 0.18f, 0.13f, 1.0f});
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PARALLAX (scrolling backgrounds) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Parallax Backgrounds";
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
    render::TextureHandle white = whiteTex(*renderer);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    Layers layers;
    layers.clouds.motionScale = math::vec2(0.1f, 0.0f);
    layers.clouds.mirroring = math::vec2(360.0f, 0.0f);
    layers.mountains.motionScale = math::vec2(0.15f, 0.0f);
    layers.mountains.mirroring = math::vec2(300.0f, 0.0f);
    layers.hills.motionScale = math::vec2(0.4f, 0.0f);
    layers.hills.mirroring = math::vec2(220.0f, 0.0f);
    layers.trees.motionScale = math::vec2(0.85f, 0.0f);
    layers.trees.mirroring = math::vec2(130.0f, 0.0f);

    const float scrolls[3] = {0.0f, 460.0f, 920.0f};
    const float stripX = 16.0f, stripW = 1248.0f, stripH = 190.0f;
    const float stripY0 = 92.0f, stripGap = 14.0f;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PARALLAX BACKGROUNDS",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "same scene at three camera scrolls - far layers barely move, near layers "
                          "sweep (game::Parallax)",
                          render::Color{0.78f, 0.83f, 0.93f, 1}, 0.32f);

            for (int i = 0; i < 3; ++i) {
                const float sy = stripY0 + static_cast<float>(i) * (stripH + stripGap);
                drawStrip(*renderer, white, layers, stripX, sy, stripW, stripH, scrolls[i]);
                // Border + scroll label.
                fillRect(*renderer, white, stripX, sy, stripW, 2.0f, render::Color{0.5f, 0.55f, 0.62f, 0.8f});
                char buf[32];
                std::snprintf(buf, sizeof(buf), "scroll = %d", static_cast<int>(scrolls[i]));
                font.drawText(*renderer, stripX + 10.0f, sy + 8.0f, buf,
                              render::Color{1.0f, 1.0f, 1.0f, 0.9f}, 0.3f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PARALLAX shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
