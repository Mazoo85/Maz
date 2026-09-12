// Maz Engine — "GUARD" (finite-state-machine AI demo)
// Guards walk a patrol loop until an intruder wanders close, then a maz::game::StateMachine flips
// them Patrol -> Chase (steer toward the intruder); if it escapes they go Return (head back to the
// nearest patrol post) and resume patrolling. State drives their color: green=patrol, red=chase,
// amber=return. Composes this loop's FSM with the earlier steering layer. Run --headless/--frames N.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace maz;

namespace {

enum class GuardState { Patrol, Chase, Return };

float distXZ(const math::vec3& a, const math::vec3& b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

struct Guard {
    game::Agent agent;
    game::StateMachine<GuardState> fsm;
    uint32_t wp = 0;
    math::vec3 home{0.0f};
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GUARD (FSM AI demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — State Machines";
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

    // Meshes.
    auto guardData = render::shapes::makeSphere(0.7f, 16, 22, render::Color{1, 1, 1, 1});
    render::MeshHandle guardMesh = renderer->createMesh(
        guardData.vertices.data(), static_cast<uint32_t>(guardData.vertices.size()),
        guardData.indices.data(), static_cast<uint32_t>(guardData.indices.size()));
    auto intruderData = render::shapes::makeSphere(0.9f, 16, 22, render::Color{1, 1, 1, 1});
    render::MeshHandle intruderMesh = renderer->createMesh(
        intruderData.vertices.data(), static_cast<uint32_t>(intruderData.vertices.size()),
        intruderData.indices.data(), static_cast<uint32_t>(intruderData.indices.size()));
    auto postData = render::shapes::makeBox(0.6f, render::Color{1, 1, 1, 1});
    render::MeshHandle post = renderer->createMesh(
        postData.vertices.data(), static_cast<uint32_t>(postData.vertices.size()),
        postData.indices.data(), static_cast<uint32_t>(postData.indices.size()));
    auto floorData = render::shapes::makePlane(18.0f, render::Color{0.28f, 0.31f, 0.35f, 1});
    render::MeshHandle floor = renderer->createMesh(
        floorData.vertices.data(), static_cast<uint32_t>(floorData.vertices.size()),
        floorData.indices.data(), static_cast<uint32_t>(floorData.indices.size()));
    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);
    auto colorTex = [&](float r, float g, float b) {
        const uint8_t px[4] = {static_cast<uint8_t>(r * 255), static_cast<uint8_t>(g * 255),
                               static_cast<uint8_t>(b * 255), 255};
        return renderer->createTexture(1, 1, px);
    };
    const render::TextureHandle texPatrol = colorTex(0.2f, 0.75f, 0.35f); // green
    const render::TextureHandle texChase = colorTex(0.9f, 0.25f, 0.25f);  // red
    const render::TextureHandle texReturn = colorTex(0.95f, 0.7f, 0.2f);  // amber
    const render::TextureHandle texIntruder = colorTex(0.85f, 0.3f, 0.9f); // magenta

    // Patrol loop (a rectangle of posts) the guards cycle around.
    std::vector<math::vec3> patrol = {
        {-6.0f, 0.9f, -6.0f}, {6.0f, 0.9f, -6.0f}, {6.0f, 0.9f, 6.0f}, {-6.0f, 0.9f, 6.0f}};

    // Tuning.
    const float chaseR = 5.0f;  // start chasing within this distance of the intruder
    const float loseR = 9.5f;   // give up beyond this distance
    const float arriveR = 1.2f; // "reached a post" radius

    // Intruder state (a scripted wanderer). Captured by reference in the guards' FSM guards.
    math::vec3 intruder{0.0f, 0.9f, 0.0f};

    // Build guards, each with its own FSM. unique_ptr keeps addresses stable for the lambda captures.
    std::vector<std::unique_ptr<Guard>> guards;
    const int kGuards = 4;
    for (int i = 0; i < kGuards; ++i) {
        auto gp = std::make_unique<Guard>();
        Guard* g = gp.get();
        // i is int and patrol.size() is size_t; widen i explicitly rather than let the modulo
        // convert it. i is 0..kGuards-1, so the value is unchanged.
        g->wp = static_cast<uint32_t>(static_cast<size_t>(i) % patrol.size());
        g->agent.pos = patrol[g->wp];
        g->agent.maxSpeed = 5.0f + static_cast<float>(i % 2);
        g->agent.maxForce = 26.0f;
        g->home = patrol[g->wp];

        g->fsm.addState(GuardState::Patrol, [g, &patrol, arriveR](float dt) {
            const math::vec3 target = patrol[g->wp];
            if (distXZ(g->agent.pos, target) < arriveR) {
                g->wp = (g->wp + 1) % static_cast<uint32_t>(patrol.size());
            }
            math::vec3 f = game::arrive(g->agent, patrol[g->wp], 2.0f);
            f.y = 0.0f;
            game::integrate(g->agent, f, dt);
            g->agent.pos.y = 0.9f;
        });
        g->fsm.addState(GuardState::Chase, [g, &intruder](float dt) {
            math::vec3 f = game::seek(g->agent, intruder);
            f.y = 0.0f;
            game::integrate(g->agent, f, dt);
            g->agent.pos.y = 0.9f;
        });
        g->fsm.addState(
            GuardState::Return,
            [g](float dt) { // arriveR was captured and never used
                math::vec3 f = game::arrive(g->agent, g->home, 2.0f);
                f.y = 0.0f;
                game::integrate(g->agent, f, dt);
                g->agent.pos.y = 0.9f;
            },
            [g, &patrol]() {
                // On entering Return, pick the nearest patrol post as the destination.
                uint32_t best = 0;
                float bestD = 1e30f;
                for (uint32_t k = 0; k < patrol.size(); ++k) {
                    const float d = distXZ(g->agent.pos, patrol[k]);
                    if (d < bestD) {
                        bestD = d;
                        best = k;
                    }
                }
                g->wp = best;
                g->home = patrol[best];
            });

        g->fsm.addTransition(GuardState::Patrol, GuardState::Chase,
                             [g, &intruder, chaseR]() { return distXZ(g->agent.pos, intruder) < chaseR; });
        g->fsm.addTransition(GuardState::Chase, GuardState::Return,
                             [g, &intruder, loseR]() { return distXZ(g->agent.pos, intruder) > loseR; });
        g->fsm.addTransition(GuardState::Return, GuardState::Patrol,
                             [g, arriveR]() { return distXZ(g->agent.pos, g->home) < arriveR; });
        // If the intruder comes close again while returning, chase immediately (any-state guard).
        g->fsm.addTransition(GuardState::Return, GuardState::Chase,
                             [g, &intruder, chaseR]() { return distXZ(g->agent.pos, intruder) < chaseR; });
        g->fsm.start(GuardState::Patrol);
        guards.push_back(std::move(gp));
    }

    render::SceneLighting lighting;
    lighting.ambient[0] = lighting.ambient[1] = lighting.ambient[2] = 0.5f;
    renderer->setLighting(lighting);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

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
            // Intruder weaves a figure-eight that drifts through the patrol area.
            intruder.x = std::sin(t * 0.6f) * 8.0f;
            intruder.z = std::sin(t * 0.9f) * 7.0f;
            for (auto& g : guards) {
                g->fsm.update(dt);
            }
        }

        int chasing = 0;
        for (auto& g : guards) {
            if (g->fsm.isIn(GuardState::Chase)) {
                ++chasing;
            }
        }

        const glm::vec3 eye(std::sin(t * 0.12f) * 20.0f, 22.0f, std::cos(t * 0.12f) * 20.0f);
        const glm::mat4 proj = math::perspective(glm::radians(52.0f), aspect, 0.1f, 120.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.13f, 0.15f, 0.19f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            glm::mat4 fm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.25f, 0.0f));
            fm = glm::scale(fm, glm::vec3(1.0f, 0.5f, 1.0f));
            renderer->drawMesh(floor, glm::value_ptr(fm), white);

            // Patrol posts.
            for (const math::vec3& wpp : patrol) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(wpp.x, 0.3f, wpp.z));
                const float e[3] = {0.15f, 0.2f, 0.28f};
                renderer->drawMeshEmissive(post, glm::value_ptr(m), white, render::kInvalidTexture, e);
            }

            // Intruder (magenta, with a bit of emissive glow so it stands out).
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(intruder.x, intruder.y, intruder.z));
                const float e[3] = {0.35f, 0.05f, 0.35f};
                renderer->drawMeshEmissive(intruderMesh, glm::value_ptr(m), texIntruder,
                                           render::kInvalidTexture, e);
            }

            // Guards, colored by state via a per-state albedo texture.
            for (auto& g : guards) {
                render::TextureHandle tex = texPatrol;
                switch (g->fsm.current()) {
                    case GuardState::Patrol: tex = texPatrol; break;
                    case GuardState::Chase: tex = texChase; break;
                    case GuardState::Return: tex = texReturn; break;
                }
                glm::mat4 m = glm::translate(
                    glm::mat4(1.0f), glm::vec3(g->agent.pos.x, g->agent.pos.y, g->agent.pos.z));
                renderer->drawMesh(guardMesh, glm::value_ptr(m), tex);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  STATE-MACHINE AI",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%d guards  |  patrol/chase/return FSM  |  %d chasing",
                          kGuards, chasing);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.8f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GUARD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
