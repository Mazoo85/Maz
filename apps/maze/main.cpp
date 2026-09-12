// Maz Engine — "MAZE" (A* pathfinding demo)
// A walker crosses an obstacle field, re-planning its route with maz::game::NavGrid (grid A*)
// whenever it reaches its goal. The computed path is drawn as a debug polyline; the walker follows
// the waypoints at constant speed. Proves the engine has reusable navigation for moving AI. Run
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MAZE (A* pathfinding demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Pathfinding";
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

    // --- Grid + world layout ------------------------------------------------------------------
    constexpr int kW = 20, kH = 20;
    constexpr float kCell = 2.0f;
    const math::vec3 gridOrigin(-static_cast<float>(kW) * kCell * 0.5f, 0.0f,
                                -static_cast<float>(kH) * kCell * 0.5f);
    game::NavGrid grid(kW, kH, kCell, gridOrigin);

    // A fixed set of wall runs (in cell coords) that leave a solvable but winding route.
    struct Wall {
        int x0, z0, x1, z1;
    };
    const Wall walls[] = {
        {4, 2, 4, 14},  {8, 5, 8, 17}, {12, 2, 12, 14}, {16, 5, 16, 17},
        {8, 5, 11, 5},  {4, 14, 7, 14}, {12, 14, 15, 14},
    };
    std::vector<std::pair<int, int>> blockedCells;
    for (const Wall& w : walls) {
        const int sx = w.x0 < w.x1 ? 1 : -1;
        const int sz = w.z0 < w.z1 ? 1 : -1;
        int x = w.x0, z = w.z0;
        while (true) {
            grid.setBlocked(x, z, true);
            blockedCells.emplace_back(x, z);
            if (x == w.x1 && z == w.z1) break;
            if (x != w.x1) x += sx;
            if (z != w.z1) z += sz;
        }
    }

    // --- Meshes -------------------------------------------------------------------------------
    auto floorData = render::shapes::makePlane(static_cast<float>(kW) * kCell * 0.5f,
                                               render::Color{0.32f, 0.35f, 0.38f, 1.0f});
    render::MeshHandle floor = renderer->createMesh(floorData.vertices.data(),
                                                    static_cast<uint32_t>(floorData.vertices.size()),
                                                    floorData.indices.data(),
                                                    static_cast<uint32_t>(floorData.indices.size()));
    auto wallData = render::shapes::makeBox(1.0f, render::Color{0.55f, 0.5f, 0.62f, 1.0f});
    render::MeshHandle wallCube = renderer->createMesh(
        wallData.vertices.data(), static_cast<uint32_t>(wallData.vertices.size()),
        wallData.indices.data(), static_cast<uint32_t>(wallData.indices.size()));
    auto agentData = render::shapes::makeSphere(0.7f, 16, 24, render::Color{1.0f, 0.75f, 0.2f, 1.0f});
    render::MeshHandle agent = renderer->createMesh(
        agentData.vertices.data(), static_cast<uint32_t>(agentData.vertices.size()),
        agentData.indices.data(), static_cast<uint32_t>(agentData.indices.size()));
    auto goalData = render::shapes::makeBox(1.0f, render::Color{0.3f, 1.0f, 0.45f, 1.0f});
    render::MeshHandle goalCube = renderer->createMesh(
        goalData.vertices.data(), static_cast<uint32_t>(goalData.vertices.size()),
        goalData.indices.data(), static_cast<uint32_t>(goalData.indices.size()));

    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    render::SceneLighting lighting;
    lighting.ambient[0] = lighting.ambient[1] = lighting.ambient[2] = 0.4f;
    lighting.sunDir[0] = 0.5f;
    lighting.sunDir[1] = 0.9f;
    lighting.sunDir[2] = 0.4f;
    renderer->setLighting(lighting);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // --- Path planning state ------------------------------------------------------------------
    // A ring of goal cells the agent visits in turn; each leg is re-planned with A*.
    const game::NavGrid::Cell goals[] = {{1, 1}, {18, 1}, {18, 18}, {1, 18}};
    int goalIdx = 0;
    game::NavGrid::Cell startCell{1, 1};
    std::vector<math::vec3> path;
    grid.findWorldPath(grid.cellToWorld(startCell), grid.cellToWorld(goals[goalIdx]), path);
    size_t leg = 0;
    math::vec3 agentPos = path.empty() ? grid.cellToWorld(startCell) : path.front();
    const float kSpeed = 7.0f;

    float t = 0.0f;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            t += dt;
            // Advance the agent toward the next waypoint; re-plan to the next goal on arrival.
            if (leg + 1 < path.size()) {
                const math::vec3 target = path[leg + 1];
                math::vec3 to = target - agentPos;
                to.y = 0.0f;
                const float dist = std::sqrt(to.x * to.x + to.z * to.z);
                const float stepLen = kSpeed * dt;
                if (dist <= stepLen || dist < 1e-4f) {
                    agentPos = target;
                    ++leg;
                } else {
                    agentPos += to * (stepLen / dist);
                }
            } else {
                // Reached the goal: pick the next one and re-plan from the current cell.
                startCell = grid.worldToCell(agentPos);
                goalIdx = (goalIdx + 1) % 4;
                if (grid.findWorldPath(grid.cellToWorld(startCell), grid.cellToWorld(goals[goalIdx]),
                                       path)) {
                    leg = 0;
                    if (!path.empty()) agentPos = path.front();
                }
            }
        }

        // Slow overhead orbit so the whole maze and the route stay visible.
        const glm::vec3 eye(std::sin(t * 0.1f) * 26.0f, 34.0f, std::cos(t * 0.1f) * 26.0f + 6.0f);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 200.0f);
        const glm::mat4 view =
            glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.15f, 0.17f, 0.22f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            renderer->drawMesh(floor, glm::value_ptr(glm::mat4(1.0f)), white);

            // Walls.
            for (const auto& bc : blockedCells) {
                const math::vec3 c = grid.cellToWorld({bc.first, bc.second});
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(c.x, 1.0f, c.z));
                m = glm::scale(m, glm::vec3(kCell, 2.0f, kCell));
                renderer->drawMesh(wallCube, glm::value_ptr(m), white);
            }

            // Goal marker.
            {
                const math::vec3 g = grid.cellToWorld(goals[goalIdx]);
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(g.x, 0.8f, g.z));
                m = glm::scale(m, glm::vec3(1.2f, 1.6f, 1.2f));
                const float e[3] = {0.1f, 0.6f, 0.2f};
                renderer->drawMeshEmissive(goalCube, glm::value_ptr(m), white, render::kInvalidTexture, e);
            }

            // The planned path, as an elevated debug polyline.
            const float pathCol[4] = {1.0f, 0.85f, 0.2f, 1.0f};
            for (size_t i = 0; i + 1 < path.size(); ++i) {
                const math::vec3 a(path[i].x, 0.6f, path[i].z);
                const math::vec3 b(path[i + 1].x, 0.6f, path[i + 1].z);
                renderer->drawLine(glm::value_ptr(a), glm::value_ptr(b), pathCol);
            }

            // The agent.
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(agentPos.x, 0.9f, agentPos.z));
                const float e[3] = {0.5f, 0.35f, 0.05f};
                renderer->drawMeshEmissive(agent, glm::value_ptr(m), white, render::kInvalidTexture, e);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  A* PATHFINDING",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "%zu-waypoint route re-planned per goal",
                          path.size());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{1.0f, 0.9f, 0.5f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MAZE shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
