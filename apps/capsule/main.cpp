// Maz Engine — "CAPSULE" (capsule collision shape, toward Godot's CapsuleShape2D)
// A 2D capsule is a segment swept by a radius — the standard character/pill collider. This adds it as a
// first-class Body2D shape with contact generation vs circles, boxes, and other capsules, wired through
// the warm solver. This demo drops a mix of capsules (upright + tilted) among a couple of boxes into a
// bin and settles them once up front with a fixed timestep, so the render is deterministic + golden-stable.

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

void drawDisc(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int N = 18;
    std::array<render::Point2, 18> pts{};
    for (int i = 0; i < N; ++i) {
        const float t = 6.2831853f * static_cast<float>(i) / static_cast<float>(N);
        pts[static_cast<std::size_t>(i)] =
            render::Point2{c.x + std::cos(t) * radius, c.y + std::sin(t) * radius};
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(N), col);
}

// Draw a capsule (stadium): the connecting rectangle plus a disc at each segment end.
void drawCapsule(render::Renderer& r, const game::Body2D& b, float pad, render::Color col) {
    const float c = std::cos(b.angle), s = std::sin(b.angle);
    const math::vec2 axis(-s, c);         // local +Y in world
    const math::vec2 perp(c, s);          // local +X in world
    const float rr = b.radius - pad;
    const math::vec2 p0 = b.pos - axis * b.half.y;
    const math::vec2 p1 = b.pos + axis * b.half.y;
    const render::Point2 quad[4] = {{p0.x + perp.x * rr, p0.y + perp.y * rr},
                                    {p1.x + perp.x * rr, p1.y + perp.y * rr},
                                    {p1.x - perp.x * rr, p1.y - perp.y * rr},
                                    {p0.x - perp.x * rr, p0.y - perp.y * rr}};
    r.drawConvexPolygon(quad, 4, col);
    drawDisc(r, p0, rr, col);
    drawDisc(r, p1, rr, col);
}

render::Color palette(std::size_t i) {
    const render::Color p[6] = {{0.95f, 0.45f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                {0.5f, 0.65f, 0.95f, 1},   {0.95f, 0.8f, 0.4f, 1},
                                {0.85f, 0.55f, 0.95f, 1},  {0.45f, 0.9f, 0.9f, 1}};
    return p[i % 6];
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CAPSULE (capsule collision shape) starting");

    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 900.0f);
    w.warmStarting = true;
    w.allowSleep = true;

    game::Body2D floor;
    floor.shape = game::Body2D::Box;
    floor.half = math::vec2(360.0f, 16.0f);
    floor.pos = math::vec2(640.0f, 590.0f);
    floor.invMass = 0.0f;
    floor.friction = 0.9f;
    w.add(floor);
    game::Body2D left;
    left.shape = game::Body2D::Box;
    left.half = math::vec2(16.0f, 200.0f);
    left.pos = math::vec2(300.0f, 390.0f);
    left.invMass = 0.0f;
    left.friction = 0.6f;
    w.add(left);
    game::Body2D right = left;
    right.pos = math::vec2(980.0f, 390.0f);
    w.add(right);
    // A couple of boxes to rest capsules against/on.
    game::Body2D crate;
    crate.shape = game::Body2D::Box;
    crate.half = math::vec2(45.0f, 30.0f);
    crate.pos = math::vec2(560.0f, 520.0f);
    crate.invMass = 1.0f;
    crate.friction = 0.8f;
    crate.enableRotation();
    w.add(crate);

    // Capsules of varying length + initial tilt, dropped in.
    struct Spawn {
        float x, y, halfY, r, angle;
    };
    const Spawn spawns[] = {{500.0f, 120.0f, 34.0f, 20.0f, 0.2f}, {640.0f, 60.0f, 46.0f, 18.0f, 1.4f},
                            {760.0f, 140.0f, 30.0f, 22.0f, -0.3f}, {600.0f, 220.0f, 40.0f, 16.0f, 0.9f},
                            {720.0f, 260.0f, 26.0f, 24.0f, -1.1f}, {820.0f, 200.0f, 38.0f, 18.0f, 0.1f}};
    for (const Spawn& sp : spawns) {
        game::Body2D cap;
        cap.shape = game::Body2D::Capsule;
        cap.half = math::vec2(0.0f, sp.halfY);
        cap.radius = sp.r;
        cap.pos = math::vec2(sp.x, sp.y);
        cap.angle = sp.angle;
        cap.invMass = 1.0f;
        cap.friction = 0.7f;
        cap.restitution = 0.02f;
        cap.enableRotation();
        w.add(cap);
    }
    for (int s = 0; s < 480; ++s) {
        w.step(1.0f / 60.0f, 10);
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Capsule Shapes";
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CAPSULE SHAPES (Godot CapsuleShape2D)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "capsules (segment swept by a radius) settling against boxes and each other",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.4f);

            for (std::size_t i = 0; i < w.bodies.size(); ++i) {
                const game::Body2D& b = w.bodies[i];
                if (b.shape == game::Body2D::Box) {
                    const auto q = boxQuad(b);
                    const render::Color col = (b.invMass == 0.0f)
                                                  ? render::Color{0.28f, 0.30f, 0.36f, 1.0f}
                                                  : render::Color{0.75f, 0.7f, 0.5f, 1.0f};
                    renderer->drawConvexPolygon(q.data(), 4, col);
                } else { // capsule
                    drawCapsule(*renderer, b, 0.0f, render::Color{0.08f, 0.09f, 0.12f, 1});
                    drawCapsule(*renderer, b, 2.5f, palette(i));
                }
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "one code path handles capsule-vs-capsule, capsule-vs-box and capsule-vs-circle",
                          render::Color{0.7f, 0.8f, 0.9f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CAPSULE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
