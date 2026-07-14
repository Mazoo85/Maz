// Maz Engine — "CONVEX" (arbitrary convex polygon dynamic bodies, toward Godot ConvexPolygonShape2D)
// Until now only circles, boxes, and capsules could be dynamic bodies. This adds arbitrary convex hulls
// as a first-class Body2D shape, with SAT + reference/incident-face clipping so they stack as stably as
// boxes (two-point manifolds), plus convex-vs-circle/box/capsule/boundary contacts. This demo drops a
// jumble of triangles, pentagons and hexagons into a bin and settles them once up front with a fixed
// timestep -> deterministic + golden-stable.

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

// World vertices of a convex body (local hull rotated by angle + pos).
std::vector<render::Point2> convexWorld(const game::Body2D& b, float shrink) {
    const float c = std::cos(b.angle), s = std::sin(b.angle);
    math::vec2 ctr(0, 0);
    for (const math::vec2& v : b.verts) {
        ctr += v;
    }
    ctr /= static_cast<float>(b.verts.size());
    std::vector<render::Point2> out;
    out.reserve(b.verts.size());
    for (math::vec2 v : b.verts) {
        v = ctr + (v - ctr) * (1.0f - shrink); // shrink toward centroid for an inset fill
        out.push_back(render::Point2{b.pos.x + v.x * c - v.y * s, b.pos.y + v.x * s + v.y * c});
    }
    return out;
}

render::Color palette(std::size_t i) {
    const render::Color p[6] = {{0.95f, 0.45f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                {0.5f, 0.65f, 0.95f, 1},   {0.95f, 0.8f, 0.4f, 1},
                                {0.85f, 0.55f, 0.95f, 1},  {0.45f, 0.9f, 0.9f, 1}};
    return p[i % 6];
}

game::Body2D makePoly(float x, float y, float r, int sides, float rot) {
    game::Body2D b;
    b.shape = game::Body2D::Convex;
    b.pos = math::vec2(x, y);
    const float step = 6.2831853f / static_cast<float>(sides);
    for (int i = 0; i < sides; ++i) {
        const float a = rot + step * static_cast<float>(i);
        b.verts.push_back(math::vec2(std::cos(a) * r, std::sin(a) * r));
    }
    b.invMass = 1.0f;
    b.friction = 0.6f;
    b.restitution = 0.05f;
    b.enableRotation();
    return b;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CONVEX (convex polygon bodies) starting");

    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 900.0f);
    w.warmStarting = true;
    w.broadphase = true;
    w.allowSleep = true;

    game::Body2D floor;
    floor.shape = game::Body2D::Box;
    floor.half = math::vec2(300.0f, 16.0f);
    floor.pos = math::vec2(640.0f, 600.0f);
    floor.invMass = 0.0f;
    floor.friction = 0.8f;
    w.add(floor);
    game::Body2D left;
    left.shape = game::Body2D::Box;
    left.half = math::vec2(16.0f, 240.0f);
    left.pos = math::vec2(340.0f, 360.0f);
    left.invMass = 0.0f;
    left.friction = 0.5f;
    w.add(left);
    game::Body2D right = left;
    right.pos = math::vec2(940.0f, 360.0f);
    w.add(right);

    // A jumble of triangles, pentagons and hexagons.
    struct Spawn {
        float x, y, r, rot;
        int sides;
    };
    const Spawn spawns[] = {
        {520.0f, 120.0f, 34.0f, 0.2f, 3},  {620.0f, 60.0f, 30.0f, 0.8f, 5},
        {720.0f, 130.0f, 32.0f, 0.0f, 6},  {560.0f, 220.0f, 28.0f, 1.1f, 3},
        {680.0f, 250.0f, 34.0f, 0.4f, 5},  {780.0f, 200.0f, 30.0f, 0.5f, 6},
        {600.0f, 340.0f, 26.0f, 0.9f, 4},  {760.0f, 330.0f, 33.0f, 0.3f, 5},
        {860.0f, 260.0f, 29.0f, 0.6f, 3},  {480.0f, 300.0f, 31.0f, 1.4f, 6},
    };
    for (const Spawn& sp : spawns) {
        w.add(makePoly(sp.x, sp.y, sp.r, sp.sides, sp.rot));
    }
    for (int s = 0; s < 480; ++s) {
        w.step(1.0f / 60.0f, 10);
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Convex Polygon Bodies";
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
                          "MAZ ENGINE  -  CONVEX POLYGON BODIES (Godot ConvexPolygonShape2D)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "triangles, pentagons and hexagons stacking via SAT + face-clip manifolds",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.4f);

            for (std::size_t i = 0; i < w.bodies.size(); ++i) {
                const game::Body2D& b = w.bodies[i];
                if (b.shape == game::Body2D::Box) {
                    const auto q = boxQuad(b);
                    renderer->drawConvexPolygon(q.data(), 4, render::Color{0.28f, 0.30f, 0.36f, 1.0f});
                } else {
                    const auto outer = convexWorld(b, 0.0f);
                    renderer->drawConvexPolygon(outer.data(), static_cast<uint32_t>(outer.size()),
                                                render::Color{0.08f, 0.09f, 0.12f, 1});
                    const auto inner = convexWorld(b, 0.12f);
                    renderer->drawConvexPolygon(inner.data(), static_cast<uint32_t>(inner.size()),
                                                palette(i));
                }
            }

            font.drawText(*renderer, 16.0f, 678.0f,
                          "one SAT + reference/incident clipping path handles any convex hull pair",
                          render::Color{0.7f, 0.8f, 0.9f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CONVEX shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
