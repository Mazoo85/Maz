// Maz Engine — "FLOWFIELD" (flow-field / vector-field pathfinding, for crowds)
// One Dijkstra outward from the GOAL bakes a direction into every grid cell (game::FlowField); then a
// whole crowd navigates for free, each agent just reading the arrow under its feet. This draws the
// integration field as a heat map (bright near the goal, cold far away), the baked flow as a grid of
// arrows, the walls, the goal, and 90 agents that were released on the left and streamed along the field
// around the barriers to the goal — their trails show the flow lines. Everything is computed once at
// startup with a fixed timestep, so the render is deterministic and golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
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

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    const int seg = 12;
    render::Point2 p[14];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

// Heat colour for a normalized cost t in [0,1]: 0 = at the goal (warm), 1 = far (cold).
render::Color heat(float t) {
    const math::vec3 hot(0.98f, 0.85f, 0.35f), mid(0.30f, 0.70f, 0.55f), cold(0.15f, 0.19f, 0.42f);
    math::vec3 c = t < 0.5f ? glm::mix(hot, mid, t * 2.0f) : glm::mix(mid, cold, (t - 0.5f) * 2.0f);
    return render::Color{c.x, c.y, c.z, 1.0f};
}

struct Agent {
    math::vec2 pos;
    std::vector<math::vec2> trail;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("FLOWFIELD (flow-field pathfinding) starting");

    // --- Grid + walls + goal. -------------------------------------------------------------------------
    const int cols = 40, rows = 20;
    const float ox = 20.0f, oy = 96.0f;
    const float cell = 30.0f;
    std::vector<std::uint8_t> blocked(static_cast<std::size_t>(cols * rows), 0);
    auto wall = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < cols && y < rows) {
            blocked[static_cast<std::size_t>(y * cols + x)] = 1;
        }
    };
    // Two serpentine barriers with a gap each, so the crowd must weave.
    for (int y = 0; y <= 13; ++y) wall(12, y);   // barrier 1: gap along the bottom
    for (int y = 6; y < rows; ++y) wall(26, y);  // barrier 2: gap along the top
    const int goalX = cols - 2, goalY = rows / 2;

    game::FlowField ff;
    ff.build(cols, rows, blocked, goalX, goalY);

    // Highest reachable cost, to normalize the heat map.
    float maxCost = 1.0f;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const float c = ff.costAt(x, y);
            if (ff.reachable(x, y) && c > maxCost) {
                maxCost = c;
            }
        }
    }

    // --- Release a crowd on the left and stream it along the field. -----------------------------------
    const math::vec2 origin(ox, oy);
    std::vector<Agent> agents;
    for (int i = 0; i < 90; ++i) {
        const int row = i % rows;
        const int band = i / rows; // a few columns of agents
        Agent a;
        a.pos = math::vec2(ox + (0.6f + static_cast<float>(band) * 0.7f) * cell,
                           oy + (static_cast<float>(row) + 0.5f) * cell);
        if (ff.reachable(static_cast<int>((a.pos.x - ox) / cell),
                         static_cast<int>((a.pos.y - oy) / cell))) {
            agents.push_back(a);
        }
    }
    const float speed = 46.0f; // px/sec
    const float dt = 1.0f / 60.0f;
    for (int step = 0; step < 640; ++step) {
        for (Agent& a : agents) {
            const math::vec2 f = ff.sampleFlow(a.pos, cell, origin);
            if (f.x == 0.0f && f.y == 0.0f) {
                continue; // at the goal or stuck
            }
            a.pos += f * (speed * dt);
            if (step % 8 == 0) {
                a.trail.push_back(a.pos);
            }
        }
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Flow-Field Pathfinding";
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

            // Integration-field heat map + walls.
            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    const float px = ox + static_cast<float>(x) * cell;
                    const float py = oy + static_cast<float>(y) * cell;
                    if (blocked[static_cast<std::size_t>(y * cols + x)]) {
                        fillRect(*renderer, px, py, cell - 1.0f, cell - 1.0f,
                                 render::Color{0.20f, 0.21f, 0.26f, 1.0f});
                    } else if (ff.reachable(x, y)) {
                        fillRect(*renderer, px, py, cell - 1.0f, cell - 1.0f,
                                 heat(ff.costAt(x, y) / maxCost));
                    } else {
                        fillRect(*renderer, px, py, cell - 1.0f, cell - 1.0f,
                                 render::Color{0.09f, 0.09f, 0.12f, 1.0f});
                    }
                }
            }

            // Flow arrows.
            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    if (!ff.reachable(x, y)) {
                        continue;
                    }
                    const math::vec2 f = ff.flowAt(x, y);
                    if (f.x == 0.0f && f.y == 0.0f) {
                        continue;
                    }
                    const math::vec2 c(ox + (static_cast<float>(x) + 0.5f) * cell,
                                       oy + (static_cast<float>(y) + 0.5f) * cell);
                    const math::vec2 tip = c + f * (cell * 0.34f);
                    thickLine(*renderer, c - f * (cell * 0.30f), tip, 2.0f,
                              render::Color{0.06f, 0.07f, 0.10f, 1.0f});
                    // little arrowhead
                    const math::vec2 perp(-f.y, f.x);
                    thickLine(*renderer, tip, tip - f * 5.0f + perp * 4.0f, 2.0f,
                              render::Color{0.06f, 0.07f, 0.10f, 1.0f});
                    thickLine(*renderer, tip, tip - f * 5.0f - perp * 4.0f, 2.0f,
                              render::Color{0.06f, 0.07f, 0.10f, 1.0f});
                }
            }

            // Agent trails then agents.
            for (const Agent& a : agents) {
                for (std::size_t i = 1; i < a.trail.size(); ++i) {
                    thickLine(*renderer, a.trail[i - 1], a.trail[i], 1.6f,
                              render::Color{0.98f, 0.99f, 1.0f, 0.5f});
                }
            }
            for (const Agent& a : agents) {
                fillCircle(*renderer, a.pos, 4.0f, render::Color{1.0f, 1.0f, 1.0f, 1.0f});
                fillCircle(*renderer, a.pos, 2.0f, render::Color{0.15f, 0.35f, 0.85f, 1.0f});
            }

            // Goal marker.
            const math::vec2 g(ox + (static_cast<float>(goalX) + 0.5f) * cell,
                               oy + (static_cast<float>(goalY) + 0.5f) * cell);
            fillCircle(*renderer, g, 12.0f, render::Color{1.0f, 0.35f, 0.35f, 1.0f});
            fillCircle(*renderer, g, 5.0f, render::Color{1, 1, 1, 1});

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  FLOW-FIELD PATHFINDING",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "one Dijkstra from the goal bakes a direction into every cell; 90 agents then "
                          "stream around the walls to it for free",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);
            font.drawText(*renderer, ox, oy + static_cast<float>(rows) * cell + 8.0f,
                          "heat = cost-to-goal (warm near, cold far)   arrows = baked flow   dots = the "
                          "crowd, trails = the streams they followed",
                          render::Color{0.7f, 0.75f, 0.85f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("FLOWFIELD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
