// Maz Engine — "MOTOR" (pin-joint angular motor + limit, toward Godot PinJoint2D motor/angular_limit)
// A hinge isn't just a pivot — it can be DRIVEN (a motor spinning to a target speed) and BOUNDED (an
// angular limit, like a door stop or a robot elbow). This adds both to Joint2D. The demo shows two arms
// pinned to hubs: the LEFT is a free motor spun continuously (its ghost trail sweeps a full circle); the
// RIGHT has an angular limit, so the motor drives it up to the stop and it parks there (ghosts pile up in
// the allowed wedge). Angles are recorded from a fixed-step sim up front -> deterministic + golden-stable.

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

// Draw an arm (a box of half-length L along its local +x) pivoted at `hub`, rotated by `angle`.
void drawArm(render::Renderer& r, math::vec2 hub, float angle, float L, float w, render::Color col) {
    const float c = std::cos(angle), s = std::sin(angle);
    const math::vec2 ax(c, s), ay(-s, c);
    auto corner = [&](float sx, float sy) {
        const math::vec2 p = hub + ax * (sx * L) + ay * (sy * w);
        return render::Point2{p.x, p.y};
    };
    const render::Point2 q[4] = {corner(0.0f, -1.0f), corner(1.0f, -1.0f), corner(1.0f, 1.0f),
                                 corner(0.0f, 1.0f)};
    r.drawConvexPolygon(q, 4, col);
}

// Run a hinge (arm pinned to a static hub at `hub`) and record the arm's angle each step.
std::vector<float> spinArm(math::vec2 hub, bool limit) {
    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 0.0f);
    w.warmStarting = true;
    game::Body2D anchor;
    anchor.shape = game::Body2D::Circle;
    anchor.radius = 4.0f;
    anchor.pos = hub;
    anchor.invMass = 0.0f;
    anchor.collisionLayer = game::layerBit(0);
    anchor.collisionMask = game::layerBit(0);
    w.add(anchor);
    game::Body2D arm;
    arm.shape = game::Body2D::Box;
    arm.half = math::vec2(70.0f, 8.0f);
    arm.pos = hub + math::vec2(70.0f, 0.0f); // arm extends to the right of the hub
    arm.invMass = 1.0f;
    arm.collisionLayer = game::layerBit(1);
    arm.collisionMask = game::layerBit(1);
    arm.enableRotation();
    w.add(arm);
    game::Joint2D j;
    j.type = game::Joint2D::Pin;
    j.a = 0;
    j.b = 1;
    j.localA = math::vec2(0.0f, 0.0f);   // hub centre
    j.anchorB = math::vec2(-70.0f, 0.0f); // the arm's inner end sits on the hub
    j.motorEnabled = true;
    j.motorSpeed = 3.0f;
    j.maxMotorTorque = 5e6f;
    if (limit) {
        j.limitEnabled = true;
        j.lowerAngle = -0.3f;
        j.upperAngle = 1.3f;
    }
    w.addJoint(j);

    std::vector<float> angles;
    for (int s = 0; s < 90; ++s) {
        angles.push_back(w.bodies[1].angle);
        w.step(1.0f / 60.0f, 8);
    }
    angles.push_back(w.bodies[1].angle);
    return angles;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MOTOR (pin-joint motor + limit) starting");

    const math::vec2 hubL(360.0f, 360.0f), hubR(940.0f, 360.0f);
    const std::vector<float> freeAngles = spinArm(hubL, false);
    const std::vector<float> limitAngles = spinArm(hubR, true);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Hinge Motor + Limit";
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

    auto drawHinge = [&](math::vec2 hub, const std::vector<float>& angles, render::Color col) {
        // Ghost trail: faint arms at past angles.
        for (std::size_t i = 0; i < angles.size(); i += 3) {
            const float a = 0.05f + 0.25f * static_cast<float>(i) / static_cast<float>(angles.size());
            drawArm(*renderer, hub, angles[i], 140.0f, 8.0f, render::Color{col.r, col.g, col.b, a});
        }
        // Final arm solid.
        drawArm(*renderer, hub, angles.back(), 140.0f, 8.0f, render::Color{0.08f, 0.09f, 0.12f, 1});
        drawArm(*renderer, hub, angles.back(), 140.0f, 5.0f, col);
        drawDisc(*renderer, hub, 12.0f, render::Color{0.7f, 0.72f, 0.8f, 1});
        drawDisc(*renderer, hub, 8.0f, render::Color{0.3f, 0.32f, 0.4f, 1});
    };

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
                          "MAZ ENGINE  -  HINGE MOTOR + LIMIT (Godot PinJoint2D)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "a pin joint can be driven by a motor and bounded by an angular limit",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.4f);

            drawHinge(hubL, freeAngles, render::Color{0.55f, 0.85f, 1.0f, 1});
            drawHinge(hubR, limitAngles, render::Color{0.95f, 0.75f, 0.45f, 1});

            font.drawText(*renderer, 250.0f, 560.0f, "motor only  ->  free spin",
                          render::Color{0.6f, 0.85f, 1.0f, 1}, 0.4f);
            font.drawText(*renderer, 820.0f, 560.0f, "motor + limit  ->  stops at the wedge",
                          render::Color{0.95f, 0.78f, 0.5f, 1}, 0.4f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MOTOR shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
