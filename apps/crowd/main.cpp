// Maz Engine — "CROWD" (steering + pathfinding demo)
// A flock of agents navigates a shared maze toward a roving goal. Each agent plans its own A* route
// (maz::game::NavGrid), follows the waypoints with path-following steering, and pushes off its
// neighbors with a separation force (maz::game::Steering) so the group flows as a stream instead of
// stacking into one point or clipping through walls. Combines this loop's steering with the previous
// loop's navigation. Run --headless / --frames N for CI.

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
    MAZ_LOG_INFO("CROWD (steering + pathfinding demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Steering";
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

    // --- Grid + walls (same obstacle field as the maze demo) ----------------------------------
    constexpr int kW = 20, kH = 20;
    constexpr float kCell = 2.0f;
    const math::vec3 gridOrigin(-static_cast<float>(kW) * kCell * 0.5f, 0.0f,
                                -static_cast<float>(kH) * kCell * 0.5f);
    game::NavGrid grid(kW, kH, kCell, gridOrigin);
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
                                               render::Color{0.30f, 0.33f, 0.36f, 1.0f});
    render::MeshHandle floor = renderer->createMesh(
        floorData.vertices.data(), static_cast<uint32_t>(floorData.vertices.size()),
        floorData.indices.data(), static_cast<uint32_t>(floorData.indices.size()));
    auto wallData = render::shapes::makeBox(1.0f, render::Color{0.52f, 0.48f, 0.6f, 1.0f});
    render::MeshHandle wallCube = renderer->createMesh(
        wallData.vertices.data(), static_cast<uint32_t>(wallData.vertices.size()),
        wallData.indices.data(), static_cast<uint32_t>(wallData.indices.size()));
    auto agentData = render::shapes::makeSphere(0.6f, 14, 20, render::Color{1, 1, 1, 1});
    render::MeshHandle agentMesh = renderer->createMesh(
        agentData.vertices.data(), static_cast<uint32_t>(agentData.vertices.size()),
        agentData.indices.data(), static_cast<uint32_t>(agentData.indices.size()));
    auto goalData = render::shapes::makeBox(1.0f, render::Color{0.3f, 1.0f, 0.45f, 1.0f});
    render::MeshHandle goalCube = renderer->createMesh(
        goalData.vertices.data(), static_cast<uint32_t>(goalData.vertices.size()),
        goalData.indices.data(), static_cast<uint32_t>(goalData.indices.size()));

    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    // Per-agent colored textures so the flock reads as distinct individuals.
    auto colorTex = [&](float r, float g, float b) {
        const uint8_t px[4] = {static_cast<uint8_t>(r * 255), static_cast<uint8_t>(g * 255),
                               static_cast<uint8_t>(b * 255), 255};
        return renderer->createTexture(1, 1, px);
    };

    render::SceneLighting lighting;
    lighting.ambient[0] = lighting.ambient[1] = lighting.ambient[2] = 0.42f;
    renderer->setLighting(lighting);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // --- Agents ------------------------------------------------------------------------------
    struct Walker {
        game::Agent agent;
        std::vector<math::vec3> path;
        uint32_t wp = 0;
        render::TextureHandle tex;
    };
    const game::NavGrid::Cell goals[] = {{18, 18}, {1, 18}, {18, 1}, {1, 1}};
    int goalIdx = 0;

    std::vector<Walker> walkers;
    constexpr int kCount = 14;
    for (int i = 0; i < kCount; ++i) {
        Walker w;
        // Spawn spread around the (1,1) corner so they don't all start on one cell.
        const game::NavGrid::Cell startCell{1 + (i % 3), 1 + (i / 3)};
        w.agent.pos = grid.cellToWorld(startCell);
        w.agent.pos.y = 0.9f;
        w.agent.maxSpeed = 6.0f + static_cast<float>(i % 3);
        w.agent.maxForce = 30.0f;
        grid.findWorldPath(w.agent.pos, grid.cellToWorld(goals[goalIdx]), w.path);
        const float hue = static_cast<float>(i) / kCount;
        w.tex = colorTex(0.5f + 0.5f * std::sin(hue * 6.28f), 0.5f + 0.4f * std::sin(hue * 6.28f + 2.0f),
                         0.6f + 0.4f * std::cos(hue * 6.28f));
        walkers.push_back(std::move(w));
    }

    float goalTimer = 0.0f;
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
            goalTimer += dt;

            // Every few seconds the goal hops to the next corner and everyone re-plans.
            if (goalTimer > 7.0f) {
                goalTimer = 0.0f;
                goalIdx = (goalIdx + 1) % 4;
                for (Walker& w : walkers) {
                    if (grid.findWorldPath(w.agent.pos, grid.cellToWorld(goals[goalIdx]), w.path)) {
                        w.wp = 0;
                    }
                }
            }

            // Gather positions once for the separation query.
            std::vector<math::vec3> positions;
            positions.reserve(walkers.size());
            for (const Walker& w : walkers) {
                positions.push_back(w.agent.pos);
            }

            for (Walker& w : walkers) {
                math::vec3 force =
                    game::followPath(w.agent, w.path, w.wp, 1.2f, 2.0f);      // stay on route
                force += game::separation(w.agent, positions, 2.2f) * 1.4f;   // don't crowd
                force.y = 0.0f;                                              // stay on the ground
                game::integrate(w.agent, force, dt);
                w.agent.pos.y = 0.9f;
            }
        }

        const glm::vec3 eye(std::sin(t * 0.1f) * 24.0f, 32.0f, std::cos(t * 0.1f) * 24.0f + 6.0f);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 200.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.14f, 0.16f, 0.21f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            renderer->drawMesh(floor, glm::value_ptr(glm::mat4(1.0f)), white);
            for (const auto& bc : blockedCells) {
                const math::vec3 c = grid.cellToWorld({bc.first, bc.second});
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(c.x, 1.0f, c.z));
                m = glm::scale(m, glm::vec3(kCell, 2.0f, kCell));
                renderer->drawMesh(wallCube, glm::value_ptr(m), white);
            }
            {
                const math::vec3 g = grid.cellToWorld(goals[goalIdx]);
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(g.x, 0.8f, g.z));
                m = glm::scale(m, glm::vec3(1.3f, 1.7f, 1.3f));
                const float e[3] = {0.1f, 0.6f, 0.2f};
                renderer->drawMeshEmissive(goalCube, glm::value_ptr(m), white, render::kInvalidTexture, e);
            }
            for (const Walker& w : walkers) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(w.agent.pos.x, w.agent.pos.y, w.agent.pos.z));
                renderer->drawMesh(agentMesh, glm::value_ptr(m), w.tex);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  STEERING + PATHFINDING",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "%d agents: A* route + path-follow + separation",
                          kCount);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CROWD shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
