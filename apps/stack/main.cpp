// Maz Engine — "STACK" (2-point contact manifolds for stable box stacks, toward Godot/Box2D contacts)
// The oriented rigid-body solver used a SINGLE contact point per box pair — enough to stop overlap, but
// with no torque balance a stacked box slowly rotates off and the tower topples. This adds proper
// TWO-point clipped manifolds (PhysicsWorld2D::solveManifolds). This demo proves it: TWO identical
// towers of oriented boxes are dropped and settled — the LEFT with two-point manifolds ON stays a clean
// square tower, the RIGHT with the old single-point solver shears and topples. Both are simulated once
// at startup with a fixed timestep and drawn statically, so the render is deterministic and golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
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

// Build a tower of `n` oriented boxes on a static floor centred at `cx`, settle it, and return the world.
game::PhysicsWorld2D settleTower(float cx, float floorTop, int n, bool manifolds) {
    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 900.0f); // +y down
    w.solveManifolds = manifolds;

    game::Body2D floor;
    floor.shape = game::Body2D::Box;
    floor.pos = math::vec2(cx, floorTop + 16.0f);
    floor.half = math::vec2(180.0f, 16.0f);
    floor.invMass = 0.0f; // static
    floor.friction = 0.9f;
    w.add(floor);

    const float bh = 22.0f; // half-height
    for (int i = 0; i < n; ++i) {
        game::Body2D box;
        box.shape = game::Body2D::Box;
        box.half = math::vec2(52.0f, bh);
        // A small alternating offset gives the single-point solver an asymmetry to amplify.
        const float dx = (i % 2 == 0) ? 4.0f : -4.0f;
        box.pos = math::vec2(cx + dx, floorTop - bh - static_cast<float>(i) * (bh * 2.0f + 2.0f));
        box.invMass = 1.0f;
        box.friction = 0.9f;
        box.restitution = 0.0f;
        box.enableRotation();
        w.add(box);
    }
    for (int s = 0; s < 420; ++s) {
        w.step(1.0f / 60.0f, 12);
    }
    return w;
}

void drawTower(render::Renderer& r, const game::PhysicsWorld2D& w) {
    const render::Color palette[6] = {{0.95f, 0.45f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                      {0.5f, 0.65f, 0.95f, 1},   {0.95f, 0.8f, 0.4f, 1},
                                      {0.85f, 0.55f, 0.95f, 1},  {0.45f, 0.9f, 0.9f, 1}};
    for (std::size_t i = 0; i < w.bodies.size(); ++i) {
        const game::Body2D& b = w.bodies[i];
        if (i == 0) { // the static floor
            const auto q = boxQuad(b);
            r.drawConvexPolygon(q.data(), 4, render::Color{0.28f, 0.30f, 0.36f, 1.0f});
            continue;
        }
        const auto outer = boxQuad(b);
        r.drawConvexPolygon(outer.data(), 4, render::Color{0.08f, 0.09f, 0.12f, 1.0f}); // outline
        const auto inner = boxQuad(b, 2.5f);
        r.drawConvexPolygon(inner.data(), 4, palette[(i - 1) % 6]);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("STACK (2-point contact manifolds) starting");

    const float floorTop = 560.0f;
    const int n = 5;
    // Simulate both towers once, up front, so the frame is static and deterministic.
    const game::PhysicsWorld2D stable = settleTower(340.0f, floorTop, n, true);
    const game::PhysicsWorld2D toppled = settleTower(940.0f, floorTop, n, false);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Stable Box Stacks (2-point manifolds)";
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
                          "MAZ ENGINE  -  STABLE BOX STACKS (2-POINT CONTACT MANIFOLDS)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "same five-box tower, dropped and settled - the only difference is the contact "
                          "solver",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            drawTower(*renderer, stable);
            drawTower(*renderer, toppled);

            font.drawText(*renderer, 190.0f, 610.0f, "solveManifolds = TRUE",
                          render::Color{0.55f, 0.9f, 0.6f, 1}, 0.46f);
            font.drawText(*renderer, 200.0f, 642.0f, "two contact points per face -> the tower stays square",
                          render::Color{0.7f, 0.85f, 0.72f, 1}, 0.34f);
            font.drawText(*renderer, 800.0f, 610.0f, "solveManifolds = FALSE",
                          render::Color{0.95f, 0.55f, 0.5f, 1}, 0.46f);
            font.drawText(*renderer, 790.0f, 642.0f, "one contact point -> no torque balance -> it topples",
                          render::Color{0.9f, 0.72f, 0.68f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("STACK shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
