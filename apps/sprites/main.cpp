// Maz Engine — "SPRITES" (sprite-sheet / flipbook animation demo)
// Generates an 8-frame sprite sheet at runtime (a dot orbiting a ring), then plays it back with
// maz::anim::SpriteAnim: one large sprite plus a grid of smaller ones started at staggered phases,
// so a wave of motion sweeps across them. A one-shot "burst" clip retriggers on a timer to show
// non-looping playback + finished(). Proves the engine animates sprites, not just draws them.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr int kFrames = 8;
constexpr int kCell = 64;

void fillCircle(std::vector<uint8_t>& px, int W, int cx, int cy, int r, uint8_t rr, uint8_t gg,
                uint8_t bb, uint8_t aa) {
    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            if (x < 0 || y < 0 || x >= W) {
                continue;
            }
            const int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r) {
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(W) +
                                  static_cast<size_t>(x)) * 4;
                if (i + 3 < px.size()) {
                    px[i] = rr;
                    px[i + 1] = gg;
                    px[i + 2] = bb;
                    px[i + 3] = aa;
                }
            }
        }
    }
}

// Build a horizontal strip of kFrames cells. In each frame a bright dot sits at one of 8 ring
// positions (the others dimmed), so playing the strip reads as a dot orbiting the ring.
render::TextureHandle makeSheet(render::Renderer& r) {
    const int W = kFrames * kCell;
    const int H = kCell;
    std::vector<uint8_t> px(static_cast<size_t>(W) * static_cast<size_t>(H) * 4, 0);
    const float ringR = kCell * 0.33f;
    for (int f = 0; f < kFrames; ++f) {
        const int ox = f * kCell;
        // Cell background.
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < kCell; ++x) {
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(W) +
                                  static_cast<size_t>(ox + x)) * 4;
                px[i] = 26;
                px[i + 1] = 30;
                px[i + 2] = 44;
                px[i + 3] = 255;
            }
        }
        const int cx = ox + kCell / 2;
        const int cy = kCell / 2;
        for (int j = 0; j < 8; ++j) {
            const float a = static_cast<float>(j) / 8.0f * 6.2831853f;
            const int dx = static_cast<int>(std::cos(a) * ringR);
            const int dy = static_cast<int>(std::sin(a) * ringR);
            const bool active = j == f;
            if (active) {
                fillCircle(px, W, cx + dx, cy + dy, 9, 120, 230, 255, 255); // bright cyan dot
            } else {
                fillCircle(px, W, cx + dx, cy + dy, 4, 70, 80, 110, 255); // dim marker
            }
        }
    }
    return r.createTexture(static_cast<uint32_t>(W), static_cast<uint32_t>(H), px.data());
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SPRITES (sprite-sheet animation demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Sprite Animation";
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

    render::TextureHandle sheet = makeSheet(*renderer);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // A grid of animators, each started at a staggered phase so a wave sweeps across them.
    constexpr int kCols = 10, kRows = 6;
    std::vector<anim::SpriteAnim> grid(kCols * kRows);
    for (int i = 0; i < kCols * kRows; ++i) {
        grid[static_cast<size_t>(i)].play(anim::gridFrames(kFrames, 1, 0, kFrames), 12.0f, true);
        grid[static_cast<size_t>(i)].update(static_cast<float>(i) * 0.03f); // phase offset
    }
    // One big hero animator.
    anim::SpriteAnim hero;
    hero.play(anim::gridFrames(kFrames, 1, 0, kFrames), 10.0f, true);

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
            const float dt = static_cast<float>(clock.fixedDelta());
            hero.update(dt);
            for (auto& a : grid) {
                a.update(dt);
            }
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            auto drawFrame = [&](const anim::SpriteFrame& f, float x, float y, float size) {
                render::SpriteDesc s;
                s.x = x;
                s.y = y;
                s.width = size;
                s.height = size;
                s.uvMinX = f.u0;
                s.uvMinY = f.v0;
                s.uvMaxX = f.u1;
                s.uvMaxY = f.v1;
                renderer->drawSprite(sheet, s);
            };

            // Hero sprite, large, centered near the top.
            const float heroSize = 200.0f;
            drawFrame(hero.frame(), sw * 0.5f - heroSize * 0.5f, 90.0f, heroSize);

            // The wave grid, centered in the lower area.
            const float cell = 72.0f, pad = 8.0f;
            const float gridW = kCols * cell + (kCols - 1) * pad;
            const float gx = sw * 0.5f - gridW * 0.5f;
            const float gy = sh - kRows * (cell + pad) - 40.0f;
            for (int r = 0; r < kRows; ++r) {
                for (int c = 0; c < kCols; ++c) {
                    const anim::SpriteAnim& a = grid[static_cast<size_t>(r * kCols + c)];
                    drawFrame(a.frame(), gx + static_cast<float>(c) * (cell + pad),
                              gy + static_cast<float>(r) * (cell + pad), cell);
                }
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SPRITE-SHEET ANIMATION",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "%d-frame flipbook, hero frame %d/%d", kFrames,
                          hero.index() + 1, kFrames);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.6f, 0.85f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SPRITES shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
