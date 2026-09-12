// Maz Engine — "SWARM" (ECS demo)
// Hundreds of entities, each with Transform / Velocity / Visual components, moved and bounced by
// tiny systems iterating maz::ecs views, then drawn as glowing sprites. Shows the engine scales
// to many objects with simple, data-oriented code. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>
#include <algorithm>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr int kEntityCount = 800;

// --- Components ---
struct Transform {
    float x, y;
};
struct Velocity {
    float vx, vy;
};
struct Visual {
    float size;
    render::Color color;
};

std::vector<uint8_t> makeGlow(uint32_t s) {
    std::vector<uint8_t> px(static_cast<size_t>(s) * s * 4, 0);
    const float c = (static_cast<float>(s) - 1.0f) * 0.5f;
    for (uint32_t y = 0; y < s; ++y) {
        for (uint32_t x = 0; x < s; ++x) {
            const float dx = static_cast<float>(x) - c;
            const float dy = static_cast<float>(y) - c;
            const float d = std::sqrt(dx * dx + dy * dy) / c;
            const float a = std::max(0.0f, 1.0f - d);
            const size_t i = (static_cast<size_t>(y) * s + x) * 4;
            px[i + 0] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * a * 255.0f);
        }
    }
    return px;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SWARM (ECS demo) starting — %d entities", kEntityCount);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — ECS Swarm";
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

    render::TextureHandle glowTex = renderer->createTexture(32, 32, makeGlow(32).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    // Spawn the swarm.
    std::mt19937 rng(7u);
    auto frand = [&](float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    };
    ecs::World world;
    for (int i = 0; i < kEntityCount; ++i) {
        const ecs::Entity e = world.create();
        world.add<Transform>(e, {frand(0.0f, static_cast<float>(cfg.width)),
                                 frand(0.0f, static_cast<float>(cfg.height))});
        world.add<Velocity>(e, {frand(-90.0f, 90.0f), frand(-90.0f, 90.0f)});
        Visual vis;
        vis.size = frand(6.0f, 20.0f);
        vis.color = render::Color{frand(0.3f, 1.0f), frand(0.3f, 1.0f), frand(0.5f, 1.0f), 0.9f};
        world.add<Visual>(e, vis);
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float w = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
        const float h = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            // System: integrate motion and bounce off the screen edges.
            world.view<Transform, Velocity>([&](ecs::Entity, Transform& t, Velocity& v) {
                t.x += v.vx * dt;
                t.y += v.vy * dt;
                if (t.x < 0.0f) { t.x = 0.0f; v.vx = -v.vx; }
                if (t.y < 0.0f) { t.y = 0.0f; v.vy = -v.vy; }
                if (t.x > w) { t.x = w; v.vx = -v.vx; }
                if (t.y > h) { t.y = h; v.vy = -v.vy; }
            });
        }

        renderer->setClearColor(render::Color{0.04f, 0.05f, 0.08f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);

            // System: render every entity that has a Transform and a Visual.
            world.view<Transform, Visual>([&](ecs::Entity, Transform& t, Visual& vis) {
                render::SpriteDesc s;
                s.x = t.x - vis.size * 0.5f;
                s.y = t.y - vis.size * 0.5f;
                s.width = vis.size;
                s.height = vis.size;
                s.color = vis.color;
                renderer->drawSprite(glowTex, s);
            });

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ECS SWARM",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[48];
            std::snprintf(buf, sizeof(buf), "ENTITIES  %zu", world.size());
            font.drawText(*renderer, 16.0f, 44.0f, buf, render::Color{0.75f, 0.8f, 0.9f, 1}, 0.55f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SWARM shutting down (%zu entities, renderer %s)", world.size(),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
