// Maz Engine — "MODEL" (glTF loading demo)
// Loads a low-poly house from a glTF 2.0 file at runtime (assets/models/house.gltf) and renders it
// on a ground plane with shadows, sky, and a 2D HUD, under a slowly orbiting camera. Proves the
// engine can render artist-authored models, not just procedural shapes. Run --headless / --frames N
// for CI (the file still loads; the renderer no-ops without a GPU).

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <string>

using namespace maz;

namespace {

std::string assetPath(const char* rel) {
    const char* base = SDL_GetBasePath();
    return (base ? std::string(base) : std::string()) + rel;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MODEL (glTF demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — glTF Model";
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

    // A 1x1 white texture: used for the ground, and as the house's fallback when the model carries
    // no texture (so its vertex colors show unmodulated).
    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle blank = renderer->createTexture(1, 1, white);

    // Load the house from disk. If it fails (missing asset), fall back to a box so the demo still
    // shows something rather than an empty scene.
    render::ModelData houseModel;
    if (!render::loadGltf(assetPath("assets/models/house.gltf").c_str(), houseModel)) {
        MAZ_LOG_WARN("falling back to a procedural box");
        houseModel.mesh = render::shapes::makeBox(1.5f, render::Color{0.8f, 0.7f, 0.5f, 1.0f});
    }
    render::MeshHandle house = renderer->createMesh(
        houseModel.mesh.vertices.data(), static_cast<uint32_t>(houseModel.mesh.vertices.size()),
        houseModel.mesh.indices.data(), static_cast<uint32_t>(houseModel.mesh.indices.size()));

    // Use the model's own base-color texture (its brick/shingle/plank/glass detail) when present.
    render::TextureHandle houseTex = blank;
    if (houseModel.hasTexture()) {
        houseTex = renderer->createTexture(houseModel.textureWidth, houseModel.textureHeight,
                                           houseModel.texturePixels.data());
    }

    render::shapes::MeshData groundData =
        render::shapes::makePlane(12.0f, render::Color{0.45f, 0.52f, 0.38f, 1.0f});
    render::MeshHandle ground = renderer->createMesh(
        groundData.vertices.data(), static_cast<uint32_t>(groundData.vertices.size()),
        groundData.indices.data(), static_cast<uint32_t>(groundData.indices.size()));

    ui::Font font;
    font.load(*renderer, assetPath("assets/fonts/DejaVuSans.ttf").c_str(), 36.0f);

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
            t += static_cast<float>(clock.fixedDelta());
        }

        // Camera slowly orbits the house at a gentle downward angle.
        const float r = 6.0f;
        const glm::vec3 eye(std::cos(t * 0.4f) * r, 3.4f, std::sin(t * 0.4f) * r);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.10f, 0.12f, 0.16f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->drawMesh(ground, glm::value_ptr(glm::mat4(1.0f)), blank);
            renderer->drawMesh(house, glm::value_ptr(glm::mat4(1.0f)), houseTex);

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  glTF MODEL",
                          render::Color{1, 1, 1, 1}, 0.75f);
            font.drawText(*renderer, 16.0f, 46.0f, "house.gltf loaded at runtime (textured)",
                          render::Color{0.75f, 0.8f, 0.9f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MODEL shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
