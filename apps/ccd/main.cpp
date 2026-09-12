// Maz Engine — "CCD" (continuous collision detection, toward Godot's continuous_cd)
// A discrete solver only checks for overlap at the START and END of a step, so a body moving faster than
// its own size leaps clean over a thin wall in one frame and never notices — it tunnels. This adds
// per-body continuous collision (Body2D::continuous): the body is swept along its motion each step and
// stops at the first surface. This demo fires TWO identical fast bullets at a thin wall: the TOP one is
// discrete and passes straight through; the BOTTOM one is continuous and stops dead at the wall.
// Trajectories are recorded from a fixed-step sim up front -> deterministic + golden-stable.

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

void drawDisc(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int N = 20;
    std::array<render::Point2, 20> pts{};
    for (int i = 0; i < N; ++i) {
        const float t = 6.2831853f * static_cast<float>(i) / static_cast<float>(N);
        pts[static_cast<std::size_t>(i)] =
            render::Point2{c.x + std::cos(t) * radius, c.y + std::sin(t) * radius};
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(N), col);
}

std::vector<math::vec2> shoot(float y, bool ccd) {
    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 0.0f);
    w.warmStarting = true;
    game::Body2D wall;
    wall.shape = game::Body2D::Box;
    wall.half = math::vec2(5.0f, 90.0f); // thin wall at x=500
    wall.pos = math::vec2(500.0f, y);
    wall.invMass = 0.0f;
    wall.restitution = 0.0f;
    w.add(wall);
    game::Body2D bullet;
    bullet.shape = game::Body2D::Circle;
    bullet.radius = 12.0f;
    bullet.pos = math::vec2(150.0f, y);
    // 150px/step: step landings 150,300,450,600,... straddle the wall (450 before, 600 past) so the
    // discrete bullet never overlaps at a step boundary and tunnels cleanly.
    bullet.vel = math::vec2(9000.0f, 0.0f);
    bullet.invMass = 1.0f;
    bullet.restitution = 0.0f;
    bullet.continuous = ccd;
    w.add(bullet);

    std::vector<math::vec2> traj;
    for (int s = 0; s < 8; ++s) {
        traj.push_back(w.bodies[1].pos);
        w.step(1.0f / 60.0f, 6);
        if (w.bodies[1].pos.x > 1240.0f) {
            break;
        }
    }
    traj.push_back(w.bodies[1].pos);
    return traj;
}

void drawLane(render::Renderer& r, const std::vector<math::vec2>& traj, float wallY,
              render::Color ballCol) {
    // Wall (at x=500, half-width 5).
    const render::Point2 wq[4] = {{495.0f, wallY - 90.0f}, {505.0f, wallY - 90.0f},
                                  {505.0f, wallY + 90.0f}, {495.0f, wallY + 90.0f}};
    r.drawConvexPolygon(wq, 4, render::Color{0.5f, 0.55f, 0.62f, 1});
    // Fading trail.
    for (std::size_t i = 0; i < traj.size(); ++i) {
        const float a = 0.12f + 0.55f * static_cast<float>(i) / static_cast<float>(traj.size());
        drawDisc(r, traj[i], 5.0f, render::Color{ballCol.r, ballCol.g, ballCol.b, a});
    }
    if (!traj.empty()) {
        drawDisc(r, traj.back(), 12.0f, render::Color{0.08f, 0.09f, 0.12f, 1});
        drawDisc(r, traj.back(), 9.0f, ballCol);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CCD (continuous collision) starting");

    const std::vector<math::vec2> tunnel = shoot(250.0f, false);
    const std::vector<math::vec2> stop = shoot(460.0f, true);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Continuous Collision";
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
                          "MAZ ENGINE  -  CONTINUOUS COLLISION (Godot continuous_cd)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "same fast bullet, same thin wall - only continuous collision differs",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.4f);

            drawLane(*renderer, tunnel, 250.0f, render::Color{0.95f, 0.55f, 0.5f, 1});
            drawLane(*renderer, stop, 460.0f, render::Color{0.55f, 0.9f, 0.6f, 1});

            font.drawText(*renderer, 150.0f, 190.0f, "continuous = FALSE  ->  tunnels through the wall",
                          render::Color{0.95f, 0.6f, 0.55f, 1}, 0.36f);
            font.drawText(*renderer, 150.0f, 560.0f, "continuous = TRUE   ->  stops at the wall",
                          render::Color{0.6f, 0.9f, 0.65f, 1}, 0.36f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CCD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
