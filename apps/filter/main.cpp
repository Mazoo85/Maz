// Maz Engine — "FILTER" (collision layer/mask filtering, toward Godot collision_layer/collision_mask)
// Overlap tests answer "do these shapes touch?"; layers answer "should they even be considered?". Each
// body lives in some layers and reacts to some mask; the world pair-tests two bodies only if either
// scans the other's layer (Godot semantics). This demo fires TWO identical balls at the same wall from
// the same spot: the TOP ball shares a layer with the wall and slams into it; the BOTTOM ball is
// filtered onto a different layer and sails straight through. Trajectories are recorded from a fixed-step
// sim up front, so the render is deterministic + golden-stable.

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

std::array<render::Point2, 4> boxQuad(const game::Body2D& b) {
    const math::vec2 h = b.half;
    return {render::Point2{b.pos.x - h.x, b.pos.y - h.y}, render::Point2{b.pos.x + h.x, b.pos.y - h.y},
            render::Point2{b.pos.x + h.x, b.pos.y + h.y}, render::Point2{b.pos.x - h.x, b.pos.y + h.y}};
}

void drawCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int N = 20;
    std::array<render::Point2, 20> pts{};
    for (int i = 0; i < N; ++i) {
        const float t = 6.2831853f * static_cast<float>(i) / static_cast<float>(N);
        pts[static_cast<std::size_t>(i)] =
            render::Point2{c.x + std::cos(t) * radius, c.y + std::sin(t) * radius};
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(N), col);
}

// Fire a ball rightward at a wall; return its recorded trajectory. If `filtered`, ball + wall are put on
// non-interacting layers so they ignore each other.
std::vector<math::vec2> shoot(float startY, bool filtered, game::Body2D& wallOut) {
    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 0.0f);
    w.warmStarting = true;

    game::Body2D wall;
    wall.shape = game::Body2D::Box;
    wall.half = math::vec2(14.0f, 150.0f);
    wall.pos = math::vec2(640.0f, startY);
    wall.invMass = 0.0f;
    wall.restitution = 0.1f;
    if (filtered) {
        wall.collisionLayer = game::layerBit(1);
        wall.collisionMask = game::layerBit(1);
    }
    w.add(wall);
    wallOut = wall;

    game::Body2D ball;
    ball.shape = game::Body2D::Circle;
    ball.radius = 18.0f;
    ball.pos = math::vec2(180.0f, startY);
    ball.vel = math::vec2(260.0f, 0.0f);
    ball.invMass = 1.0f;
    ball.restitution = 0.1f;
    if (filtered) {
        ball.collisionLayer = game::layerBit(0);
        ball.collisionMask = game::layerBit(0);
    }
    const uint32_t id = w.add(ball);

    std::vector<math::vec2> traj;
    for (int s = 0; s < 210; ++s) {
        w.step(1.0f / 60.0f, 6);
        if (s % 4 == 0) {
            traj.push_back(w.bodies[id].pos);
        }
    }
    return traj;
}

void drawLane(render::Renderer& r, const std::vector<math::vec2>& traj, const game::Body2D& wall,
              render::Color ballCol, render::Color wallCol) {
    const auto q = boxQuad(wall);
    r.drawConvexPolygon(q.data(), 4, wallCol);
    // Fading trail.
    for (std::size_t i = 0; i < traj.size(); ++i) {
        const float a = 0.12f + 0.5f * static_cast<float>(i) / static_cast<float>(traj.size());
        drawCircle(r, traj[i], 5.0f, render::Color{ballCol.r, ballCol.g, ballCol.b, a});
    }
    if (!traj.empty()) {
        drawCircle(r, traj.back(), 18.0f, render::Color{0.08f, 0.09f, 0.12f, 1});
        drawCircle(r, traj.back(), 15.0f, ballCol);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("FILTER (collision layer/mask) starting");

    game::Body2D wallA, wallB;
    const std::vector<math::vec2> hit = shoot(250.0f, false, wallA);
    const std::vector<math::vec2> pass = shoot(470.0f, true, wallB);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Collision Layers / Masks";
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
                          "MAZ ENGINE  -  COLLISION LAYERS / MASKS",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "same ball, same wall - only the layer/mask differs (Godot "
                          "collision_layer / collision_mask)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            drawLane(*renderer, hit, wallA, render::Color{0.55f, 0.9f, 0.6f, 1},
                     render::Color{0.5f, 0.55f, 0.62f, 1});
            drawLane(*renderer, pass, wallB, render::Color{0.95f, 0.55f, 0.5f, 1},
                     render::Color{0.36f, 0.38f, 0.45f, 1});

            font.drawText(*renderer, 200.0f, 200.0f, "masks match -> ball hits the wall",
                          render::Color{0.6f, 0.9f, 0.65f, 1}, 0.36f);
            font.drawText(*renderer, 200.0f, 560.0f, "filtered apart -> ball passes through",
                          render::Color{0.95f, 0.6f, 0.55f, 1}, 0.36f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("FILTER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
