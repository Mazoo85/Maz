// Maz Engine — "LAYERS" (collision layers & masks, toward Godot's collision_layer / collision_mask)
// Same overlap geometry as the AREA2D demo, but now each zone only REACTS to certain kinds of thing.
// Three species of agent stream across the scene — players (blue), enemies (red) and pickups (gold),
// each living on its own collision layer. Two sensor zones watch different layers: a red HURTBOX that
// scans only the ENEMY layer, and a gold MAGNET that scans only the PICKUP layer. An agent is detected
// only when it BOTH overlaps a zone AND sits on a layer that zone's mask includes — so an enemy walking
// through the magnet is ignored, and a coin drifting through the hurtbox is ignored. Detected agents get
// a bright ring in the zone's colour; agents overlapping a zone they don't match are drawn with a faint
// dashed "ignored" ring, making the filter visible. The sweep is stepped once at startup and drawn
// statically -> deterministic, golden-stable. Run --headless / --frames for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    const int seg = 28;
    render::Point2 p[30];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

void ringCircle(render::Renderer& r, math::vec2 c, float rad, float w, render::Color col, int seg = 40) {
    for (int i = 0; i < seg; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(seg);
        const render::Point2 q[4] = {{c.x + std::cos(a0) * rad, c.y + std::sin(a0) * rad},
                                     {c.x + std::cos(a1) * rad, c.y + std::sin(a1) * rad},
                                     {c.x + std::cos(a1) * (rad - w), c.y + std::sin(a1) * (rad - w)},
                                     {c.x + std::cos(a0) * (rad - w), c.y + std::sin(a0) * (rad - w)}};
        r.drawConvexPolygon(q, 4, col);
    }
}

// A dashed ring (every other segment) to signal "overlapping but ignored".
void dashedRing(render::Renderer& r, math::vec2 c, float rad, float w, render::Color col) {
    const int seg = 24;
    for (int i = 0; i < seg; i += 2) {
        const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(seg);
        const render::Point2 q[4] = {{c.x + std::cos(a0) * rad, c.y + std::sin(a0) * rad},
                                     {c.x + std::cos(a1) * rad, c.y + std::sin(a1) * rad},
                                     {c.x + std::cos(a1) * (rad - w), c.y + std::sin(a1) * (rad - w)},
                                     {c.x + std::cos(a0) * (rad - w), c.y + std::sin(a0) * (rad - w)}};
        r.drawConvexPolygon(q, 4, col);
    }
}

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

void rectOutline(render::Renderer& r, float x, float y, float w, float h, float t, render::Color c) {
    fillRect(r, x, y, w, t, c);
    fillRect(r, x, y + h - t, w, t, c);
    fillRect(r, x, y, t, h, c);
    fillRect(r, x + w - t, y, t, h, c);
}

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

struct Agent {
    int species = 0; // 0 player, 1 enemy, 2 pickup
    game::LayerMask layer = 0;
    math::vec2 start, vel;
    float radius = 8.0f;
    math::vec2 pos;
    std::vector<math::vec2> trail;
    bool inHurt = false, inMagnet = false;         // detected (overlap AND layer matches) this zone
    bool grazeHurt = false, grazeMagnet = false;    // overlapping but layer does NOT match
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LAYERS (collision layers / masks) starting");

    // Name three collision layers (registry assigns bit indices in insertion order).
    game::LayerRegistry reg;
    reg.add("player");
    reg.add("enemy");
    reg.add("pickup");
    const game::LayerMask layPlayer = reg.bit("player");
    const game::LayerMask layEnemy = reg.bit("enemy");
    const game::LayerMask layPickup = reg.bit("pickup");

    const render::Color colPlayer{0.45f, 0.7f, 1.0f, 1.0f};
    const render::Color colEnemy{1.0f, 0.42f, 0.42f, 1.0f};
    const render::Color colPickup{0.98f, 0.82f, 0.35f, 1.0f};

    // Two sensor zones, each scanning only one layer.
    game::Area2D hurt;
    hurt.shape = game::Area2D::Circle;
    hurt.pos = math::vec2(430.0f, 360.0f);
    hurt.radius = 150.0f;
    const game::LayerMask hurtMask = layEnemy; // hurtbox reacts only to enemies

    game::Area2D magnet;
    magnet.shape = game::Area2D::Box;
    magnet.pos = math::vec2(880.0f, 360.0f);
    magnet.half = math::vec2(120.0f, 150.0f);
    const game::LayerMask magnetMask = layPickup; // magnet reacts only to pickups

    // A stream of agents, cycling species so all three kinds pass through both zones.
    std::vector<Agent> agents;
    for (int i = 0; i < 15; ++i) {
        Agent a;
        a.species = i % 3;
        a.layer = (a.species == 0) ? layPlayer : (a.species == 1) ? layEnemy : layPickup;
        const float y = 170.0f + static_cast<float>(i) * 28.0f;
        a.start = math::vec2(90.0f - static_cast<float>(i) * 42.0f, y);
        const float vy = (i % 3 == 0) ? 16.0f : (i % 3 == 1 ? -10.0f : 4.0f);
        a.vel = math::vec2(150.0f, vy);
        a.pos = a.start;
        agents.push_back(a);
    }

    // Step the sweep once, layer-filtering each zone's membership through an AreaMonitor.
    game::AreaMonitor hurtMon, magnetMon;
    int hurtEnter = 0, magnetEnter = 0;
    const float dt = 1.0f / 60.0f;
    const int steps = 360;
    for (int s = 0; s < steps; ++s) {
        std::vector<int> inHurt, inMagnet;
        for (std::size_t i = 0; i < agents.size(); ++i) {
            Agent& a = agents[i];
            a.pos = a.start + a.vel * (static_cast<float>(s) * dt);
            if (s % 6 == 0) {
                a.trail.push_back(a.pos);
            }
            game::Area2D body;
            body.shape = game::Area2D::Circle;
            body.pos = a.pos;
            body.radius = a.radius;

            const bool overHurt = game::overlaps(hurt, body);
            const bool overMag = game::overlaps(magnet, body);
            // Detected only when overlap AND the zone's mask includes the agent's layer.
            a.grazeHurt = overHurt && !game::detects(hurtMask, a.layer);
            a.grazeMagnet = overMag && !game::detects(magnetMask, a.layer);
            if (overHurt && game::detects(hurtMask, a.layer)) {
                inHurt.push_back(static_cast<int>(i));
            }
            if (overMag && game::detects(magnetMask, a.layer)) {
                inMagnet.push_back(static_cast<int>(i));
            }
        }
        std::vector<int> ent, ext;
        hurtMon.update(inHurt, ent, ext);
        hurtEnter += static_cast<int>(ent.size());
        magnetMon.update(inMagnet, ent, ext);
        magnetEnter += static_cast<int>(ent.size());
    }
    for (std::size_t i = 0; i < agents.size(); ++i) {
        agents[i].inHurt = hurtMon.contains(static_cast<int>(i));
        agents[i].inMagnet = magnetMon.contains(static_cast<int>(i));
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Collision Layers & Masks";
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

    auto speciesColor = [&](int sp) {
        return sp == 0 ? colPlayer : sp == 1 ? colEnemy : colPickup;
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  COLLISION LAYERS & MASKS",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "each zone only reacts to the layers its mask includes - overlap alone is not "
                          "enough (game::CollisionLayers)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            // Zones.
            fillCircle(*renderer, hurt.pos, hurt.radius,
                       render::Color{colEnemy.r, colEnemy.g, colEnemy.b, 0.1f});
            ringCircle(*renderer, hurt.pos, hurt.radius, 2.5f,
                       render::Color{colEnemy.r, colEnemy.g, colEnemy.b, 0.8f});
            fillRect(*renderer, magnet.pos.x - magnet.half.x, magnet.pos.y - magnet.half.y,
                     magnet.half.x * 2.0f, magnet.half.y * 2.0f,
                     render::Color{colPickup.r, colPickup.g, colPickup.b, 0.1f});
            rectOutline(*renderer, magnet.pos.x - magnet.half.x, magnet.pos.y - magnet.half.y,
                        magnet.half.x * 2.0f, magnet.half.y * 2.0f, 2.5f,
                        render::Color{colPickup.r, colPickup.g, colPickup.b, 0.8f});

            // Trails.
            for (const Agent& a : agents) {
                const render::Color tc{speciesColor(a.species).r, speciesColor(a.species).g,
                                       speciesColor(a.species).b, 0.28f};
                for (std::size_t i = 1; i < a.trail.size(); ++i) {
                    thickLine(*renderer, a.trail[i - 1], a.trail[i], 1.3f, tc);
                }
            }

            // Agents: bright ring when DETECTED, faint dashed ring when overlapping-but-ignored.
            for (const Agent& a : agents) {
                if (a.inHurt) {
                    ringCircle(*renderer, a.pos, a.radius + 6.0f, 3.0f, colEnemy);
                } else if (a.grazeHurt) {
                    dashedRing(*renderer, a.pos, a.radius + 6.0f, 2.0f,
                               render::Color{0.6f, 0.6f, 0.66f, 0.7f});
                }
                if (a.inMagnet) {
                    ringCircle(*renderer, a.pos, a.radius + 10.0f, 3.0f, colPickup);
                } else if (a.grazeMagnet) {
                    dashedRing(*renderer, a.pos, a.radius + 10.0f, 2.0f,
                               render::Color{0.6f, 0.6f, 0.66f, 0.7f});
                }
                fillCircle(*renderer, a.pos, a.radius, speciesColor(a.species));
            }

            // Per-zone readouts.
            auto stat = [&](float x, float y, const char* name, const char* watches, render::Color col,
                            int now, int en) {
                font.drawText(*renderer, x, y, name, col, 0.44f);
                font.drawText(*renderer, x, y + 28.0f, watches,
                              render::Color{0.8f, 0.83f, 0.9f, 1}, 0.34f);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "detected now: %d   entered: %d", now, en);
                font.drawText(*renderer, x, y + 52.0f, buf, render::Color{0.7f, 0.74f, 0.82f, 1}, 0.34f);
            };
            stat(60.0f, 600.0f, "HURTBOX (circle)", "watches: ENEMY only", colEnemy,
                 static_cast<int>(hurtMon.members().size()), hurtEnter);
            stat(460.0f, 600.0f, "MAGNET (box)", "watches: PICKUP only", colPickup,
                 static_cast<int>(magnetMon.members().size()), magnetEnter);

            // Legend: the three layers.
            font.drawText(*renderer, 900.0f, 590.0f, "layers:", render::Color{0.7f, 0.74f, 0.82f, 1}, 0.34f);
            fillCircle(*renderer, math::vec2(910.0f, 622.0f), 7.0f, colPlayer);
            font.drawText(*renderer, 924.0f, 612.0f, "player", colPlayer, 0.32f);
            fillCircle(*renderer, math::vec2(1010.0f, 622.0f), 7.0f, colEnemy);
            font.drawText(*renderer, 1024.0f, 612.0f, "enemy", colEnemy, 0.32f);
            fillCircle(*renderer, math::vec2(1110.0f, 622.0f), 7.0f, colPickup);
            font.drawText(*renderer, 1124.0f, 612.0f, "pickup", colPickup, 0.32f);
            font.drawText(*renderer, 900.0f, 648.0f, "dashed ring = overlapping but ignored",
                          render::Color{0.6f, 0.63f, 0.7f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LAYERS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
