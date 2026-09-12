// Maz Engine — "MANYBODY" (integrated spatial-hash broadphase, toward Godot's many-body scaling)
// The warm solver found contact pairs with an O(n^2) all-pairs scan — fine for a few bodies, quadratic
// for a crowd. This adds an opt-in uniform spatial-hash broadphase (PhysicsWorld2D::broadphase) that
// only tests bodies sharing a grid cell, so a big pile scales. It is provably lossless: it returns the
// same contacts in the same order as the brute-force scan (candidates are sorted by index), so the
// result is bit-identical — just faster. This demo drops ~140 circles and boxes into a bin and settles
// them once up front with a fixed timestep, so the render is deterministic + golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

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

void drawCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col, float angle) {
    const int N = 18;
    std::array<render::Point2, 18> pts{};
    for (int i = 0; i < N; ++i) {
        const float t = angle + 6.2831853f * static_cast<float>(i) / static_cast<float>(N);
        pts[static_cast<std::size_t>(i)] = render::Point2{c.x + std::cos(t) * radius,
                                                          c.y + std::sin(t) * radius};
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(N), col);
}

render::Color palette(std::size_t i) {
    const render::Color p[8] = {{0.95f, 0.45f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                {0.5f, 0.65f, 0.95f, 1},   {0.95f, 0.8f, 0.4f, 1},
                                {0.85f, 0.55f, 0.95f, 1},  {0.45f, 0.9f, 0.9f, 1},
                                {0.95f, 0.65f, 0.4f, 1},   {0.7f, 0.8f, 0.5f, 1}};
    return p[i % 8];
}

game::PhysicsWorld2D settlePile(float cx, float floorY) {
    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 900.0f);
    w.warmStarting = true;
    w.broadphase = true;      // the point of the demo — scale to a crowd
    w.broadphaseCellSize = 44.0f;

    game::Body2D floor;
    floor.shape = game::Body2D::Box;
    floor.half = math::vec2(300.0f, 14.0f);
    floor.pos = math::vec2(cx, floorY);
    floor.invMass = 0.0f;
    floor.friction = 0.7f;
    w.add(floor);
    game::Body2D left;
    left.shape = game::Body2D::Box;
    left.half = math::vec2(14.0f, 260.0f);
    left.pos = math::vec2(cx - 300.0f, floorY - 260.0f);
    left.invMass = 0.0f;
    left.friction = 0.5f;
    w.add(left);
    game::Body2D right = left;
    right.pos = math::vec2(cx + 300.0f, floorY - 260.0f);
    w.add(right);

    // ~140 bodies rained in a grid; a tiny per-column offset breaks symmetry so they pack naturally.
    for (int gy = 0; gy < 10; ++gy) {
        for (int gx = 0; gx < 14; ++gx) {
            game::Body2D b;
            const bool box = ((gx * 3 + gy * 5) % 5) == 0;
            b.shape = box ? game::Body2D::Box : game::Body2D::Circle;
            b.radius = 15.0f;
            b.half = math::vec2(15.0f, 15.0f);
            const float ox = ((gx + gy) % 2 == 0) ? 2.0f : -2.0f;
            b.pos = math::vec2(cx - 270.0f + static_cast<float>(gx) * 40.0f + ox,
                               floorY - 500.0f + static_cast<float>(gy) * 44.0f);
            b.invMass = 1.0f;
            b.friction = 0.5f;
            b.restitution = 0.05f;
            if (box) {
                b.enableRotation();
            }
            w.add(b);
        }
    }
    for (int s = 0; s < 480; ++s) {
        w.step(1.0f / 60.0f, 8);
    }
    return w;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MANYBODY (spatial-hash broadphase) starting");

    const game::PhysicsWorld2D pile = settlePile(640.0f, 620.0f);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Spatial-Hash Broadphase (Many Bodies)";
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
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f,
                          "MAZ ENGINE  -  SPATIAL-HASH BROADPHASE (MANY BODIES)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "140 bodies settled with broadphase ON - lossless: identical contacts to the "
                          "O(n^2) scan, just faster",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            for (std::size_t i = 0; i < pile.bodies.size(); ++i) {
                const game::Body2D& b = pile.bodies[i];
                if (i < 3) { // static bin
                    const auto q = boxQuad(b);
                    renderer->drawConvexPolygon(q.data(), 4, render::Color{0.28f, 0.30f, 0.36f, 1.0f});
                    continue;
                }
                if (b.shape == game::Body2D::Box) {
                    const auto outer = boxQuad(b);
                    renderer->drawConvexPolygon(outer.data(), 4, render::Color{0.08f, 0.09f, 0.12f, 1});
                    const auto inner = boxQuad(b, 2.0f);
                    renderer->drawConvexPolygon(inner.data(), 4, palette(i));
                } else {
                    drawCircle(*renderer, b.pos, b.radius, render::Color{0.08f, 0.09f, 0.12f, 1}, b.angle);
                    drawCircle(*renderer, b.pos, b.radius - 2.0f, palette(i), b.angle);
                }
            }

            font.drawText(*renderer, 16.0f, 678.0f,
                          "broadphase = TRUE   |   only bodies sharing a grid cell are pair-tested",
                          render::Color{0.6f, 0.85f, 0.7f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MANYBODY shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
