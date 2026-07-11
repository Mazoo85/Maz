// Maz Engine — "SCENE3D" (ECS + 3D demo)
// A small 3D scene: a ground plane and a ring of spinning spheres and boxes, each an ECS entity
// (Transform3D + Renderable), lit and depth-tested, viewed by an orbiting camera, with a 2D HUD.
// Demonstrates the 3D mesh path, procedural primitives, and the ECS composing together.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

struct Transform3D {
    glm::vec3 pos;
    float scale;
    glm::vec3 axis;
    float spin;  // radians/sec
    float angle; // current radians
};
struct Renderable {
    render::MeshHandle mesh;
    render::TextureHandle tex;
};

std::vector<uint8_t> makeChecker(uint32_t size, uint32_t cell) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4, 255);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const bool on = ((x / cell) + (y / cell)) % 2 == 0;
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            const uint8_t c = on ? 235 : 150;
            px[i + 0] = c;
            px[i + 1] = c;
            px[i + 2] = c;
        }
    }
    return px;
}

render::MeshHandle upload(render::Renderer& r, const render::shapes::MeshData& m) {
    return r.createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                        m.indices.data(), static_cast<uint32_t>(m.indices.size()));
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SCENE3D (ECS + 3D demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 3D Scene";
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

    // Meshes: a ground plane + a palette of colored spheres and boxes.
    namespace sh = render::shapes;
    render::MeshHandle ground = upload(*renderer, sh::makePlane(9.0f, render::Color{0.18f, 0.22f, 0.26f, 1}));
    std::vector<render::MeshHandle> shapes = {
        upload(*renderer, sh::makeSphere(0.7f, 16, 24, render::Color{0.90f, 0.35f, 0.45f, 1})),
        upload(*renderer, sh::makeSphere(0.7f, 16, 24, render::Color{0.35f, 0.80f, 0.55f, 1})),
        upload(*renderer, sh::makeSphere(0.7f, 16, 24, render::Color{0.40f, 0.60f, 0.95f, 1})),
        upload(*renderer, sh::makeBox(1.1f, render::Color{0.95f, 0.75f, 0.30f, 1})),
        upload(*renderer, sh::makeBox(1.1f, render::Color{0.80f, 0.45f, 0.90f, 1})),
    };

    // A flat white texture leaves mesh vertex colors unchanged; a checker tiles across the floor.
    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle whiteTex = renderer->createTexture(1, 1, white);
    render::TextureHandle groundTex = renderer->createTexture(64, 64, makeChecker(64, 32).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    // Scene: the ground, plus a ring of spinning shapes.
    ecs::World world;
    {
        const ecs::Entity g = world.create();
        world.add<Transform3D>(g, {glm::vec3(0.0f), 1.0f, glm::vec3(0, 1, 0), 0.0f, 0.0f});
        world.add<Renderable>(g, {ground, groundTex});
    }
    const int ringCount = 20;
    for (int i = 0; i < ringCount; ++i) {
        const float a = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(ringCount);
        const float radius = 5.0f;
        Transform3D t;
        t.pos = glm::vec3(std::cos(a) * radius, 1.0f, std::sin(a) * radius);
        t.scale = 0.6f + 0.4f * static_cast<float>((i * 37) % 5) / 4.0f;
        t.axis = glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f + 0.1f * static_cast<float>(i % 3)));
        t.spin = 0.8f + 0.5f * static_cast<float>(i % 4);
        t.angle = a;
        const ecs::Entity e = world.create();
        world.add<Transform3D>(e, t);
        world.add<Renderable>(e, {shapes[static_cast<size_t>(i) % shapes.size()], whiteTex});
    }

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
            world.each<Transform3D>([&](ecs::Entity, Transform3D& t) { t.angle += t.spin * dt; });
        }

        // Orbiting camera.
        const float ct = static_cast<float>(clock.elapsed()) * 0.35f;
        const glm::vec3 eye(std::cos(ct) * 11.0f, 6.0f, std::sin(ct) * 11.0f);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            world.view<Transform3D, Renderable>([&](ecs::Entity, Transform3D& t, Renderable& r) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), t.pos);
                model = glm::rotate(model, t.angle, t.axis);
                model = glm::scale(model, glm::vec3(t.scale));
                renderer->drawMesh(r.mesh, glm::value_ptr(model), r.tex);
            });

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  3D SCENE",
                          render::Color{1, 1, 1, 1}, 0.72f);
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%zu objects  (ECS + 3D)", world.size());
            font.drawText(*renderer, 16.0f, 44.0f, buf, render::Color{0.75f, 0.8f, 0.9f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SCENE3D shutting down (%zu entities, renderer %s)", world.size(),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
