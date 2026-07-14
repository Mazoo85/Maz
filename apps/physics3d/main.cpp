// Maz Engine — "PHYSICS3D" (3D rigid-body dynamics, toward Godot RigidBody3D)
// The 3D physics deep-dive, sibling of the 2D solver. game::PhysicsWorld3D simulates spheres and
// oriented boxes under gravity with full angular dynamics (inertia tensors + quaternion spin) and
// resolves sphere-sphere, sphere-box and box/sphere-vs-ground-plane contacts with rotation-aware
// impulses (restitution + friction + positional correction). This demo drops tilted boxes that tumble
// and settle flat on the ground, plus a cluster of balls that fall into a heap — some landing on top of
// the boxes. The sim is run up front on a fixed timestep (deterministic) and the settled scene drawn
// statically, so the captured frame is golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::MeshHandle upload(render::Renderer& r, const render::shapes::MeshData& m) {
    return r.createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()), m.indices.data(),
                        static_cast<uint32_t>(m.indices.size()));
}

render::Color palette(std::size_t i) {
    const render::Color p[6] = {{0.93f, 0.36f, 0.36f, 1}, {0.42f, 0.80f, 0.45f, 1},
                                {0.40f, 0.60f, 0.95f, 1}, {0.95f, 0.78f, 0.30f, 1},
                                {0.80f, 0.48f, 0.92f, 1}, {0.35f, 0.86f, 0.86f, 1}};
    return p[i % 6];
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PHYSICS3D (3D rigid-body dynamics) starting");

    // --- Simulate the scene up front (deterministic, no renderer needed) ---------------------------
    game::PhysicsWorld3D world;
    world.gravity = math::vec3(0.0f, -9.81f, 0.0f);
    world.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));

    // Three boxes, well separated (box-vs-box lands in a later milestone), each dropped tilted so they
    // tumble and settle flat on the ground.
    struct BoxSpawn {
        float x, y, z, h;
        math::vec3 tilt;
    };
    const BoxSpawn boxSpawns[] = {
        {-3.2f, 3.0f, -0.2f, 0.6f, math::vec3(0.5f, 0.2f, 0.3f)},
        {0.0f, 4.2f, 0.3f, 0.7f, math::vec3(0.2f, 0.4f, 0.6f)},
        {3.2f, 3.4f, -0.3f, 0.55f, math::vec3(0.6f, 0.1f, 0.2f)},
    };
    std::vector<int> boxIds;
    for (const BoxSpawn& s : boxSpawns) {
        game::Body3D b = game::makeBox(math::vec3(s.x, s.y, s.z), math::vec3(s.h, s.h, s.h), 1.0f);
        b.orientation = glm::normalize(math::quat(s.tilt));
        b.restitution = 0.1f;
        b.friction = 0.7f;
        b.enableRotation();
        boxIds.push_back(world.add(b));
    }

    // A cluster of balls dropped down the middle; they pile up and some land on the boxes.
    struct BallSpawn {
        float x, y, z, r;
    };
    const BallSpawn ballSpawns[] = {
        {-3.1f, 5.0f, -0.1f, 0.4f},  {0.1f, 6.4f, 0.2f, 0.45f},  {3.1f, 5.4f, -0.2f, 0.4f},
        {-0.6f, 7.6f, -0.4f, 0.42f}, {0.6f, 8.8f, 0.35f, 0.5f},  {-1.5f, 10.0f, 0.1f, 0.44f},
        {1.4f, 11.2f, -0.2f, 0.46f}, {0.0f, 12.4f, 0.4f, 0.48f}, {-0.3f, 13.6f, -0.3f, 0.43f},
    };
    std::vector<int> ballIds;
    for (const BallSpawn& s : ballSpawns) {
        game::Body3D b = game::makeSphere(math::vec3(s.x, s.y, s.z), s.r, 1.0f);
        b.restitution = 0.2f;
        b.friction = 0.6f;
        ballIds.push_back(world.add(b));
    }
    for (int step = 0; step < 720; ++step) {
        world.step(1.0f / 60.0f, 14);
    }

    // --- Render the settled scene ------------------------------------------------------------------
    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 3D Physics (rigid-body dynamics)";
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

    // One mesh per body (colour baked in). Boxes use a unit cube scaled to their extents at draw time.
    std::vector<render::MeshHandle> boxMesh;
    for (std::size_t i = 0; i < boxIds.size(); ++i) {
        boxMesh.push_back(upload(*renderer, render::shapes::makeBox(1.0f, palette(i + 3))));
    }
    std::vector<render::MeshHandle> ballMesh;
    for (std::size_t i = 0; i < ballIds.size(); ++i) {
        const float r = world.bodies[static_cast<size_t>(ballIds[i])].radius;
        ballMesh.push_back(upload(*renderer, render::shapes::makeSphere(r, 18, 26, palette(i))));
    }
    render::MeshHandle ground =
        upload(*renderer, render::shapes::makePlane(10.0f, render::Color{0.30f, 0.33f, 0.38f, 1}));
    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle whiteTex = renderer->createTexture(1, 1, white);

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

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect = bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        const glm::vec3 eye(7.5f, 5.5f, 9.5f);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.10f, 0.12f, 0.16f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            const glm::mat4 groundModel(1.0f);
            renderer->drawMesh(ground, glm::value_ptr(groundModel), whiteTex);

            for (std::size_t i = 0; i < boxIds.size(); ++i) {
                const game::Body3D& b = world.bodies[static_cast<size_t>(boxIds[i])];
                const glm::quat q(b.orientation.w, b.orientation.x, b.orientation.y, b.orientation.z);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(b.pos.x, b.pos.y, b.pos.z));
                model *= glm::mat4_cast(q);
                model = glm::scale(model, glm::vec3(2.0f * b.half.x, 2.0f * b.half.y, 2.0f * b.half.z));
                renderer->drawMesh(boxMesh[i], glm::value_ptr(model), whiteTex);
            }

            for (std::size_t i = 0; i < ballIds.size(); ++i) {
                const game::Body3D& b = world.bodies[static_cast<size_t>(ballIds[i])];
                const glm::mat4 model =
                    glm::translate(glm::mat4(1.0f), glm::vec3(b.pos.x, b.pos.y, b.pos.z));
                renderer->drawMesh(ballMesh[i], glm::value_ptr(model), whiteTex);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f,
                          "MAZ ENGINE  -  3D PHYSICS (rigid-body dynamics, toward RigidBody3D)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "tilted boxes tumbled and settled flat; balls fell into a pile",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.4f);
            font.drawText(*renderer, 16.0f, 678.0f,
                          "PhysicsWorld3D: spheres + oriented boxes, inertia tensors, angular impulses",
                          render::Color{0.7f, 0.8f, 0.9f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PHYSICS3D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
