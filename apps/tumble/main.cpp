// Maz Engine — "TUMBLE" (2D rigid-body ROTATION, toward Godot's RigidBody2D angular dynamics)
// The 2D physics gained real angular dynamics: oriented boxes carry an orientation + spin and a
// finite moment of inertia, and contact impulses applied at the actual contact point produce torque
// (game::Body2D::enableRotation + the oriented solver in game::PhysicsWorld2D). This demo drops a
// stack of rectangles at assorted tilts into a bin; they fall, strike the floor/walls/each other on
// their corners, tumble, and settle into a leaning pile — none of which the old translation-only
// bodies could do (they could only slide axis-aligned). Deterministic under the fixed timestep, so
// once the pile comes to rest the render is golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

// World-space corners of an oriented box, optionally inset (shrunk) by `pad` on each side.
std::array<render::Point2, 4> boxQuad(const game::Body2D& b, float pad = 0.0f) {
    const float c = std::cos(b.angle), s = std::sin(b.angle);
    const float hx = b.half.x - pad, hy = b.half.y - pad;
    const math::vec2 ax(c, s), ay(-s, c);
    auto corner = [&](float sx, float sy) {
        const math::vec2 p = b.pos + ax * (sx * hx) + ay * (sy * hy);
        return render::Point2{p.x, p.y};
    };
    return {corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1)};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TUMBLE (2D rigid-body rotation) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2D Rotation";
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

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);

    game::PhysicsWorld2D world;
    world.gravity = math::vec2(0.0f, 1200.0f); // +y down

    // Static bin: floor + two side walls (invMass 0). Rotation stays locked on these.
    auto addStatic = [&](float cx, float cy, float hx, float hy) {
        game::Body2D b;
        b.shape = game::Body2D::Box;
        b.pos = math::vec2(cx, cy);
        b.half = math::vec2(hx, hy);
        b.invMass = 0.0f;
        b.friction = 0.8f;
        world.add(b);
    };
    const float floorTop = sh - 60.0f;
    addStatic(sw * 0.5f, floorTop + 20.0f, sw * 0.42f, 20.0f); // floor
    addStatic(sw * 0.5f - sw * 0.42f, sh * 0.55f, 18.0f, sh * 0.4f); // left wall
    addStatic(sw * 0.5f + sw * 0.42f, sh * 0.55f, 18.0f, sh * 0.4f); // right wall
    const size_t firstDynamic = world.bodies.size();

    const render::Color palette[6] = {{0.95f, 0.45f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                      {0.5f, 0.65f, 0.95f, 1},   {0.95f, 0.8f, 0.4f, 1},
                                      {0.85f, 0.55f, 0.95f, 1},  {0.45f, 0.9f, 0.9f, 1}};
    std::vector<render::Color> colors;

    // Deterministic spawn: rectangles of varied size + tilt, staggered above the bin, dropped with a
    // slight sideways drift so they cascade into a heap and settle at leaning angles.
    const int count = 11;
    for (int i = 0; i < count; ++i) {
        game::Body2D b;
        b.shape = game::Body2D::Box;
        const float w = 34.0f + static_cast<float>((i * 7) % 5) * 9.0f;
        const float h = 22.0f + static_cast<float>((i * 3) % 4) * 8.0f;
        b.half = math::vec2(w, h);
        b.pos = math::vec2(sw * 0.32f + static_cast<float>((i * 5) % 7) * (sw * 0.36f / 6.0f),
                           120.0f + static_cast<float>(i) * 34.0f);
        b.vel = math::vec2(static_cast<float>((i % 3) - 1) * 45.0f, 0.0f);
        b.angle = -1.0f + static_cast<float>(i) * 0.35f; // varied initial tilt
        b.invMass = 1.0f / (w * h * 0.01f);              // mass ~ area
        b.restitution = 0.05f;
        b.friction = 0.6f;
        b.linearDamping = 0.5f;
        b.angularDamping = 1.6f;
        b.enableRotation();
        world.add(b);
        colors.push_back(palette[static_cast<size_t>(i % 6)]);
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
            world.step(static_cast<float>(clock.fixedDelta()), 10);
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Static bin.
            for (size_t i = 0; i < firstDynamic; ++i) {
                const auto q = boxQuad(world.bodies[i]);
                renderer->drawConvexPolygon(q.data(), 4, render::Color{0.16f, 0.17f, 0.21f, 1.0f});
            }
            // Tumbled boxes: a filled quad + a lighter inset panel so the orientation reads clearly.
            for (size_t i = firstDynamic; i < world.bodies.size(); ++i) {
                const game::Body2D& b = world.bodies[i];
                const render::Color col = colors[i - firstDynamic];
                const auto outer = boxQuad(b);
                renderer->drawConvexPolygon(outer.data(), 4, col);
                const auto inner = boxQuad(b, 6.0f);
                renderer->drawConvexPolygon(inner.data(), 4,
                                            render::Color{col.r * 0.6f, col.g * 0.6f, col.b * 0.6f, 1.0f});
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D RIGID-BODY ROTATION",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "oriented boxes tumble on corner contacts + settle (torque + angular impulses)",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TUMBLE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
