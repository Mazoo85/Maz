// Maz Engine — "CONFIG" (cvar / config-system demo)
// Every subsystem registers named, typed, documented tunables in a core::CVarRegistry. A JSON config
// document then overrides a subset of them (io::loadConfig), exactly as an on-disk config.json would.
// The scene is driven entirely by those cvars: the number of orbiting dots, their orbit speed, their
// hue, the HDR-ish brightness, and whether a background grid is drawn all come from the registry — and
// the full cvar table is listed live on the left, name = value, so you can see the config that produced
// the picture. The orbit is a pure function of the fixed-step clock, so the render is golden-stable.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
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
            const float a = d >= 1.0f ? 0.0f : (d > 0.9f ? (1.0f - d) / 0.1f : 1.0f);
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

// HSV -> RGB (h,s,v in 0..1), used so a single "hue" cvar drives the palette.
render::Color hsv(float h, float s, float v, float a = 1.0f) {
    h = h - std::floor(h);
    const float i = std::floor(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    float r = v, g = v, b = v;
    switch (static_cast<int>(i) % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    }
    return render::Color{r, g, b, a};
}

// The config document. In a shipping game this is config.json on disk (see io::loadConfigFile);
// embedding it keeps the demo self-contained and the golden reproducible.
const char* kConfigJson = R"JSON(
{
  "app.title": "CONFIG-DRIVEN SCENE",
  "scene.orbCount": 14,
  "scene.orbSpeed": 0.6,
  "scene.hue": 0.08,
  "render.brightness": 1.15,
  "ui.showGrid": true
}
)JSON";

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CONFIG (cvar demo) starting");

    // --- Register the engine/app cvars (defaults + descriptions + ranges) ------------------------
    core::CVarRegistry cvars;
    cvars.registerString("app.title", "MAZ CONFIG", "HUD title");
    cvars.registerInt("scene.orbCount", 8, "number of orbiting dots");
    cvars.setRange("scene.orbCount", 1, 24);
    cvars.registerFloat("scene.orbSpeed", 1.0f, "orbit angular speed");
    cvars.setRange("scene.orbSpeed", 0.0, 4.0);
    cvars.registerFloat("scene.hue", 0.55f, "base palette hue");
    cvars.setRange("scene.hue", 0.0, 1.0);
    cvars.registerFloat("render.brightness", 1.0f, "dot brightness");
    cvars.setRange("render.brightness", 0.2, 2.0);
    cvars.registerBool("ui.showGrid", false, "draw background grid");

    // Overrides come from the JSON config (io::loadConfig). applyAssignments("name=value", ...) is
    // available for command-line/CLI overrides too, but this demo drives everything from the config.
    auto parsed = io::parseJson(kConfigJson);
    const int applied = parsed.ok ? io::loadConfig(cvars, parsed.value) : 0;
    MAZ_LOG_INFO("config applied %d cvars from JSON", applied);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Config / CVars";
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

        // Read the cvars that drive the scene.
        const int orbCount = cvars.getInt("scene.orbCount");
        const float orbSpeed = cvars.getFloat("scene.orbSpeed");
        const float hue = cvars.getFloat("scene.hue");
        const float brightness = cvars.getFloat("render.brightness");
        const bool showGrid = cvars.getBool("ui.showGrid");

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float cx = sw * 0.62f;
            const float cy = sh * 0.52f;

            // Optional background grid (a cvar toggle).
            if (showGrid) {
                const float step = 56.0f;
                const render::Color line{0.16f, 0.18f, 0.24f, 1.0f};
                for (float gx = cx - 300.0f; gx <= cx + 300.0f; gx += step) {
                    render::SpriteDesc d;
                    d.x = gx;
                    d.y = cy - 260.0f;
                    d.width = 1.5f;
                    d.height = 520.0f;
                    d.color = line;
                    renderer->drawSprite(white, d);
                }
                for (float gy = cy - 260.0f; gy <= cy + 260.0f; gy += step) {
                    render::SpriteDesc d;
                    d.x = cx - 300.0f;
                    d.y = gy;
                    d.width = 600.0f;
                    d.height = 1.5f;
                    d.color = line;
                    renderer->drawSprite(white, d);
                }
            }

            // The ring of orbiting dots — count, speed, hue, brightness all cvar-driven.
            const float radius = 190.0f;
            for (int k = 0; k < orbCount; ++k) {
                const float frac = static_cast<float>(k) / static_cast<float>(orbCount);
                const float ang = t * orbSpeed + frac * 6.2831853f;
                const float ox = cx + std::cos(ang) * radius;
                const float oy = cy + std::sin(ang) * radius;
                const float sz = 34.0f;
                render::Color col = hsv(hue + frac * 0.5f, 0.7f, std::min(1.0f, 0.75f * brightness));
                render::SpriteDesc d;
                d.x = ox - sz * 0.5f;
                d.y = oy - sz * 0.5f;
                d.width = sz;
                d.height = sz;
                d.color = col;
                renderer->drawSprite(disc, d);
            }

            // HUD title (from a string cvar) + the live cvar table on the left.
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CONFIG / CVARS",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f, cvars.getString("app.title").c_str(),
                          render::Color{0.7f, 0.85f, 1.0f, 1}, 0.5f);

            float ty = 110.0f;
            for (const auto& e : cvars.entries()) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%-18s = %s", e.name.c_str(),
                              cvars.valueString(e).c_str());
                font.drawText(*renderer, 16.0f, ty, buf, render::Color{0.62f, 0.7f, 0.82f, 1}, 0.44f);
                ty += 30.0f;
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CONFIG shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
