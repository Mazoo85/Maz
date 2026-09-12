// Maz Engine — "BOXES" (2D box physics + friction demo)
// A mix of dynamic boxes and balls drops onto static platforms and stacks. maz::game::PhysicsWorld2D
// now resolves box-box, circle-box, and circle-circle contacts with a normal impulse plus Coulomb
// friction, so boxes settle squarely on ledges and pile up instead of sliding off. Static bodies
// (invMass 0) are the ledges/ground. Deterministic under the fixed timestep. Run --headless/--frames.

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

render::TextureHandle circleTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.9f ? (1.0f - (d - 0.9f) / 0.1f) : 1.0f);
            float shade = 1.0f - 0.3f * d;
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(size) +
                              static_cast<size_t>(x)) * 4;
            px[i] = px[i + 1] = px[i + 2] = static_cast<uint8_t>(255.0f * shade);
            px[i + 3] = static_cast<uint8_t>(255.0f * a);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BOXES (2D box physics demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Box Physics";
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
    render::TextureHandle ball = circleTex(*renderer, 64);

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);

    game::PhysicsWorld2D world;
    world.gravity = math::vec2(0.0f, 1500.0f);
    world.bounds = game::Bounds2D{20.0f, 70.0f, sw - 20.0f, sh - 20.0f};
    world.hasBounds = true;

    std::vector<render::Color> colors;
    const render::Color palette[6] = {{0.9f, 0.5f, 0.45f, 1}, {0.5f, 0.8f, 0.55f, 1},
                                      {0.5f, 0.65f, 0.9f, 1},  {0.9f, 0.8f, 0.45f, 1},
                                      {0.8f, 0.55f, 0.9f, 1},  {0.5f, 0.85f, 0.85f, 1}};

    // Static ledges (invMass 0): a ground bar plus two staggered shelves.
    auto addStatic = [&](float cx, float cy, float hx, float hy) {
        game::Body2D b;
        b.shape = game::Body2D::Box;
        b.pos = math::vec2(cx, cy);
        b.half = math::vec2(hx, hy);
        b.invMass = 0.0f;
        b.friction = 0.7f;
        world.add(b);
        colors.push_back(render::Color{0.28f, 0.30f, 0.4f, 1});
    };
    addStatic(sw * 0.5f, sh - 40.0f, sw * 0.5f - 20.0f, 14.0f); // ground
    addStatic(sw * 0.32f, sh * 0.62f, 150.0f, 12.0f);          // left shelf
    addStatic(sw * 0.70f, sh * 0.46f, 150.0f, 12.0f);          // right shelf
    const size_t staticCount = world.bodies.size();

    // Dynamic mix: boxes and balls dropped in a staggered grid so they cascade and stack.
    const int cols = 8, rows = 4;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            game::Body2D b;
            const bool isBox = ((c + r) % 2) == 0;
            const float s = 26.0f + static_cast<float>((c + r * 2) % 3) * 8.0f;
            b.pos = math::vec2(120.0f + static_cast<float>(c) * (sw - 240.0f) / (cols - 1),
                               100.0f + static_cast<float>(r) * 46.0f);
            b.vel = math::vec2(static_cast<float>((c % 3) - 1) * 30.0f, 0.0f);
            b.restitution = 0.15f;
            b.friction = 0.6f;
            if (isBox) {
                b.shape = game::Body2D::Box;
                b.half = math::vec2(s, s);
                b.invMass = 1.0f / (s * s * 0.02f);
            } else {
                b.shape = game::Body2D::Circle;
                b.radius = s;
                b.invMass = 1.0f / (s * s * 0.02f);
            }
            world.add(b);
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
            world.step(static_cast<float>(clock.fixedDelta()), 8);
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            for (size_t i = 0; i < world.bodies.size(); ++i) {
                const game::Body2D& b = world.bodies[i];
                if (b.shape == game::Body2D::Box) {
                    render::SpriteDesc s;
                    s.x = b.pos.x - b.half.x;
                    s.y = b.pos.y - b.half.y;
                    s.width = b.half.x * 2.0f;
                    s.height = b.half.y * 2.0f;
                    s.color = colors[i];
                    renderer->drawSprite(white, s);
                } else {
                    render::SpriteDesc s;
                    s.x = b.pos.x - b.radius;
                    s.y = b.pos.y - b.radius;
                    s.width = b.radius * 2.0f;
                    s.height = b.radius * 2.0f;
                    s.color = colors[i];
                    renderer->drawSprite(ball, s);
                }
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D BOX PHYSICS + FRICTION",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%zu boxes+balls on %zu static ledges (impulse + friction)",
                          world.bodies.size() - staticCount, staticCount);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.8f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BOXES shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
