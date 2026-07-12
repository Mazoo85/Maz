// Maz Engine — "GROOVE" (groove / slider joints, toward Godot's GrooveJoint2D)
// Three tilted rails. On each, a box is pinned to the rail by a GROOVE joint (game::Joint2D::Groove):
// it is free to slide ALONG the rail but is held ON its line, so gravity pulls each box down its incline
// (not straight down) until it comes to rest against a stop block at the low end. The scene settles to a
// static equilibrium, so the render is deterministic and golden-stable regardless of capture timing.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void thickLine(render::Renderer& r, math::vec2 a, math::vec2 b, float w, render::Color c) {
    math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y * w * 0.5f, d.x * w * 0.5f);
    const render::Point2 q[4] = {{a.x + n.x, a.y + n.y},
                                 {b.x + n.x, b.y + n.y},
                                 {b.x - n.x, b.y - n.y},
                                 {a.x - n.x, a.y - n.y}};
    r.drawConvexPolygon(q, 4, c);
}

void fillBox(render::Renderer& r, math::vec2 c, math::vec2 half, render::Color col) {
    const render::Point2 q[4] = {{c.x - half.x, c.y - half.y},
                                 {c.x + half.x, c.y - half.y},
                                 {c.x + half.x, c.y + half.y},
                                 {c.x - half.x, c.y + half.y}};
    r.drawConvexPolygon(q, 4, col);
}

struct Rail {
    math::vec2 high, low;
    size_t sliderIdx;
    render::Color col;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GROOVE (slider joints) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Groove / Slider Joints";
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

    const math::vec2 sliderHalf{16.0f, 16.0f};
    const math::vec2 stopHalf{20.0f, 20.0f};

    game::PhysicsWorld2D world;
    world.gravity = math::vec2(0.0f, 700.0f); // +y down

    std::vector<Rail> rails;
    auto addRail = [&](math::vec2 high, math::vec2 low, render::Color col) {
        math::vec2 dir = low - high;
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        dir /= (len > 1e-5f ? len : 1.0f);

        game::Body2D rail; // static groove body at the high end (the groove line passes through it)
        rail.shape = game::Body2D::Box;
        rail.half = math::vec2(2.0f, 2.0f);
        rail.pos = high;
        rail.invMass = 0.0f;
        const uint32_t rid = world.add(rail);

        game::Body2D stop; // static block the slider rests against
        stop.shape = game::Body2D::Box;
        stop.half = stopHalf;
        stop.pos = low;
        stop.invMass = 0.0f;
        world.add(stop);

        game::Body2D slider;
        slider.shape = game::Body2D::Box;
        slider.half = sliderHalf;
        slider.pos = high + dir * 46.0f; // start near the top of the rail
        slider.invMass = 1.0f;
        slider.restitution = 0.0f;
        slider.linearDamping = 1.2f; // settle firmly to a static rest
        const uint32_t sid = world.add(slider);

        game::Joint2D g;
        g.type = game::Joint2D::Groove;
        g.a = static_cast<int>(rid);
        g.b = static_cast<int>(sid);
        g.localA = math::vec2(0.0f, 0.0f); // groove through the rail body
        g.axis = dir;                      // slide direction (rail body angle is 0, so world == local)
        g.anchorB = math::vec2(0.0f, 0.0f); // slider anchored at its centre
        world.addJoint(g);

        rails.push_back(Rail{high, low, sid, col});
    };

    addRail(math::vec2(220.0f, 150.0f), math::vec2(470.0f, 470.0f), render::Color{0.45f, 0.80f, 1.0f, 1});
    addRail(math::vec2(780.0f, 150.0f), math::vec2(560.0f, 460.0f), render::Color{1.0f, 0.6f, 0.45f, 1});
    addRail(math::vec2(880.0f, 250.0f), math::vec2(1150.0f, 470.0f), render::Color{0.6f, 1.0f, 0.55f, 1});

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            world.step(1.0f / 60.0f, 8);
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            for (const Rail& rl : rails) {
                // The rail line + stop block.
                thickLine(*renderer, rl.high, rl.low, 3.0f, render::Color{0.34f, 0.37f, 0.44f, 1});
                fillBox(*renderer, rl.low, stopHalf, render::Color{0.16f, 0.17f, 0.21f, 1});
                // High-end anchor marker.
                fillBox(*renderer, rl.high, math::vec2(4.0f, 4.0f), render::Color{0.8f, 0.82f, 0.9f, 1});
                // The slider box at its current (settled) position.
                fillBox(*renderer, world.bodies[rl.sliderIdx].pos, sliderHalf, rl.col);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  GROOVE / SLIDER JOINTS",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 52.0f,
                          "each box is pinned to a rail: free to slide along it, held on its line — "
                          "gravity slides it to the stop",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GROOVE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
