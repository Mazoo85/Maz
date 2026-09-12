// Maz Engine — "AREA2D" (sensor / trigger regions with enter/exit events, toward Godot Area2D)
// Two trigger zones — a circular "aura" and a rectangular "gate" — sit in the scene while a stream of
// agents drifts across on fixed paths. An Area2D doesn't push anything; it just watches which agents
// OVERLAP it, and an AreaMonitor turns that into ENTER / EXIT events as agents cross each boundary. The
// whole sweep is stepped once at startup: each agent is drawn at its final position (bright + ringed in a
// zone's colour if it's currently inside that zone), with its trail, and each zone shows how many agents
// are inside now and how many enter/exit events fired over the run. Deterministic -> golden-stable.

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

void ringCircle(render::Renderer& r, math::vec2 c, float rad, float w, render::Color col) {
    const int seg = 40;
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
    math::vec2 start, vel;
    float agentRadius = 8.0f;
    math::vec2 pos;
    std::vector<math::vec2> trail;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("AREA2D (sensor / trigger) starting");

    // Two trigger zones.
    game::Area2D aura;
    aura.shape = game::Area2D::Circle;
    aura.pos = math::vec2(430.0f, 380.0f);
    aura.radius = 150.0f;
    const render::Color auraCol{0.45f, 0.75f, 1.0f, 1.0f};

    game::Area2D gate;
    gate.shape = game::Area2D::Box;
    gate.pos = math::vec2(880.0f, 380.0f);
    gate.half = math::vec2(120.0f, 150.0f);
    const render::Color gateCol{0.95f, 0.6f, 0.4f, 1.0f};

    // A stream of agents on fixed diagonal/horizontal paths that thread through both zones.
    std::vector<Agent> agents;
    for (int i = 0; i < 14; ++i) {
        Agent a;
        const float y = 180.0f + static_cast<float>(i) * 30.0f;
        // Stagger each agent's start behind the last so at the final frame the stream spreads across the
        // scene — some inside the aura, some inside the gate, some between — showing live membership.
        a.start = math::vec2(80.0f - static_cast<float>(i) * 44.0f, y);
        const float vy = (i % 3 == 0) ? 18.0f : (i % 3 == 1 ? -12.0f : 0.0f);
        a.vel = math::vec2(150.0f, vy);
        a.pos = a.start;
        agents.push_back(a);
    }

    // Step the whole sweep once, tracking each zone's membership + enter/exit counts.
    game::AreaMonitor auraMon, gateMon;
    int auraEnter = 0, auraExit = 0, gateEnter = 0, gateExit = 0;
    const float dt = 1.0f / 60.0f;
    const int steps = 360; // ~6s: the staggered stream ends spread across both zones
    for (int s = 0; s < steps; ++s) {
        std::vector<int> inAura, inGate;
        for (std::size_t i = 0; i < agents.size(); ++i) {
            Agent& a = agents[i];
            a.pos = a.start + a.vel * (static_cast<float>(s) * dt);
            if (s % 6 == 0) {
                a.trail.push_back(a.pos);
            }
            game::Area2D body;
            body.shape = game::Area2D::Circle;
            body.pos = a.pos;
            body.radius = a.agentRadius;
            if (game::overlaps(aura, body)) {
                inAura.push_back(static_cast<int>(i));
            }
            if (game::overlaps(gate, body)) {
                inGate.push_back(static_cast<int>(i));
            }
        }
        std::vector<int> ent, ext;
        auraMon.update(inAura, ent, ext);
        auraEnter += static_cast<int>(ent.size());
        auraExit += static_cast<int>(ext.size());
        gateMon.update(inGate, ent, ext);
        gateEnter += static_cast<int>(ent.size());
        gateExit += static_cast<int>(ext.size());
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Area2D Sensors";
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

        renderer->setClearColor(render::Color{0.06f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  AREA2D SENSORS",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "trigger zones that fire enter/exit as agents cross them - no pushing, just "
                          "detection (game::Area2D)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            // Zones (filled translucent + outline).
            fillCircle(*renderer, aura.pos, aura.radius,
                       render::Color{auraCol.r, auraCol.g, auraCol.b, 0.12f});
            ringCircle(*renderer, aura.pos, aura.radius, 2.5f,
                       render::Color{auraCol.r, auraCol.g, auraCol.b, 0.8f});
            fillRect(*renderer, gate.pos.x - gate.half.x, gate.pos.y - gate.half.y, gate.half.x * 2.0f,
                     gate.half.y * 2.0f, render::Color{gateCol.r, gateCol.g, gateCol.b, 0.12f});
            rectOutline(*renderer, gate.pos.x - gate.half.x, gate.pos.y - gate.half.y,
                        gate.half.x * 2.0f, gate.half.y * 2.0f, 2.5f,
                        render::Color{gateCol.r, gateCol.g, gateCol.b, 0.8f});

            // Agent trails.
            for (const Agent& a : agents) {
                for (std::size_t i = 1; i < a.trail.size(); ++i) {
                    thickLine(*renderer, a.trail[i - 1], a.trail[i], 1.4f,
                              render::Color{0.5f, 0.53f, 0.62f, 0.5f});
                }
            }

            // Agents: ring in a zone's colour when currently inside it.
            for (std::size_t i = 0; i < agents.size(); ++i) {
                const Agent& a = agents[i];
                const bool inA = auraMon.contains(static_cast<int>(i));
                const bool inG = gateMon.contains(static_cast<int>(i));
                if (inA) {
                    ringCircle(*renderer, a.pos, a.agentRadius + 6.0f, 3.0f, auraCol);
                }
                if (inG) {
                    ringCircle(*renderer, a.pos, a.agentRadius + 10.0f, 3.0f, gateCol);
                }
                const render::Color body = (inA || inG) ? render::Color{0.98f, 0.99f, 1.0f, 1.0f}
                                                        : render::Color{0.45f, 0.48f, 0.56f, 1.0f};
                fillCircle(*renderer, a.pos, a.agentRadius, body);
            }

            // Per-zone readouts.
            auto stat = [&](float x, float y, const char* name, render::Color col, int inside, int en,
                            int ex) {
                font.drawText(*renderer, x, y, name, col, 0.44f);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "inside now: %d", inside);
                font.drawText(*renderer, x, y + 30.0f, buf, render::Color{0.85f, 0.88f, 0.95f, 1}, 0.36f);
                std::snprintf(buf, sizeof(buf), "entered: %d   exited: %d", en, ex);
                font.drawText(*renderer, x, y + 56.0f, buf, render::Color{0.7f, 0.74f, 0.82f, 1}, 0.34f);
            };
            stat(70.0f, 600.0f, "AURA (circle sensor)", auraCol,
                 static_cast<int>(auraMon.members().size()), auraEnter, auraExit);
            stat(470.0f, 600.0f, "GATE (box sensor)", gateCol,
                 static_cast<int>(gateMon.members().size()), gateEnter, gateExit);
            font.drawText(*renderer, 900.0f, 600.0f, "a lit ring = that agent is currently",
                          render::Color{0.66f, 0.7f, 0.8f, 1}, 0.32f);
            font.drawText(*renderer, 900.0f, 626.0f, "inside the zone of that colour",
                          render::Color{0.66f, 0.7f, 0.8f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("AREA2D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
