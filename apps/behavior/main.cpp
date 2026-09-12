// Maz Engine — "BEHAVIOR" (behavior-tree AI demo)
// Each agent's decisions come from a maz::game::bt behavior tree, not a hand-written state machine:
//   selector( sequence(intruder VERY close?  -> flee),
//             sequence(intruder close?       -> chase),
//             patrol )
// Because the selector is reactive (re-evaluated every tick), agents pre-empt patrol to chase, and
// chase to flee, the instant the intruder crosses a range — then fall back as it recedes. Colour
// shows the active leaf: green=patrol, red=chase, yellow=flee. Composes M71 trees with M58 steering.
// Run --headless / --frames N for CI.

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

float distXZ(const math::vec3& a, const math::vec3& b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

enum Behavior { Patrol = 0, Chase = 1, Flee = 2 };

struct Agent {
    game::Agent body;
    game::bt::BehaviorTree tree;
    uint32_t wp = 0;
    int behavior = Patrol;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BEHAVIOR (behavior-tree AI demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Behavior Trees";
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

    auto sphere = render::shapes::makeSphere(0.7f, 16, 22, render::Color{1, 1, 1, 1});
    render::MeshHandle agentMesh = renderer->createMesh(
        sphere.vertices.data(), static_cast<uint32_t>(sphere.vertices.size()), sphere.indices.data(),
        static_cast<uint32_t>(sphere.indices.size()));
    auto isph = render::shapes::makeSphere(0.9f, 16, 22, render::Color{1, 1, 1, 1});
    render::MeshHandle intruderMesh = renderer->createMesh(
        isph.vertices.data(), static_cast<uint32_t>(isph.vertices.size()), isph.indices.data(),
        static_cast<uint32_t>(isph.indices.size()));
    auto postD = render::shapes::makeBox(0.6f, render::Color{1, 1, 1, 1});
    render::MeshHandle post = renderer->createMesh(postD.vertices.data(),
                                                   static_cast<uint32_t>(postD.vertices.size()),
                                                   postD.indices.data(),
                                                   static_cast<uint32_t>(postD.indices.size()));
    auto floorD = render::shapes::makePlane(18.0f, render::Color{0.28f, 0.31f, 0.35f, 1});
    render::MeshHandle floor = renderer->createMesh(floorD.vertices.data(),
                                                    static_cast<uint32_t>(floorD.vertices.size()),
                                                    floorD.indices.data(),
                                                    static_cast<uint32_t>(floorD.indices.size()));
    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);
    auto colorTex = [&](float r, float g, float b) {
        const uint8_t px[4] = {static_cast<uint8_t>(r * 255), static_cast<uint8_t>(g * 255),
                               static_cast<uint8_t>(b * 255), 255};
        return renderer->createTexture(1, 1, px);
    };
    const render::TextureHandle texPatrol = colorTex(0.2f, 0.75f, 0.35f);
    const render::TextureHandle texChase = colorTex(0.9f, 0.25f, 0.25f);
    const render::TextureHandle texFlee = colorTex(0.95f, 0.85f, 0.2f);
    const render::TextureHandle texIntruder = colorTex(0.85f, 0.3f, 0.9f);

    std::vector<math::vec3> patrol = {
        {-6.0f, 0.9f, -6.0f}, {6.0f, 0.9f, -6.0f}, {6.0f, 0.9f, 6.0f}, {-6.0f, 0.9f, 6.0f}};
    const float fleeR = 3.0f, chaseR = 6.5f, arriveR = 1.2f;
    math::vec3 intruder{0, 0.9f, 0};
    float frameDt = 1.0f / 60.0f;

    // Build agents, each with its own reactive behavior tree.
    std::vector<std::unique_ptr<Agent>> agents;
    const int kAgents = 5;
    for (int i = 0; i < kAgents; ++i) {
        auto ap = std::make_unique<Agent>();
        Agent* a = ap.get();
        // i is int, patrol.size() is size_t: widen before the modulo rather than letting it
        // convert. i is 0..count-1, so the value is unchanged.
        a->wp = static_cast<uint32_t>(static_cast<size_t>(i) % patrol.size());
        a->body.pos = patrol[a->wp];
        a->body.maxSpeed = 5.5f + static_cast<float>(i % 2);
        a->body.maxForce = 28.0f;

        namespace bt = game::bt;
        a->tree.setRoot(bt::selector(
            // Flee when the intruder is very close.
            bt::sequence(
                bt::condition([a, &intruder, fleeR] { return distXZ(a->body.pos, intruder) < fleeR; }),
                bt::action([a, &intruder, &frameDt] {
                    math::vec3 f = game::flee(a->body, intruder);
                    f.y = 0.0f;
                    game::integrate(a->body, f, frameDt);
                    a->body.pos.y = 0.9f;
                    a->behavior = Flee;
                    return bt::Status::Running;
                })),
            // Otherwise chase when it's within range.
            bt::sequence(
                bt::condition([a, &intruder, chaseR] { return distXZ(a->body.pos, intruder) < chaseR; }),
                bt::action([a, &intruder, &frameDt] {
                    math::vec3 f = game::seek(a->body, intruder);
                    f.y = 0.0f;
                    game::integrate(a->body, f, frameDt);
                    a->body.pos.y = 0.9f;
                    a->behavior = Chase;
                    return bt::Status::Running;
                })),
            // Fallback: patrol the posts.
            bt::action([a, &patrol, &frameDt, arriveR] {
                if (distXZ(a->body.pos, patrol[a->wp]) < arriveR) {
                    a->wp = (a->wp + 1) % static_cast<uint32_t>(patrol.size());
                }
                math::vec3 f = game::arrive(a->body, patrol[a->wp], 2.0f);
                f.y = 0.0f;
                game::integrate(a->body, f, frameDt);
                a->body.pos.y = 0.9f;
                a->behavior = Patrol;
                return bt::Status::Running;
            })));
        agents.push_back(std::move(ap));
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
            frameDt = static_cast<float>(clock.fixedDelta());
            t += frameDt;
            intruder.x = std::sin(t * 0.6f) * 8.0f;
            intruder.z = std::sin(t * 0.9f) * 7.0f;
            for (auto& a : agents) {
                a->tree.tick(); // one decision + action per agent per step
            }
        }

        int fleeing = 0, chasing = 0;
        for (auto& a : agents) {
            if (a->behavior == Flee) ++fleeing;
            else if (a->behavior == Chase) ++chasing;
        }

        const glm::vec3 eye(std::sin(t * 0.12f) * 20.0f, 22.0f, std::cos(t * 0.12f) * 20.0f);
        const glm::mat4 proj = math::perspective(glm::radians(52.0f), aspect, 0.1f, 120.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.13f, 0.15f, 0.18f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            glm::mat4 fm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.25f, 0.0f));
            fm = glm::scale(fm, glm::vec3(1.0f, 0.5f, 1.0f));
            renderer->drawMesh(floor, glm::value_ptr(fm), white);

            for (const math::vec3& p : patrol) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(p.x, 0.3f, p.z));
                const float e[3] = {0.15f, 0.2f, 0.28f};
                renderer->drawMeshEmissive(post, glm::value_ptr(m), white, render::kInvalidTexture, e);
            }
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(intruder.x, intruder.y, intruder.z));
                const float e[3] = {0.35f, 0.05f, 0.35f};
                renderer->drawMeshEmissive(intruderMesh, glm::value_ptr(m), texIntruder,
                                           render::kInvalidTexture, e);
            }
            for (auto& a : agents) {
                render::TextureHandle tex = a->behavior == Flee    ? texFlee
                                            : a->behavior == Chase ? texChase
                                                                   : texPatrol;
                glm::mat4 m = glm::translate(
                    glm::mat4(1.0f), glm::vec3(a->body.pos.x, a->body.pos.y, a->body.pos.z));
                renderer->drawMesh(agentMesh, glm::value_ptr(m), tex);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  BEHAVIOR-TREE AI",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%d agents  |  flee/chase/patrol tree  |  %d fleeing  %d chasing",
                          kAgents, fleeing, chasing);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.8f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BEHAVIOR shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
