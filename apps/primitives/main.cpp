// Maz Engine — "PRIMITIVES" (procedural mesh primitives, toward Godot's built-in meshes)
// A lit gallery of the four new procedural solids — cylinder, cone, torus, capsule — built by
// render::shapes::make* and drawn through the existing 3D mesh path under a fixed camera (deterministic
// golden). Each is a distinct colour; a 2D HUD labels the row. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

// A tiny flat white texture so the per-vertex mesh colour shows through the lit material.
std::vector<uint8_t> makeWhite(uint32_t size) {
    return std::vector<uint8_t>(static_cast<size_t>(size) * size * 4, 235);
}

render::MeshHandle upload(render::Renderer& r, const render::shapes::MeshData& m) {
    return r.createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()), m.indices.data(),
                        static_cast<uint32_t>(m.indices.size()));
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PRIMITIVES (procedural meshes) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Mesh Primitives";
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

    namespace sh = render::shapes;
    const render::MeshHandle cylinder =
        upload(*renderer, sh::makeCylinder(1.1f, 2.6f, 32, render::Color{0.42f, 0.72f, 1.0f, 1.0f}));
    const render::MeshHandle cone =
        upload(*renderer, sh::makeCone(1.3f, 2.8f, 32, render::Color{1.0f, 0.66f, 0.34f, 1.0f}));
    const render::MeshHandle torus =
        upload(*renderer, sh::makeTorus(1.1f, 0.42f, 40, 20, render::Color{0.5f, 0.82f, 0.5f, 1.0f}));
    const render::MeshHandle capsule =
        upload(*renderer, sh::makeCapsule(0.8f, 1.6f, 32, 10, render::Color{0.85f, 0.5f, 0.9f, 1.0f}));
    const render::TextureHandle white = renderer->createTexture(4, 4, makeWhite(4).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    struct Item {
        render::MeshHandle mesh;
        float x;
        const char* label;
        float labelX;
    };
    const Item items[4] = {{cylinder, -5.4f, "cylinder", 120.0f},
                           {cone, -1.8f, "cone", 400.0f},
                           {torus, 1.8f, "torus", 700.0f},
                           {capsule, 5.4f, "capsule", 980.0f}};

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
        }

        // Fixed camera looking slightly down at the row.
        const glm::vec3 eye(0.0f, 3.4f, 9.4f);
        const glm::mat4 proj = math::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.14f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            render::Renderer::Material mat;
            mat.albedo = white;
            mat.roughness = 0.5f;
            mat.specular = 1.0f;
            for (const Item& it : items) {
                const glm::mat4 model =
                    glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(it.x, 0.0f, 0.0f)),
                                glm::radians(24.0f), glm::normalize(glm::vec3(1.0f, 0.4f, 0.0f)));
                renderer->drawMeshMaterial(it.mesh, glm::value_ptr(model), mat);
            }

            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  MESH PRIMITIVES (Cylinder/Cone/Torus/Capsule)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f, "procedural solids built by render::shapes::make*",
                          render::Color{0.75f, 0.8f, 0.9f, 1}, 0.34f);
            for (const Item& it : items) {
                font.drawText(*renderer, it.labelX, 640.0f, it.label,
                              render::Color{0.82f, 0.86f, 0.95f, 1}, 0.4f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PRIMITIVES shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
