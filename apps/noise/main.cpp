// Maz Engine — "NOISE" (procedural terrain from Perlin/fbm noise)
// A terrain heightmap generated entirely from core::Noise: for each texel, fractal Brownian motion
// (several octaves of Perlin noise) gives a height, mapped through a color ramp (deep water -> water
// -> sand -> grass -> forest -> rock -> snow) with a cheap slope-based hillshade for relief. The field
// is seeded, so the same seed always yields the same continent. It's generated once at startup into a
// texture and drawn full-screen, so the render is deterministic and golden-stable.
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

struct Rgb {
    float r, g, b;
};

Rgb lerpRgb(Rgb a, Rgb b, float t) {
    return Rgb{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

// Height (h in ~[-1,1]) -> terrain color via a banded ramp with soft transitions.
Rgb terrainColor(float h) {
    const float t = (h + 1.0f) * 0.5f; // [0,1]
    struct Stop {
        float t;
        Rgb c;
    };
    static const Stop stops[] = {
        {0.00f, {0.04f, 0.10f, 0.28f}}, // deep water
        {0.34f, {0.10f, 0.32f, 0.60f}}, // water
        {0.44f, {0.20f, 0.52f, 0.78f}}, // shallow
        {0.47f, {0.80f, 0.74f, 0.48f}}, // sand
        {0.52f, {0.34f, 0.62f, 0.30f}}, // grass
        {0.66f, {0.20f, 0.46f, 0.22f}}, // forest
        {0.80f, {0.45f, 0.40f, 0.34f}}, // rock
        {0.90f, {0.60f, 0.58f, 0.55f}}, // high rock
        {1.00f, {0.95f, 0.96f, 0.98f}}, // snow
    };
    const int n = static_cast<int>(sizeof(stops) / sizeof(stops[0]));
    if (t <= stops[0].t) return stops[0].c;
    for (int i = 1; i < n; ++i) {
        if (t <= stops[i].t) {
            const float span = stops[i].t - stops[i - 1].t;
            const float f = span > 0.0f ? (t - stops[i - 1].t) / span : 0.0f;
            return lerpRgb(stops[i - 1].c, stops[i].c, f);
        }
    }
    return stops[n - 1].c;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("NOISE (procedural terrain) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Procedural Terrain";
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

    // --- Generate the terrain texture once from a seeded fbm field --------------------------------
    const uint64_t seed = 0xA11CE5EEDULL;
    const int octaves = 6;
    const int TW = 480, TH = 270;
    const float scale = 0.012f; // domain frequency (lower = larger features)
    core::Noise noise(seed);
    std::vector<uint8_t> pixels(static_cast<size_t>(TW) * static_cast<size_t>(TH) * 4);
    for (int y = 0; y < TH; ++y) {
        for (int x = 0; x < TW; ++x) {
            const float nx = static_cast<float>(x) * scale;
            const float ny = static_cast<float>(y) * scale;
            const float h = noise.fbm2(nx, ny, octaves);
            // Slope-based hillshade from a forward difference of the height field.
            const float hR = noise.fbm2(nx + scale, ny, octaves);
            const float hD = noise.fbm2(nx, ny + scale, octaves);
            const float slope = (h - hR) + (h - hD);
            const float shade = clampf(0.85f + slope * 4.0f, 0.55f, 1.2f);
            Rgb c = terrainColor(h);
            // Water stays flat-lit; land takes the hillshade.
            const bool land = (h + 1.0f) * 0.5f > 0.44f;
            const float s = land ? shade : 1.0f;
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(TW) + static_cast<size_t>(x)) * 4;
            pixels[i] = static_cast<uint8_t>(clampf(c.r * s, 0.0f, 1.0f) * 255.0f);
            pixels[i + 1] = static_cast<uint8_t>(clampf(c.g * s, 0.0f, 1.0f) * 255.0f);
            pixels[i + 2] = static_cast<uint8_t>(clampf(c.b * s, 0.0f, 1.0f) * 255.0f);
            pixels[i + 3] = 255;
        }
    }
    render::TextureHandle terrain =
        renderer->createTexture(static_cast<uint32_t>(TW), static_cast<uint32_t>(TH), pixels.data());
    MAZ_LOG_INFO("generated %dx%d terrain (%d octaves) from seed 0x%llX", TW, TH, octaves,
                 static_cast<unsigned long long>(seed));

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
            // Static scene.
        }

        renderer->setClearColor(render::Color{0.0f, 0.0f, 0.0f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Draw the terrain texture full-screen (bilinear upscale).
            render::SpriteDesc d;
            d.x = 0.0f;
            d.y = 0.0f;
            d.width = sw;
            d.height = sh;
            d.color = render::Color{1, 1, 1, 1};
            renderer->drawSprite(terrain, d);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PROCEDURAL TERRAIN (Perlin fbm)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char sub[112];
            std::snprintf(sub, sizeof(sub),
                          "fbm noise, %d octaves, seed 0x%llX  -  same seed, same continent", octaves,
                          static_cast<unsigned long long>(seed));
            font.drawText(*renderer, 16.0f, 46.0f, sub, render::Color{0.9f, 0.95f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("NOISE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
