// Maz Engine — "MENU" (immediate-mode UI demo)
// A settings menu built from maz::ui::Context widgets: a panel, buttons, a toggle, and a volume
// slider. A synthetic auto-cursor drives it (deterministic under the fixed timestep, so CI/golden
// capture is stable); the moment a real mouse moves, control hands over to it. Proves the engine
// has interactive UI, not just static text. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MENU (immediate-mode UI demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — UI";
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

    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    ui::Context gui;
    gui.init(*renderer, font, white);

    // Menu state the widgets mutate.
    bool fullscreen = true;
    bool vsync = false;
    float volume = 0.6f;
    const char* lastAction = "-";
    bool useReal = false; // flips to true once a real mouse moves

    float t = 0.0f;
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
            t += static_cast<float>(clock.fixedDelta());
        }

        // Panel geometry (centered).
        const float pw = 360.0f, ph = 400.0f;
        const float px = sw * 0.5f - pw * 0.5f;
        const float py = sh * 0.5f - ph * 0.5f;

        // Pointer: real mouse once it moves, otherwise a scripted auto-cursor sweeping the panel.
        if (input.mouseDX() != 0.0f || input.mouseDY() != 0.0f || input.mouseDown(0)) {
            useReal = true;
        }
        float pointerX, pointerY;
        bool pointerDown;
        if (useReal) {
            pointerX = input.mouseX();
            pointerY = input.mouseY();
            pointerDown = input.mouseDown(0);
        } else {
            pointerX = px + pw * 0.5f + std::sin(t * 0.9f) * pw * 0.42f;
            pointerY = py + ph * 0.55f + std::sin(t * 0.6f + 1.0f) * ph * 0.40f;
            pointerDown = std::fmod(t, 1.6f) > 1.35f; // brief click pulses
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            gui.begin(pointerX, pointerY, pointerDown);

            // Backdrop panel + title.
            gui.panel(ui::Rect{px, py, pw, ph}, gui.colBg);
            font.drawText(*renderer, px + 24.0f, py + 20.0f, "SETTINGS", render::Color{1, 1, 1, 1}, 0.75f);

            float y = py + 74.0f;
            const float bx = px + 24.0f, bwid = pw - 48.0f;
            if (gui.button(1, ui::Rect{bx, y, bwid, 42.0f}, "Play")) {
                lastAction = "Play";
            }
            y += 54.0f;
            if (gui.button(2, ui::Rect{bx, y, bwid, 42.0f}, "New Game")) {
                lastAction = "New Game";
            }
            y += 54.0f;
            if (gui.button(3, ui::Rect{bx, y, bwid, 42.0f}, "Quit")) {
                lastAction = "Quit";
            }

            y += 66.0f;
            gui.toggle(4, ui::Rect{bx, y, 28.0f, 28.0f}, "Fullscreen", fullscreen);
            y += 40.0f;
            gui.toggle(5, ui::Rect{bx, y, 28.0f, 28.0f}, "VSync", vsync);

            y += 52.0f;
            font.drawText(*renderer, bx, y - 4.0f, "Volume", render::Color{0.85f, 0.9f, 1.0f, 1}, 0.45f);
            gui.slider(6, ui::Rect{bx + 96.0f, y + 4.0f, bwid - 96.0f, 14.0f}, volume, 0.0f, 1.0f);

            gui.end();

            // Status line + the auto-cursor marker.
            char buf[96];
            std::snprintf(buf, sizeof(buf), "last: %s   vol: %d%%   fs:%s", lastAction,
                          static_cast<int>(volume * 100.0f + 0.5f), fullscreen ? "on" : "off");
            font.drawText(*renderer, px + 24.0f, py + ph - 30.0f, buf,
                          render::Color{0.7f, 0.8f, 0.95f, 1}, 0.42f);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  IMMEDIATE-MODE UI",
                          render::Color{1, 1, 1, 1}, 0.6f);
            if (!useReal) {
                // Draw the synthetic cursor as a small bright square.
                render::SpriteDesc cur;
                cur.x = pointerX - 5.0f;
                cur.y = pointerY - 5.0f;
                cur.width = 10.0f;
                cur.height = 10.0f;
                cur.color = pointerDown ? render::Color{1.0f, 0.9f, 0.3f, 1} : render::Color{1, 1, 1, 1};
                renderer->drawSprite(white, cur);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MENU shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
