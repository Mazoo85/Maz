// Maz Engine — "POLYLINE" (static chain / polyline terrain collider, toward Godot ConcavePolygonShape2D)
// Level terrain is rarely a single box or line — it's a run of connected segments forming hills and
// valleys. This adds Polyline as a static shape (make with makePolyline(points, thickness)); each
// segment is treated as a swept capsule, so circles, boxes and capsules rest on the terrain, roll into
// valleys, and sit on slopes exactly as they would on Godot's ConcavePolygonShape2D. The demo builds a
// rolling terrain chain and drops a mix of shapes onto it, settled up front on a fixed timestep so the
// captured frame is golden-stable.

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

void drawCapsule(render::Renderer& r, const game::Body2D& b, float pad, render::Color col) {
    const float c = std::cos(b.angle), s = std::sin(b.angle);
    const math::vec2 axis(-s, c), perp(c, s);
    const float rr = b.radius - pad;
    const math::vec2 p0 = b.pos - axis * b.half.y, p1 = b.pos + axis * b.half.y;
    const render::Point2 quad[4] = {{p0.x + perp.x * rr, p0.y + perp.y * rr},
                                    {p1.x + perp.x * rr, p1.y + perp.y * rr},
                                    {p1.x - perp.x * rr, p1.y - perp.y * rr},
                                    {p0.x - perp.x * rr, p0.y - perp.y * rr}};
    r.drawConvexPolygon(quad, 4, col);
    drawDisc(r, p0, rr, col);
    drawDisc(r, p1, rr, col);
}

// Draw a polyline chain as a thick strip: one quad per segment plus a disc at each joint (so the
// terrain reads as a continuous swept band the width of the collider's thickness).
void drawPolyline(render::Renderer& r, const game::Body2D& b, render::Color col) {
    const float ca = std::cos(b.angle), sa = std::sin(b.angle);
    auto world = [&](const math::vec2& v) {
        return math::vec2(b.pos.x + v.x * ca - v.y * sa, b.pos.y + v.x * sa + v.y * ca);
    };
    const float t = b.radius > 1.0f ? b.radius : 3.0f;
    for (std::size_t i = 0; i + 1 < b.verts.size(); ++i) {
        const math::vec2 w0 = world(b.verts[i]);
        const math::vec2 w1 = world(b.verts[i + 1]);
        math::vec2 d = w1 - w0;
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1e-4f) {
            continue;
        }
        d /= len;
        const math::vec2 n(-d.y, d.x);
        const render::Point2 quad[4] = {{w0.x + n.x * t, w0.y + n.y * t},
                                        {w1.x + n.x * t, w1.y + n.y * t},
                                        {w1.x - n.x * t, w1.y - n.y * t},
                                        {w0.x - n.x * t, w0.y - n.y * t}};
        r.drawConvexPolygon(quad, 4, col);
        drawDisc(r, w0, t, col);
    }
    if (!b.verts.empty()) {
        drawDisc(r, world(b.verts.back()), t, col);
    }
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
    MAZ_LOG_INFO("POLYLINE (static chain terrain) starting");

    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 700.0f);
    w.warmStarting = true;
    w.allowSleep = true;

    // A rolling terrain chain across the screen: hills and a valley, thickness 6 so shapes rest cleanly.
    const std::vector<math::vec2> terrain = {
        math::vec2(20.0f, 520.0f),   math::vec2(180.0f, 470.0f),  math::vec2(340.0f, 560.0f),
        math::vec2(520.0f, 620.0f),  math::vec2(700.0f, 560.0f),  math::vec2(860.0f, 460.0f),
        math::vec2(1020.0f, 540.0f), math::vec2(1180.0f, 500.0f), math::vec2(1260.0f, 560.0f)};
    w.add(game::makePolyline(terrain, 6.0f));

    // A mix of shapes dropped onto the terrain.
    struct Spawn {
        int shape;
        float x, y, r, hy;
    };
    const Spawn spawns[] = {
        {game::Body2D::Circle, 180.0f, 120.0f, 26.0f, 0.0f},
        {game::Body2D::Box, 340.0f, 80.0f, 0.0f, 0.0f},
        {game::Body2D::Circle, 520.0f, 140.0f, 30.0f, 0.0f},
        {game::Body2D::Capsule, 700.0f, 100.0f, 18.0f, 34.0f},
        {game::Body2D::Box, 860.0f, 70.0f, 0.0f, 0.0f},
        {game::Body2D::Circle, 1020.0f, 120.0f, 22.0f, 0.0f},
        {game::Body2D::Capsule, 1160.0f, 110.0f, 20.0f, 26.0f},
        {game::Body2D::Circle, 440.0f, 90.0f, 20.0f, 0.0f},
    };
    for (const Spawn& sp : spawns) {
        game::Body2D b;
        b.shape = sp.shape;
        b.radius = sp.r;
        b.half = (sp.shape == game::Body2D::Box) ? math::vec2(28.0f, 24.0f) : math::vec2(0.0f, sp.hy);
        b.pos = math::vec2(sp.x, sp.y);
        b.invMass = 1.0f;
        b.friction = 0.9f;
        b.restitution = 0.02f;
        b.enableRotation();
        w.add(b);
    }
    for (int s = 0; s < 540; ++s) {
        w.step(1.0f / 60.0f, 10);
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Polyline Chain Terrain";
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
                          "MAZ ENGINE  -  POLYLINE CHAIN TERRAIN (Godot ConcavePolygonShape2D)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "the ground is a chain of connected segments - shapes rest on hills and slopes",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.4f);

            for (std::size_t i = 0; i < w.bodies.size(); ++i) {
                const game::Body2D& b = w.bodies[i];
                if (b.shape == game::Body2D::Polyline) {
                    drawPolyline(*renderer, b, render::Color{0.35f, 0.42f, 0.5f, 1});
                } else if (b.shape == game::Body2D::Box) {
                    const auto q = boxQuad(b);
                    renderer->drawConvexPolygon(q.data(), 4, render::Color{0.08f, 0.09f, 0.12f, 1});
                    const auto q2 = boxQuad(b, 2.5f);
                    renderer->drawConvexPolygon(q2.data(), 4, palette(i));
                } else if (b.shape == game::Body2D::Capsule) {
                    drawCapsule(*renderer, b, 0.0f, render::Color{0.08f, 0.09f, 0.12f, 1});
                    drawCapsule(*renderer, b, 2.5f, palette(i));
                } else {
                    drawDisc(*renderer, b.pos, b.radius, render::Color{0.08f, 0.09f, 0.12f, 1});
                    drawDisc(*renderer, b.pos, b.radius - 2.5f, palette(i));
                }
            }

            font.drawText(*renderer, 16.0f, 678.0f,
                          "makePolyline(points, thickness)  |  each segment is a swept capsule collider",
                          render::Color{0.7f, 0.8f, 0.9f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("POLYLINE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
