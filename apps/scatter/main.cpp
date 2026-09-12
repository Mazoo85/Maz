// Maz Engine — "SCATTER" (deterministic procedural generation demo)
// A field of tokens scattered with a single seeded core::Random: positions sampled uniformly in a
// disc, each token's rarity chosen by weighted() (Common 70 / Uncommon 20 / Rare 8 / Epic 2) which
// drives its color and size. Because the RNG is deterministic, the SAME seed always produces the SAME
// field — the basis for shareable seeds, replays, and reproducible tests. A legend on the left tallies
// how many of each rarity were rolled, so you can see the weighted distribution the generator produced.
// The scene is generated once at startup, so the render is trivially golden-stable.
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

render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.82f ? (1.0f - d) / 0.18f : 1.0f);
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

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

struct Token {
    float x, y, size;
    int rarity;
};

const render::Color kRarityColor[4] = {
    {0.55f, 0.58f, 0.64f, 1.0f}, // Common  (gray)
    {0.40f, 0.85f, 0.50f, 1.0f}, // Uncommon (green)
    {0.40f, 0.65f, 1.00f, 1.0f}, // Rare     (blue)
    {1.00f, 0.80f, 0.30f, 1.0f}, // Epic     (gold)
};
const char* kRarityName[4] = {"Common", "Uncommon", "Rare", "Epic"};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SCATTER (procedural RNG demo) starting");

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);

    // --- Generate the field once from a fixed seed ------------------------------------------------
    const uint64_t seed = 0x5EED1234ULL;
    core::Random rng(seed);
    const std::vector<float> weights{70.0f, 20.0f, 8.0f, 2.0f};
    const float sizeLo[4] = {9.0f, 13.0f, 18.0f, 26.0f};
    const float sizeHi[4] = {15.0f, 20.0f, 26.0f, 38.0f};

    const float cx = sw * 0.62f, cy = sh * 0.54f;
    const float R = std::min(sw, sh) * 0.42f;

    std::vector<Token> tokens;
    int counts[4] = {0, 0, 0, 0};
    const int total = 520;
    for (int i = 0; i < total; ++i) {
        Token t;
        // Uniform-in-disc sampling: radius scaled by sqrt so density is even.
        const float rad = R * std::sqrt(rng.nextFloat());
        const float ang = rng.nextAngle();
        t.x = cx + std::cos(ang) * rad;
        t.y = cy + std::sin(ang) * rad;
        t.rarity = static_cast<int>(rng.weighted(weights));
        t.size = rng.range(sizeLo[t.rarity], sizeHi[t.rarity]);
        tokens.push_back(t);
        ++counts[t.rarity];
    }
    MAZ_LOG_INFO("scattered %d tokens (C:%d U:%d R:%d E:%d)", total, counts[0], counts[1], counts[2],
                 counts[3]);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Procedural Scatter";
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

    render::TextureHandle disc = discTex(*renderer, 64);
    render::TextureHandle white = whiteTex(*renderer);

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

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            // Static scene; nothing to advance.
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Draw rarer tokens last so they sit on top.
            for (int pass = 0; pass < 4; ++pass) {
                for (const Token& t : tokens) {
                    if (t.rarity != pass) continue;
                    render::SpriteDesc d;
                    d.x = t.x - t.size * 0.5f;
                    d.y = t.y - t.size * 0.5f;
                    d.width = t.size;
                    d.height = t.size;
                    d.color = kRarityColor[t.rarity];
                    renderer->drawSprite(disc, d);
                }
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PROCEDURAL SCATTER (seeded RNG)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char sub[96];
            std::snprintf(sub, sizeof(sub), "%d tokens from seed 0x%08X  -  same seed, same field",
                          total, static_cast<unsigned>(seed));
            font.drawText(*renderer, 16.0f, 46.0f, sub, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.44f);

            // Legend + distribution bars.
            float ly = 120.0f;
            for (int i = 0; i < 4; ++i) {
                // Swatch.
                render::SpriteDesc sw2;
                sw2.x = 20.0f;
                sw2.y = ly;
                sw2.width = 22.0f;
                sw2.height = 22.0f;
                sw2.color = kRarityColor[i];
                renderer->drawSprite(disc, sw2);

                char line[64];
                std::snprintf(line, sizeof(line), "%-9s %3d  (%.0f%%)", kRarityName[i], counts[i],
                              100.0 * static_cast<double>(counts[i]) / static_cast<double>(total));
                font.drawText(*renderer, 52.0f, ly + 1.0f, line,
                              render::Color{0.8f, 0.85f, 0.9f, 1}, 0.44f);

                // Bar proportional to count.
                render::SpriteDesc bar;
                bar.x = 52.0f;
                bar.y = ly + 26.0f;
                bar.width = 3.0f + 250.0f * static_cast<float>(counts[i]) / static_cast<float>(total);
                bar.height = 8.0f;
                bar.color = kRarityColor[i];
                renderer->drawSprite(white, bar);

                ly += 52.0f;
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SCATTER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
