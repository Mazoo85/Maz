// Maz Engine — "SHAPECAST" (game::shapeCastCircle, toward Godot's ShapeCast2D / cast_motion)
// Continuous collision detection: unlike a ray (a zero-width line) a shape-cast sweeps a CIRCLE of finite
// radius through the world and stops it a radius short of the first surface it touches — which is what
// keeps a fast projectile or a character step from tunnelling through a thin wall. This demo fans ~50
// swept probe circles out from a source point on the left through a field of static obstacles (two
// circles, an upright box, a rotated box, and a deliberately THIN wall). Each probe is drawn as its
// travel line plus the caster circle frozen at its first contact, with a short white normal at the
// contact point — so the stopped circles trace a rounded "inflated" silhouette a radius outside every
// obstacle, and the thin wall visibly blocks its lane (a ray field would leak through the gaps a circle
// cannot). All geometry is static -> deterministic, golden-stable. Run --headless / --frames N for CI.

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

// Filled circle as a convex N-gon (a single drawConvexPolygon call).
void fillCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col, int seg = 28) {
    std::vector<render::Point2> pts;
    pts.reserve(static_cast<std::size_t>(seg));
    for (int i = 0; i < seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        pts.push_back({c.x + std::cos(a) * radius, c.y + std::sin(a) * radius});
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(pts.size()), col);
}

void ringCircle(render::Renderer& r, math::vec2 c, float radius, float w, render::Color col, int seg = 28) {
    for (int i = 0; i < seg; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(seg);
        thickLine(r, {c.x + std::cos(a0) * radius, c.y + std::sin(a0) * radius},
                  {c.x + std::cos(a1) * radius, c.y + std::sin(a1) * radius}, w, col);
    }
}

// Draw one QueryShape2D (circle or oriented box) filled.
void fillShape(render::Renderer& r, const game::QueryShape2D& s, render::Color col) {
    if (s.kind == game::QueryShape2D::Circle) {
        fillCircle(r, s.pos, s.radius, col, 40);
        return;
    }
    const float ca = std::cos(s.angle), sa = std::sin(s.angle);
    auto corner = [&](float sx, float sy) {
        const math::vec2 l(sx * s.half.x, sy * s.half.y);
        return render::Point2{s.pos.x + l.x * ca - l.y * sa, s.pos.y + l.x * sa + l.y * ca};
    };
    const render::Point2 q[4] = {corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1)};
    r.drawConvexPolygon(q, 4, col);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SHAPECAST (game::shapeCastCircle) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Swept Circle Cast (continuous collision)";
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

    // ---- Static obstacle field (pixel space, +y = down) ---------------------------------------------
    std::vector<game::QueryShape2D> obstacles;
    auto addCircle = [&](float x, float y, float r) {
        game::QueryShape2D s;
        s.kind = game::QueryShape2D::Circle;
        s.pos = math::vec2(x, y);
        s.radius = r;
        obstacles.push_back(s);
    };
    auto addBox = [&](float x, float y, float hx, float hy, float deg) {
        game::QueryShape2D s;
        s.kind = game::QueryShape2D::Box;
        s.pos = math::vec2(x, y);
        s.half = math::vec2(hx, hy);
        s.angle = deg * 3.14159265f / 180.0f;
        obstacles.push_back(s);
    };
    addBox(430.0f, 250.0f, 9.0f, 150.0f, 0.0f);    // thin wall — the tunnelling test lane
    addCircle(640.0f, 220.0f, 72.0f);              // upper circle
    addBox(760.0f, 470.0f, 95.0f, 46.0f, 32.0f);   // rotated slab
    addCircle(700.0f, 620.0f, 52.0f);              // lower circle
    addBox(1010.0f, 330.0f, 60.0f, 150.0f, 0.0f);  // tall upright pillar

    // ---- Cast a fan of swept circles from a source point (deterministic) -----------------------------
    const math::vec2 source(150.0f, 380.0f);
    const float castRadius = 13.0f;
    const float reach = 1150.0f;
    const int rays = 52;
    const float a0 = -52.0f * 3.14159265f / 180.0f;
    const float a1 = 52.0f * 3.14159265f / 180.0f;

    struct Probe {
        game::ShapeCastHit2D hit;
    };
    std::vector<Probe> probes;
    probes.reserve(static_cast<std::size_t>(rays));
    for (int i = 0; i < rays; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(rays - 1);
        const float ang = a0 + (a1 - a0) * t;
        const math::vec2 dir(std::cos(ang), std::sin(ang));
        probes.push_back({game::shapeCastCircle(source, dir * reach, castRadius, obstacles)});
    }

    const render::Color kObstacle{0.24f, 0.27f, 0.34f, 1.0f};
    const render::Color kObstacleEdge{0.42f, 0.47f, 0.58f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SWEPT CIRCLE CAST", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "game::shapeCastCircle (Godot ShapeCast2D): a finite-radius circle swept until "
                          "first contact - stops a radius short, never tunnels the thin wall",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // Probe travel lines first (behind everything).
            for (const Probe& p : probes) {
                const render::Color line = p.hit.hit ? rgba(0.9f, 0.55f, 0.28f, 0.5f)
                                                     : rgba(0.32f, 0.5f, 0.7f, 0.35f);
                thickLine(*renderer, source, p.hit.safePos, 1.5f, line);
            }

            // Obstacles.
            for (const game::QueryShape2D& s : obstacles) {
                fillShape(*renderer, s, kObstacle);
            }

            // Stopped caster circles + contact normals.
            for (const Probe& p : probes) {
                if (p.hit.hit) {
                    fillCircle(*renderer, p.hit.safePos, castRadius, rgba(1.0f, 0.62f, 0.24f, 0.85f), 20);
                    ringCircle(*renderer, p.hit.safePos, castRadius, 1.4f, rgba(1.0f, 0.85f, 0.55f, 1.0f),
                               20);
                    thickLine(*renderer, p.hit.point, p.hit.point + p.hit.normal * 22.0f, 2.0f,
                              rgba(1, 1, 1, 0.9f));
                } else {
                    ringCircle(*renderer, p.hit.safePos, castRadius, 1.2f, rgba(0.45f, 0.62f, 0.85f, 0.7f),
                               16);
                }
            }

            // Obstacle outlines on top for crisp silhouettes.
            for (const game::QueryShape2D& s : obstacles) {
                if (s.kind == game::QueryShape2D::Circle) {
                    ringCircle(*renderer, s.pos, s.radius, 2.0f, kObstacleEdge, 44);
                }
            }

            // Source marker.
            fillCircle(*renderer, source, 7.0f, rgba(0.6f, 0.95f, 1.0f, 1.0f), 20);

            font.drawText(*renderer, 16.0f, 684.0f,
                          "orange = probe blocked (circle frozen at first contact, white = surface "
                          "normal)   blue ring = probe reached full range",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SHAPECAST shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
