// Maz Engine — "PHYSICS" (2D rigid-body dynamics demo)
// A box of balls falls under gravity, bounces off the walls and floor, and collides and stacks on
// each other via maz::game::PhysicsWorld2D (impulse resolution + positional correction). Proves the
// engine has real 2D dynamics, not just collision detection. Deterministic under the fixed timestep.
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

// A soft-edged filled white circle, tinted per-ball at draw time.
render::TextureHandle circleTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c;
            const float dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            // Solid inside with a soft 1-texel rim, plus a subtle top-left highlight for volume.
            float a = d >= 1.0f ? 0.0f : (d > 0.9f ? (1.0f - (d - 0.9f) / 0.1f) : 1.0f);
            float shade = 1.0f - 0.35f * d + 0.25f * (-dx - dy) * 0.5f;
            shade = shade < 0.35f ? 0.35f : (shade > 1.0f ? 1.0f : shade);
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(size) +
                              static_cast<size_t>(x)) * 4;
            px[i] = static_cast<uint8_t>(255.0f * shade);
            px[i + 1] = static_cast<uint8_t>(255.0f * shade);
            px[i + 2] = static_cast<uint8_t>(255.0f * shade);
            px[i + 3] = static_cast<uint8_t>(255.0f * a);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PHYSICS (2D dynamics demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2D Physics";
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

    render::TextureHandle ball = circleTex(*renderer, 64);

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);

    game::PhysicsWorld2D world;
    world.gravity = math::vec2(0.0f, 1400.0f); // +y down, px/s^2
    world.bounds = game::Bounds2D{20.0f, 80.0f, sw - 20.0f, sh - 20.0f};
    world.hasBounds = true;

    const render::Color palette[6] = {{0.95f, 0.45f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                      {0.5f, 0.65f, 0.95f, 1},   {0.95f, 0.8f, 0.4f, 1},
                                      {0.8f, 0.5f, 0.9f, 1},     {0.45f, 0.9f, 0.9f, 1}};
    std::vector<render::Color> colors;

    // Deterministic spawn: a staggered grid of varied-size balls near the top with a slight sideways
    // drift, so they cascade, collide, and pile up.
    const int cols = 9, rows = 5;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            game::Body2D bd;
            const float rad = 20.0f + static_cast<float>((c + r * 3) % 4) * 7.0f;
            bd.radius = rad;
            bd.pos = math::vec2(90.0f + static_cast<float>(c) * (sw - 180.0f) / (cols - 1),
                                110.0f + static_cast<float>(r) * 60.0f);
            bd.vel = math::vec2(static_cast<float>((c % 3) - 1) * 40.0f, 0.0f);
            bd.invMass = 1.0f / (rad * rad * 0.01f); // mass ~ area
            bd.restitution = 0.45f;
            world.add(bd);
            colors.push_back(palette[static_cast<size_t>((c + r) % 6)]);
        }
    }

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
            world.step(static_cast<float>(clock.fixedDelta()), 6);
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            for (size_t i = 0; i < world.bodies.size(); ++i) {
                const game::Body2D& b = world.bodies[i];
                render::SpriteDesc s;
                s.x = b.pos.x - b.radius;
                s.y = b.pos.y - b.radius;
                s.width = b.radius * 2.0f;
                s.height = b.radius * 2.0f;
                s.color = colors[i];
                renderer->drawSprite(ball, s);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D RIGID-BODY PHYSICS",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[96];
            std::snprintf(buf, sizeof(buf),
                          "%zu balls  |  gravity + impulse collisions + stacking",
                          world.bodies.size());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.8f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PHYSICS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
