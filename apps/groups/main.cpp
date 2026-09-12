// Maz Engine — "GROUPS" (node groups, toward Godot's SceneTree add_to_group / get_nodes_in_group)
// A grid of 36 nodes, each tagged into named groups (enemies / pickups / scenery, plus cross-cutting vip
// and hazard tags). The scene is drawn coloured by each node's primary group; then the group system is
// queried live: scene::GroupRegistry::nodesInGroup("vip") rings the VIPs, and call("hazard", ...) stamps a
// warning on every hazard node — without the app keeping its own lists. A footer reports each group's size
// via groupSize(). Fixed layout -> deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void quad(render::Renderer& r, float cx, float cy, float hx, float hy, render::Color c) {
    const render::Point2 p[4] = {
        {cx - hx, cy - hy}, {cx + hx, cy - hy}, {cx + hx, cy + hy}, {cx - hx, cy + hy}};
    r.drawConvexPolygon(p, 4, c);
}

render::Color rgb(float r, float g, float b) { return render::Color{r, g, b, 1.0f}; }

struct Cell {
    scene::GroupNode id;
    float x, y;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GROUPS (node groups) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Node Groups";
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

    // ---- Build the scene: a 6x6 grid of nodes, tagged into groups. -----------------------------------
    const int cols = 6, rows = 6;
    const float x0 = 150.0f, y0 = 170.0f, dx = 165.0f, dy = 78.0f;
    std::vector<Cell> cells;
    scene::GroupRegistry groups;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const scene::GroupNode id = static_cast<scene::GroupNode>(r * cols + c);
            cells.push_back({id, x0 + static_cast<float>(c) * dx, y0 + static_cast<float>(r) * dy});
            // Primary species by index mod 3.
            const int prim = static_cast<int>(id) % 3;
            groups.add(id, prim == 0 ? "enemies" : (prim == 1 ? "pickups" : "scenery"));
            // Cross-cutting tags.
            if (static_cast<int>(id) % 7 == 0) {
                groups.add(id, "vip");
            }
            if (r + c == cols - 1) { // anti-diagonal, distinct from the vip set
                groups.add(id, "hazard");
            }
        }
    }

    const render::Color kEnemy = rgb(0.90f, 0.42f, 0.40f);
    const render::Color kPickup = rgb(0.95f, 0.80f, 0.36f);
    const render::Color kScenery = rgb(0.46f, 0.60f, 0.72f);
    const render::Color kVip = rgb(1.0f, 0.95f, 0.5f);
    const render::Color kHazard = rgb(1.0f, 0.35f, 0.30f);

    // Precompute the highlight sets from the group system (as the demo's "queries").
    const std::vector<scene::GroupNode> vips = groups.nodesInGroup("vip");
    std::array<bool, 36> isVip{};
    for (scene::GroupNode v : vips) {
        if (v < isVip.size()) {
            isVip[v] = true;
        }
    }
    std::array<bool, 36> isHazard{};
    groups.call("hazard", [&](scene::GroupNode n) {
        if (n < isHazard.size()) {
            isHazard[n] = true;
        }
    });

    auto primColor = [&](scene::GroupNode id) {
        const int prim = static_cast<int>(id) % 3;
        return prim == 0 ? kEnemy : (prim == 1 ? kPickup : kScenery);
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  NODE GROUPS", rgb(1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "36 nodes tagged into groups; the group system answers the queries — no per-app "
                          "lists (scene::GroupRegistry)",
                          rgb(0.8f, 0.86f, 0.95f), 0.32f);

            const float hx = 66.0f, hy = 30.0f;
            for (const Cell& cell : cells) {
                // VIP ring: a bright inset border drawn behind the tile (from nodesInGroup("vip")).
                if (isVip[cell.id]) {
                    quad(*renderer, cell.x, cell.y, hx + 6.0f, hy + 6.0f, kVip);
                }
                quad(*renderer, cell.x, cell.y, hx, hy, primColor(cell.id));

                // Hazard mark: a warning triangle stamped by call("hazard", ...).
                if (isHazard[cell.id]) {
                    const render::Point2 tri[3] = {{cell.x, cell.y - 18.0f},
                                                   {cell.x - 15.0f, cell.y + 12.0f},
                                                   {cell.x + 15.0f, cell.y + 12.0f}};
                    renderer->drawConvexPolygon(tri, 3, kHazard);
                }

                const std::string label = std::to_string(cell.id);
                font.drawText(*renderer, cell.x - 8.0f, cell.y - 12.0f, label.c_str(),
                              rgb(0.12f, 0.13f, 0.16f), 0.34f);
            }

            // Footer: group sizes straight from the registry.
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "enemies %zu   pickups %zu   scenery %zu   vip %zu   hazard %zu   (groups: %zu)",
                          groups.groupSize("enemies"), groups.groupSize("pickups"),
                          groups.groupSize("scenery"), groups.groupSize("vip"),
                          groups.groupSize("hazard"), groups.groupCount());
            font.drawText(*renderer, 24.0f, 664.0f, buf, rgb(0.7f, 0.76f, 0.86f), 0.34f);
            font.drawText(*renderer, 24.0f, 634.0f,
                          "yellow border = nodesInGroup(\"vip\");  red triangle = call(\"hazard\", ...)",
                          rgb(0.6f, 0.64f, 0.72f), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GROUPS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
