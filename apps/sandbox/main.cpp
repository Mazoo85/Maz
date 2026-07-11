// Maz Engine — Sandbox
// The M0 walking skeleton: open a window, run a fixed-timestep loop, clear the screen to an
// animated color, and shut down cleanly. Run with --headless (or --frames N) for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <vector>

using namespace maz;

namespace {

// Build a simple 2-color checkerboard RGBA8 texture so the demo needs no asset files.
std::vector<uint8_t> makeCheckerboard(uint32_t size, uint32_t cell) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const bool on = ((x / cell) + (y / cell)) % 2 == 0;
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            px[i + 0] = on ? 240 : 40;
            px[i + 1] = on ? 240 : 40;
            px[i + 2] = on ? 240 : 60;
            px[i + 3] = 255;
        }
    }
    return px;
}

struct Mover {
    float x, y, vx, vy, size, spin, rot;
    render::Color tint;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("Maz Engine sandbox starting (headless=%d, frames=%d)", cfg.headless, cfg.frames);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = cfg.title;
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        MAZ_LOG_ERROR("window init failed");
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
#if defined(MAZ_DEBUG)
    rc.enableValidation = !cfg.headless; // no ICD under the dummy driver
#endif
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        MAZ_LOG_ERROR("renderer init failed");
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    // A checkerboard texture + a handful of sprites bouncing around the window.
    const auto checker = makeCheckerboard(64, 8);
    render::TextureHandle tex = renderer->createTexture(64, 64, checker.data());

    Mover movers[] = {
        {120.0f, 100.0f, 140.0f, 90.0f, 96.0f, 1.2f, 0.0f, {1.0f, 0.4f, 0.7f, 1.0f}},
        {600.0f, 300.0f, -110.0f, 120.0f, 128.0f, -0.8f, 0.0f, {0.3f, 0.9f, 1.0f, 1.0f}},
        {900.0f, 500.0f, 90.0f, -140.0f, 80.0f, 2.0f, 0.0f, {0.6f, 1.0f, 0.4f, 1.0f}},
    };

    // Simulation state advanced on the fixed step (frame-rate independent).
    float hue = 0.0f;

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (window.consumeResized()) {
            uint32_t w = 0, h = 0;
            window.drawableSize(w, h);
            renderer->onResize(w, h);
        }
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t vw = 0, vh = 0;
        window.drawableSize(vw, vh);
        const float bw = vw > 0 ? static_cast<float>(vw) : static_cast<float>(cfg.width);
        const float bh = vh > 0 ? static_cast<float>(vh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            hue += dt; // one unit ~= 1 rad/sec
            for (Mover& m : movers) {
                m.x += m.vx * dt;
                m.y += m.vy * dt;
                m.rot += m.spin * dt;
                if (m.x < 0.0f) { m.x = 0.0f; m.vx = -m.vx; }
                if (m.y < 0.0f) { m.y = 0.0f; m.vy = -m.vy; }
                if (m.x + m.size > bw) { m.x = bw - m.size; m.vx = -m.vx; }
                if (m.y + m.size > bh) { m.y = bh - m.size; m.vy = -m.vy; }
            }
        }

        render::Color clear;
        clear.r = 0.5f + 0.5f * std::sin(hue);
        clear.g = 0.5f + 0.5f * std::sin(hue + 2.094f); // +120 deg
        clear.b = 0.5f + 0.5f * std::sin(hue + 4.188f); // +240 deg
        renderer->setClearColor(clear);
        if (renderer->beginFrame()) {
            for (const Mover& m : movers) {
                render::SpriteDesc s;
                s.x = m.x;
                s.y = m.y;
                s.width = m.size;
                s.height = m.size;
                s.rotation = m.rot;
                s.color = m.tint;
                renderer->drawSprite(tex, s);
            }
            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            MAZ_LOG_INFO("reached frame cap (%d); exiting", cfg.frames);
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("shutting down after %d frames (%.2fs, renderer %s)", rendered, clock.elapsed(),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
