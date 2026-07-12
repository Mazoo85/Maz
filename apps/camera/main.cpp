// Maz Engine — "CAMERA" (2D follow-camera demo)
// A world much larger than the screen: a grid of markers across a 2600x1800 level, a bordered edge,
// and an avatar that moves on a deterministic path reaching near the world's corners. A
// game::CameraController2D follows the avatar with a DEADZONE (the avatar can drift inside a central
// box without scrolling), exponential SMOOTHING (the view eases, it doesn't snap), and WORLD-BOUNDS
// clamping (the camera never scrolls past the level edge — watch the border stop at screen edges).
// The scene is drawn through the controller's world-space camera; the HUD through a pixel-space pass.
// The avatar path is a pure function of the fixed-step clock, so the render is golden-stable.
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

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.85f ? (1.0f - d) / 0.15f : 1.0f);
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

// Draw a rectangle outline (world or pixel space — just sprites) as four thin bars.
void drawRectOutline(render::Renderer& r, render::TextureHandle white, float cx, float cy,
                     float halfW, float halfH, float thick, render::Color col) {
    auto bar = [&](float x, float y, float w, float h) {
        render::SpriteDesc d;
        d.x = x;
        d.y = y;
        d.width = w;
        d.height = h;
        d.color = col;
        r.drawSprite(white, d);
    };
    bar(cx - halfW, cy - halfH, halfW * 2.0f, thick);          // top
    bar(cx - halfW, cy + halfH - thick, halfW * 2.0f, thick);  // bottom
    bar(cx - halfW, cy - halfH, thick, halfH * 2.0f);          // left
    bar(cx + halfW - thick, cy - halfH, thick, halfH * 2.0f);  // right
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CAMERA (2D follow-camera demo) starting");

    const float worldW = 2600.0f, worldH = 1800.0f;

    game::CameraController2D camera;
    camera.setBounds({0.0f, 0.0f}, {worldW, worldH});
    camera.setDeadzone({150.0f, 95.0f});
    camera.setSmoothing(6.0f);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Follow Camera";
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
    render::TextureHandle disc = discTex(*renderer, 48);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // Avatar starts at world center; snap the camera there so the first frame is settled.
    math::vec2 avatar{worldW * 0.5f, worldH * 0.5f};
    camera.setViewport(static_cast<float>(cfg.width), static_cast<float>(cfg.height), 1.0f);
    camera.follow(avatar);
    camera.snap();
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
        camera.setViewport(sw, sh, 1.0f);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            t += dt;
            // Deterministic path sweeping near the world corners.
            avatar.x = worldW * 0.5f + 1080.0f * std::sin(t * 0.55f);
            avatar.y = worldH * 0.5f + 760.0f * std::sin(t * 0.9f + 1.0f);
            camera.follow(avatar);
            camera.update(dt);
        }

        renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            // --- World-space pass through the follow camera ---------------------------------------
            render::Camera2D worldCam;
            worldCam.usePixelSpace = false;
            worldCam.zoom = camera.zoom();
            const math::vec2 cc = camera.center();
            worldCam.centerX = cc.x;
            worldCam.centerY = cc.y;
            renderer->setCamera2D(worldCam);

            // Marker grid across the world (only those near the view matter, but drawing all is fine).
            const float step = 130.0f;
            for (float gy = step; gy < worldH; gy += step) {
                for (float gx = step; gx < worldW; gx += step) {
                    render::SpriteDesc d;
                    d.width = 8.0f;
                    d.height = 8.0f;
                    d.x = gx - 4.0f;
                    d.y = gy - 4.0f;
                    d.color = render::Color{0.28f, 0.32f, 0.42f, 1.0f};
                    renderer->drawSprite(disc, d);
                }
            }

            // World border (clamping makes this stop at the screen edge instead of scrolling past).
            drawRectOutline(*renderer, white, worldW * 0.5f, worldH * 0.5f, worldW * 0.5f,
                            worldH * 0.5f, 6.0f, render::Color{0.45f, 0.55f, 0.8f, 1.0f});

            // Deadzone box around the follow focus (the avatar rides inside this without scrolling).
            const math::vec2 focus = camera.position();
            drawRectOutline(*renderer, white, focus.x, focus.y, 150.0f, 95.0f, 2.0f,
                            render::Color{0.5f, 0.9f, 0.6f, 0.7f});

            // Avatar.
            {
                const float sz = 46.0f;
                render::SpriteDesc d;
                d.x = avatar.x - sz * 0.5f;
                d.y = avatar.y - sz * 0.5f;
                d.width = sz;
                d.height = sz;
                d.color = render::Color{1.0f, 0.75f, 0.35f, 1.0f};
                renderer->drawSprite(disc, d);
            }

            // --- Pixel-space HUD pass --------------------------------------------------------------
            render::Camera2D hud;
            hud.usePixelSpace = true;
            renderer->setCamera2D(hud);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D FOLLOW CAMERA",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "follow + deadzone + world bounds   |   cam (%.0f, %.0f)   avatar (%.0f, %.0f)",
                          static_cast<double>(cc.x), static_cast<double>(cc.y),
                          static_cast<double>(avatar.x), static_cast<double>(avatar.y));
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CAMERA shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
