// Maz Engine — "AVOID" (RVO local collision avoidance, toward Godot's NavigationAgent2D avoidance)
// The classic reciprocal-velocity-obstacle test: agents spaced around a circle each head for the point
// directly opposite, so every path crosses the crowded centre. game::rvoVelocity nudges each agent's
// preferred (toward-goal) velocity to dodge its neighbours, reciprocally, so they swirl through the
// middle without colliding and re-form on the far side. The whole crossing is simulated once at startup
// (fixed timestep) and each agent's TRAIL recorded, then drawn statically — so the render is fully
// deterministic and golden-stable regardless of capture timing. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col, int sides = 20) {
    std::vector<render::Point2> p(static_cast<size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        p[static_cast<size_t>(i)] = render::Point2{c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p.data(), static_cast<uint32_t>(sides), col);
}

render::Color hue(float h) {
    h -= std::floor(h);
    const float r = std::fabs(h * 6.0f - 3.0f) - 1.0f;
    const float g = 2.0f - std::fabs(h * 6.0f - 2.0f);
    const float b = 2.0f - std::fabs(h * 6.0f - 4.0f);
    auto cl = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    return render::Color{cl(r), cl(g), cl(b), 1.0f};
}

struct Agent {
    math::vec2 pos, vel, goal;
    std::vector<math::vec2> trail;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("AVOID (RVO local avoidance) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — RVO Avoidance";
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

    const math::vec2 center(static_cast<float>(cfg.width) * 0.5f,
                            static_cast<float>(cfg.height) * 0.5f + 12.0f);
    const float ring = 262.0f;
    const float agentR = 13.0f;
    const float maxSpeed = 150.0f;
    const int count = 14;

    std::vector<Agent> agents(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
        Agent& ag = agents[static_cast<size_t>(i)];
        ag.pos = center + math::vec2(std::cos(a), std::sin(a)) * ring;
        ag.goal = center - math::vec2(std::cos(a), std::sin(a)) * ring; // antipodal
        ag.vel = math::vec2(0.0f, 0.0f);
        ag.trail.push_back(ag.pos);
    }

    // Simulate the whole crossing once, deterministically, recording trails.
    const float dt = 1.0f / 60.0f;
    for (int step = 0; step < 380; ++step) {
        std::vector<math::vec2> newVel(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            Agent& ag = agents[static_cast<size_t>(i)];
            math::vec2 d = ag.goal - ag.pos;
            const float dist = std::sqrt(d.x * d.x + d.y * d.y);
            const math::vec2 pref =
                dist > 2.0f ? d / dist * std::min(maxSpeed, dist / dt) : math::vec2(0.0f, 0.0f);
            std::vector<game::AvoidNeighbor> nb;
            for (int j = 0; j < count; ++j) {
                if (j != i) {
                    nb.push_back({agents[static_cast<size_t>(j)].pos,
                                  agents[static_cast<size_t>(j)].vel, agentR});
                }
            }
            newVel[static_cast<size_t>(i)] =
                game::rvoVelocity(ag.pos, ag.vel, pref, agentR, maxSpeed, nb, 2.5f);
        }
        for (int i = 0; i < count; ++i) {
            Agent& ag = agents[static_cast<size_t>(i)];
            ag.vel = newVel[static_cast<size_t>(i)];
            ag.pos += ag.vel * dt;
            if (step % 2 == 0) {
                ag.trail.push_back(ag.pos);
            }
        }
    }

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

            fillCircle(*renderer, center, 4.0f, render::Color{0.3f, 0.32f, 0.4f, 1});

            for (int i = 0; i < count; ++i) {
                const Agent& ag = agents[static_cast<size_t>(i)];
                render::Color col = hue(static_cast<float>(i) / static_cast<float>(count));
                // Trail: fade from faint (start) to full (end).
                for (size_t k = 1; k < ag.trail.size(); ++k) {
                    render::Color t = col;
                    t.a = 0.12f + 0.55f * static_cast<float>(k) / static_cast<float>(ag.trail.size());
                    thickLine(*renderer, ag.trail[k - 1], ag.trail[k], 3.0f, t);
                }
                // Start marker (hollow) + final agent (solid).
                fillCircle(*renderer, ag.trail.front(), 4.0f, render::Color{col.r, col.g, col.b, 0.5f});
                fillCircle(*renderer, ag.pos, agentR, col);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RVO LOCAL AVOIDANCE",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "agents cross to the opposite side; reciprocal velocity obstacles route them "
                          "around each other",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("AVOID shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
