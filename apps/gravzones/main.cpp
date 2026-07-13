// Maz Engine — "GRAVZONES" (area gravity fields, toward Godot's Area2D gravity override)
// Three zones change the gravity a body feels inside them: a WIND field (directional, added on top of the
// world's downward pull), an UPDRAFT field (directional, replacing gravity with an upward push), and an
// ATTRACTOR (point field, pulling toward a centre with inverse-square falloff). A deterministic fixed-step
// sim drops balls from the top; each step reads game::gravityAt at the ball's position, so their trails
// bend right in the wind, U-turn in the updraft, and curve inward toward the attractor. Fixed initial
// state + fixed step count -> golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, const math::Rect2& b, render::Color c) {
    const render::Point2 p[4] = {{b.left(), b.top()},
                                 {b.right(), b.top()},
                                 {b.right(), b.bottom()},
                                 {b.left(), b.bottom()}};
    r.drawConvexPolygon(p, 4, c);
}

void fillCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 18;
    render::Point2 pts[18];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

void bar(render::Renderer& r, math::vec2 a, math::vec2 b, float w, render::Color c) {
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

// An arrow from `a` to `b` (line + a small triangular head).
void arrow(render::Renderer& r, math::vec2 a, math::vec2 b, render::Color c) {
    bar(r, a, b, 4.0f, c);
    math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y, d.x);
    const render::Point2 head[3] = {{b.x + d.x * 14.0f, b.y + d.y * 14.0f},
                                    {b.x + n.x * 9.0f, b.y + n.y * 9.0f},
                                    {b.x - n.x * 9.0f, b.y - n.y * 9.0f}};
    r.drawConvexPolygon(head, 3, c);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

struct Ball {
    math::vec2 pos;
    math::vec2 vel;
    render::Color color;
    std::vector<math::vec2> trail;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GRAVZONES (area gravity fields) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Area Gravity Fields";
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

    // ---- The gravity zones. --------------------------------------------------------------------------
    const math::vec2 baseGravity(0.0f, 640.0f); // world default: down

    game::GravityArea2D wind;
    wind.region = math::Rect2(150.0f, 150.0f, 260.0f, 460.0f);
    wind.type = game::GravityType::Directional;
    wind.mode = game::GravityMode::Add;
    wind.direction = math::vec2(1.0f, 0.0f);
    wind.strength = 900.0f;

    game::GravityArea2D updraft;
    updraft.region = math::Rect2(500.0f, 150.0f, 220.0f, 460.0f);
    updraft.type = game::GravityType::Directional;
    updraft.mode = game::GravityMode::Replace;
    updraft.direction = math::vec2(0.0f, -1.0f);
    updraft.strength = 1050.0f;

    game::GravityArea2D attractor;
    attractor.region = math::Rect2(820.0f, 150.0f, 380.0f, 460.0f);
    attractor.type = game::GravityType::Point;
    attractor.mode = game::GravityMode::Replace;
    attractor.center = math::vec2(1010.0f, 400.0f);
    attractor.strength = 1500.0f;
    attractor.unitDistance = 150.0f;

    const std::vector<game::GravityArea2D> zones = {wind, updraft, attractor};

    // ---- Deterministic fixed-step drop simulation. ---------------------------------------------------
    std::vector<Ball> balls = {
        {math::vec2(210.0f, 110.0f), math::vec2(0, 0), rgba(0.95f, 0.55f, 0.35f, 1), {}},
        {math::vec2(320.0f, 110.0f), math::vec2(0, 0), rgba(1.0f, 0.8f, 0.4f, 1), {}},
        {math::vec2(600.0f, 110.0f), math::vec2(0, 0), rgba(0.45f, 0.85f, 1.0f, 1), {}},
        {math::vec2(680.0f, 110.0f), math::vec2(0, 0), rgba(0.6f, 0.95f, 0.85f, 1), {}},
        {math::vec2(900.0f, 110.0f), math::vec2(0, 0), rgba(0.75f, 0.7f, 1.0f, 1), {}},
        {math::vec2(1120.0f, 110.0f), math::vec2(0, 0), rgba(1.0f, 0.6f, 0.75f, 1), {}},
    };
    const float dt = 1.0f / 60.0f;
    const int steps = 110;
    const float radius = 11.0f;
    for (Ball& b : balls) {
        b.trail.push_back(b.pos);
    }
    for (int s = 0; s < steps; ++s) {
        for (Ball& b : balls) {
            const math::vec2 g = game::gravityAt(zones, b.pos, baseGravity);
            b.vel += g * dt;
            b.pos += b.vel * dt;
            if (s % 2 == 0) {
                b.trail.push_back(b.pos);
            }
        }
    }

    struct ZoneViz {
        const game::GravityArea2D* z;
        render::Color tint;
        const char* label;
    };
    const ZoneViz viz[3] = {
        {&zones[0], rgba(0.45f, 0.72f, 1.0f, 0.14f), "wind  ->  (add)"},
        {&zones[1], rgba(0.5f, 0.9f, 0.6f, 0.14f), "updraft  ^  (replace)"},
        {&zones[2], rgba(1.0f, 0.7f, 0.5f, 0.14f), "attractor  *  (point)"},
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  AREA GRAVITY FIELDS", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "each zone overrides gravity inside it; ball paths bend per game::gravityAt "
                          "(Godot Area2D gravity)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.32f);

            // Zones: translucent fills + a direction arrow (directional) or centre dot (point).
            for (const ZoneViz& v : viz) {
                const math::Rect2& r = v.z->region;
                fillRect(*renderer, r, v.tint);
                const math::vec2 c = r.center();
                if (v.z->type == game::GravityType::Directional) {
                    const math::vec2 d = v.z->direction;
                    arrow(*renderer, math::vec2(c.x - d.x * 46.0f, c.y - d.y * 46.0f),
                          math::vec2(c.x + d.x * 46.0f, c.y + d.y * 46.0f), rgba(0.9f, 0.93f, 1.0f, 0.5f));
                } else {
                    fillCircle(*renderer, v.z->center, 9.0f, rgba(1.0f, 0.85f, 0.55f, 0.9f));
                }
                font.drawText(*renderer, r.left() + 8.0f, r.top() + 8.0f, v.label,
                              rgba(0.82f, 0.86f, 0.94f, 1), 0.3f);
            }

            // Trails then balls.
            for (const Ball& b : balls) {
                render::Color faint = b.color;
                faint.a = 0.4f;
                for (const math::vec2& t : b.trail) {
                    fillCircle(*renderer, t, 2.5f, faint);
                }
            }
            for (const Ball& b : balls) {
                fillCircle(*renderer, b.pos, radius, b.color);
            }

            font.drawText(*renderer, 40.0f, 666.0f,
                          "trails: right-drift in the wind, U-turn in the updraft, inward curl toward the "
                          "attractor centre",
                          rgba(0.6f, 0.64f, 0.72f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GRAVZONES shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
