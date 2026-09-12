// Maz Engine — "JOINTS" (2D physics constraints, toward Godot's PinJoint2D / DampedSpringJoint2D)
// The rigid-body world gained joints: a Pin forces two anchor points together (a hinge / rope link),
// a Spring pulls two anchors toward a rest length with stiffness + damping. This demo builds a rope
// bridge — a chain of boxes pinned end-to-end between two posts that sags into a catenary — and a row
// of masses hung from springs of increasing stiffness (so each settles at a different stretch). Both
// come to rest, and the sim is deterministic under the fixed timestep, so the settled frame is
// golden-stable. Run --headless / --frames N for CI.

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

std::array<render::Point2, 4> boxQuad(const game::Body2D& b) {
    const float c = std::cos(b.angle), s = std::sin(b.angle);
    const math::vec2 ax(c, s), ay(-s, c);
    auto corner = [&](float sx, float sy) {
        const math::vec2 p = b.pos + ax * (sx * b.half.x) + ay * (sy * b.half.y);
        return render::Point2{p.x, p.y};
    };
    return {corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1)};
}

void drawLine(render::Renderer& r, math::vec2 a, math::vec2 b, float thick, render::Color col) {
    math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y * thick * 0.5f, d.x * thick * 0.5f);
    const render::Point2 quad[4] = {{a.x + n.x, a.y + n.y},
                                    {b.x + n.x, b.y + n.y},
                                    {b.x - n.x, b.y - n.y},
                                    {a.x - n.x, a.y - n.y}};
    r.drawConvexPolygon(quad, 4, col);
}

void drawDot(render::Renderer& r, math::vec2 p, float rad, render::Color col) {
    const render::Point2 q[4] = {
        {p.x - rad, p.y - rad}, {p.x + rad, p.y - rad}, {p.x + rad, p.y + rad}, {p.x - rad, p.y + rad}};
    r.drawConvexPolygon(q, 4, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("JOINTS (2D physics constraints) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2D Joints";
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

    const float sw = static_cast<float>(cfg.width);

    game::PhysicsWorld2D world;
    world.gravity = math::vec2(0.0f, 700.0f);

    // --- Rope bridge: a chain of boxes pinned end-to-end between two fixed posts. -------------------
    const math::vec2 postA(sw * 0.10f, 190.0f);
    const math::vec2 postB(sw * 0.52f, 190.0f);
    const int links = 11;
    const float hx = 24.0f, hy = 7.0f;
    const float step = (postB.x - postA.x) / static_cast<float>(links);
    std::vector<uint32_t> chain;
    for (int i = 0; i < links; ++i) {
        game::Body2D b;
        b.shape = game::Body2D::Box;
        b.half = math::vec2(hx, hy);
        b.pos = math::vec2(postA.x + step * (static_cast<float>(i) + 0.5f), postA.y);
        b.invMass = 1.0f / 6.0f;
        b.friction = 0.4f;
        b.linearDamping = 0.15f;
        b.angularDamping = 0.15f;
        b.enableRotation();
        chain.push_back(world.add(b));
    }
    // Pin the left end to post A, each link to the next, and the right end to post B.
    {
        game::Joint2D j;
        j.type = game::Joint2D::Pin;
        j.a = static_cast<int>(chain.front());
        j.b = -1;
        j.localA = math::vec2(-hx, 0.0f);
        j.anchorB = postA;
        world.addJoint(j);
    }
    for (int i = 0; i + 1 < links; ++i) {
        game::Joint2D j;
        j.type = game::Joint2D::Pin;
        j.a = static_cast<int>(chain[static_cast<size_t>(i)]);
        j.b = static_cast<int>(chain[static_cast<size_t>(i + 1)]);
        j.localA = math::vec2(hx, 0.0f);
        j.anchorB = math::vec2(-hx, 0.0f);
        world.addJoint(j);
    }
    {
        game::Joint2D j;
        j.type = game::Joint2D::Pin;
        j.a = static_cast<int>(chain.back());
        j.b = -1;
        j.localA = math::vec2(hx, 0.0f);
        j.anchorB = postB;
        world.addJoint(j);
    }

    // --- Spring rack: masses hung from anchors by springs of increasing stiffness. ------------------
    const int springs = 4;
    std::vector<uint32_t> masses;
    std::vector<math::vec2> springAnchors;
    for (int i = 0; i < springs; ++i) {
        const float ax = sw * 0.64f + static_cast<float>(i) * (sw * 0.30f / (springs - 1));
        const math::vec2 anchor(ax, 150.0f);
        springAnchors.push_back(anchor);

        game::Body2D m;
        m.shape = game::Body2D::Circle;
        m.radius = 16.0f;
        m.pos = math::vec2(ax, anchor.y + 70.0f);
        m.invMass = 1.0f / 3.0f;
        const uint32_t id = world.add(m);
        masses.push_back(id);

        game::Joint2D j;
        j.type = game::Joint2D::Spring;
        j.a = static_cast<int>(id);
        j.b = -1;
        j.localA = math::vec2(0.0f, 0.0f);
        j.anchorB = anchor;
        j.restLength = 60.0f;
        j.stiffness = 12.0f + static_cast<float>(i) * 10.0f; // stiffer -> less sag
        j.damping = 3.0f;
        world.addJoint(j);
    }

    const render::Color chainCol{0.85f, 0.7f, 0.45f, 1.0f};
    const render::Color springPalette[4] = {{0.95f, 0.5f, 0.5f, 1},
                                             {0.6f, 0.85f, 0.55f, 1},
                                             {0.55f, 0.7f, 0.95f, 1},
                                             {0.85f, 0.6f, 0.95f, 1}};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            world.step(static_cast<float>(clock.fixedDelta()), 12);
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Bridge posts + link boxes.
            drawDot(*renderer, postA, 9.0f, render::Color{0.7f, 0.72f, 0.8f, 1});
            drawDot(*renderer, postB, 9.0f, render::Color{0.7f, 0.72f, 0.8f, 1});
            for (uint32_t id : chain) {
                const auto q = boxQuad(world.bodies[id]);
                renderer->drawConvexPolygon(q.data(), 4, chainCol);
            }

            // Springs: a line from each anchor to its mass, then the mass.
            for (int i = 0; i < springs; ++i) {
                const game::Body2D& m = world.bodies[masses[static_cast<size_t>(i)]];
                drawDot(*renderer, springAnchors[static_cast<size_t>(i)], 7.0f,
                        render::Color{0.7f, 0.72f, 0.8f, 1});
                drawLine(*renderer, springAnchors[static_cast<size_t>(i)], m.pos, 4.0f,
                         render::Color{0.5f, 0.53f, 0.6f, 1});
                const int sides = 20;
                std::array<render::Point2, 20> circ;
                for (int k = 0; k < sides; ++k) {
                    const float a = 6.2831853f * static_cast<float>(k) / static_cast<float>(sides);
                    circ[static_cast<size_t>(k)] =
                        render::Point2{m.pos.x + std::cos(a) * m.radius, m.pos.y + std::sin(a) * m.radius};
                }
                renderer->drawConvexPolygon(circ.data(), static_cast<uint32_t>(sides),
                                            springPalette[static_cast<size_t>(i)]);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D PHYSICS JOINTS",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "pin-jointed rope bridge (catenary) + spring-hung masses",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("JOINTS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
