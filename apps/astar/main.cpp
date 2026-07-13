// Maz Engine — "ASTAR" (game::AStar2D general weighted-graph pathfinding, toward Godot's AStar2D)
// A free-form waypoint graph: nodes placed anywhere, edges you draw yourself, and a least-cost route
// found across them. LEFT panel: a road network of 11 junctions with two-way links (thin blue) and
// one heavily-weighted junction (red, "toll") that the shortest path from START (green) to GOAL
// (orange) routes AROUND — the chosen route is drawn as a thick amber ribbon over its edges and dots.
// RIGHT panel: getClosestPositionInSegment — a free query point (white) snapped onto the nearest edge
// of a small graph (amber projection line to the snap point). Fixed geometry -> deterministic, so it
// unit-tests exactly and drives a golden. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

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

void dot(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 18;
    render::Point2 pts[18];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

void ring(render::Renderer& r, math::vec2 c, float radius, float w, render::Color col) {
    const int n = 36;
    math::vec2 prev;
    for (int i = 0; i <= n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        const math::vec2 p(c.x + std::cos(a) * radius, c.y + std::sin(a) * radius);
        if (i > 0) {
            thickLine(r, prev, p, w, col);
        }
        prev = p;
    }
}

void panelBox(render::Renderer& r, float x0, float y0, float x1, float y1) {
    const render::Point2 q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    r.drawConvexPolygon(q, 4, rgba(0.12f, 0.13f, 0.17f, 1));
}

bool inPath(const std::vector<int64_t>& path, int64_t a, int64_t b) {
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        if ((path[i] == a && path[i + 1] == b) || (path[i] == b && path[i + 1] == a)) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ASTAR (game::AStar2D) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — AStar2D";
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

    using math::vec2;

    // ---- LEFT: a road network. Ids 0..10; junction 5 carries a heavy weight ("toll"). ----------
    game::AStar2D road;
    struct NodeDef {
        int64_t id;
        vec2 pos;
        float weight;
    };
    const NodeDef nodes[] = {
        {0, vec2(90, 500), 1.0f},   // START (bottom-left)
        {1, vec2(210, 360), 1.0f},  {2, vec2(150, 200), 1.0f}, {3, vec2(330, 210), 1.0f},
        {4, vec2(300, 430), 1.0f},  {5, vec2(430, 350), 6.0f}, // heavy "toll" junction (centre)
        {6, vec2(560, 200), 1.0f},  {7, vec2(540, 470), 1.0f}, {8, vec2(680, 340), 1.0f},
        {9, vec2(760, 210), 1.0f},  {10, vec2(790, 500), 1.0f}, // GOAL (bottom-right)
    };
    for (const NodeDef& n : nodes) {
        road.addPoint(n.id, n.pos, n.weight);
    }
    const std::pair<int64_t, int64_t> edges[] = {
        {0, 1}, {1, 2}, {2, 3}, {1, 4}, {3, 5}, {4, 5}, {3, 6},  {5, 6},
        {4, 7}, {5, 8}, {6, 9}, {7, 8}, {8, 9}, {8, 10}, {9, 10}, {7, 10},
    };
    for (const auto& e : edges) {
        road.connectPoints(e.first, e.second);
    }
    const std::vector<int64_t> path = road.getIdPath(0, 10);

    // ---- RIGHT: a tiny graph for closest-position-in-segment. -----------------------------------
    game::AStar2D snap;
    snap.addPoint(0, vec2(920, 200));
    snap.addPoint(1, vec2(1180, 250));
    snap.addPoint(2, vec2(980, 520));
    snap.addPoint(3, vec2(1160, 470));
    snap.connectPoints(0, 1);
    snap.connectPoints(0, 2);
    snap.connectPoints(1, 3);
    snap.connectPoints(2, 3);
    const vec2 query(1080.0f, 360.0f);
    const vec2 snapped = snap.getClosestPositionInSegment(query);

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ASTAR2D", rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "arbitrary weighted waypoint graph A* (game::AStar2D, Godot AStar2D)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // ---- LEFT panel: road network + shortest path -------------------------------------
            panelBox(*renderer, 40.0f, 120.0f, 850.0f, 600.0f);

            // All edges first (thin), then path edges thick over them.
            for (const auto& e : edges) {
                thickLine(*renderer, road.getPointPosition(e.first), road.getPointPosition(e.second),
                          2.0f, rgba(0.32f, 0.4f, 0.55f, 1));
            }
            for (const auto& e : edges) {
                if (inPath(path, e.first, e.second)) {
                    thickLine(*renderer, road.getPointPosition(e.first),
                              road.getPointPosition(e.second), 7.0f, rgba(1.0f, 0.75f, 0.28f, 1));
                }
            }
            // Nodes.
            for (const NodeDef& n : nodes) {
                const vec2 p = n.pos;
                if (n.id == 0) {
                    dot(*renderer, p, 12.0f, rgba(0.45f, 0.95f, 0.5f, 1)); // START green
                } else if (n.id == 10) {
                    dot(*renderer, p, 12.0f, rgba(1.0f, 0.62f, 0.2f, 1)); // GOAL orange
                } else if (n.weight > 1.5f) {
                    dot(*renderer, p, 13.0f, rgba(0.92f, 0.32f, 0.34f, 1)); // heavy toll red
                    ring(*renderer, p, 18.0f, 2.0f, rgba(0.92f, 0.5f, 0.5f, 1));
                } else {
                    dot(*renderer, p, 8.0f, rgba(0.55f, 0.72f, 0.95f, 1));
                }
            }
            font.drawText(*renderer, 48.0f, 96.0f,
                          "road net: START->GOAL routes around the red toll junction (weight x6)",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            // ---- RIGHT panel: closest-position-in-segment -------------------------------------
            panelBox(*renderer, 880.0f, 120.0f, 1240.0f, 600.0f);
            const int64_t snapEdges[][2] = {{0, 1}, {0, 2}, {1, 3}, {2, 3}};
            for (const auto& e : snapEdges) {
                thickLine(*renderer, snap.getPointPosition(e[0]), snap.getPointPosition(e[1]), 2.5f,
                          rgba(0.5f, 0.65f, 0.9f, 1));
            }
            for (int64_t id = 0; id <= 3; ++id) {
                dot(*renderer, snap.getPointPosition(id), 7.0f, rgba(0.6f, 0.75f, 0.95f, 1));
            }
            thickLine(*renderer, query, snapped, 2.0f, rgba(1.0f, 0.8f, 0.45f, 0.9f));
            dot(*renderer, snapped, 5.0f, rgba(1.0f, 0.8f, 0.45f, 1));
            dot(*renderer, query, 6.0f, rgba(1, 1, 1, 1));
            font.drawText(*renderer, 888.0f, 96.0f, "closest position on nearest edge (snap)",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ASTAR shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
