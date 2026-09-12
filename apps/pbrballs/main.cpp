// Maz Engine — "PBR BALLS" (the classic physically-based-rendering test grid)
// A 6x6 grid of spheres: columns sweep roughness left(glossy)->right(rough), rows sweep metallic
// bottom(dielectric)->top(metal). Lit by a warm directional sun plus a cool point light so the
// Cook-Torrance BRDF shows both highlights. This is the standard way engines (Godot included)
// showcase their metallic/roughness workflow. Fixed camera => deterministic golden image.
// Run --headless / --frames N for CI.

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

// A tiny flat white texture so the per-vertex/material albedo shows through unmodulated.
std::vector<uint8_t> makeWhite(uint32_t size) {
    return std::vector<uint8_t>(static_cast<size_t>(size) * size * 4, 255);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PBR BALLS (metallic/roughness grid) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — PBR Balls";
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

    // One shared sphere mesh, instanced across the grid via per-draw materials.
    const render::MeshHandle sphere = [&] {
        auto m = render::shapes::makeSphere(0.5f, 48, 64, render::Color{1, 1, 1, 1});
        return renderer->createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                                    m.indices.data(), static_cast<uint32_t>(m.indices.size()));
    }();
    const render::TextureHandle white = renderer->createTexture(4, 4, makeWhite(4).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    constexpr int N = 6;              // grid is N x N
    constexpr float spacing = 1.35f;  // world units between sphere centres

    // Lighting: warm sun + a cool point light off to the right so both BRDF lobes are visible.
    render::SceneLighting light;
    light.ambient[0] = light.ambient[1] = light.ambient[2] = 0.14f; // low ambient so highlights pop
    light.sunDir[0] = -0.35f;
    light.sunDir[1] = 0.75f;
    light.sunDir[2] = 0.55f;
    light.sunColor[0] = 1.0f;
    light.sunColor[1] = 0.93f;
    light.sunColor[2] = 0.82f;
    light.pointCount = 1;
    light.points[0].pos[0] = 5.5f;
    light.points[0].pos[1] = 2.0f;
    light.points[0].pos[2] = 4.5f;
    light.points[0].range = 22.0f;
    light.points[0].color[0] = 0.45f;
    light.points[0].color[1] = 0.65f;
    light.points[0].color[2] = 1.0f;
    light.points[0].intensity = 2.4f;
    // A studio-like environment gradient: deep-blue zenith, bright warm horizon band, dark floor.
    // Metals now mirror this (analytic IBL), so the gradient reads as a reflection across each ball.
    light.skyZenith[0] = 0.10f;
    light.skyZenith[1] = 0.16f;
    light.skyZenith[2] = 0.34f;
    light.skyHorizon[0] = 0.78f;
    light.skyHorizon[1] = 0.80f;
    light.skyHorizon[2] = 0.86f;
    light.skyGround[0] = 0.06f;
    light.skyGround[1] = 0.05f;
    light.skyGround[2] = 0.05f;
    renderer->setLighting(light);

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

        // Fixed camera framing the whole grid straight-on.
        const glm::vec3 eye(0.0f, 0.0f, 12.2f);
        const glm::mat4 proj = math::perspective(glm::radians(42.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            const float origin = -0.5f * (N - 1) * spacing;
            for (int row = 0; row < N; ++row) {
                for (int col = 0; col < N; ++col) {
                    render::Renderer::Material mat;
                    mat.albedo = white;
                    mat.specular = 1.0f; // opt into the PBR BRDF
                    // Columns: roughness 0.05 (glossy) -> 1.0 (rough). Rows: metallic 0 -> 1. The
                    // albedo is white, so metals read as chrome (Fresnel F0 = albedo) and dielectrics
                    // as a plain white plastic — the cleanest way to read the two sweeps.
                    mat.roughness = 0.05f + (static_cast<float>(col) / (N - 1)) * 0.95f;
                    mat.metallic = static_cast<float>(row) / (N - 1);

                    const glm::vec3 p(origin + static_cast<float>(col) * spacing,
                                      origin + static_cast<float>(row) * spacing, 0.0f);
                    const glm::mat4 model = glm::translate(glm::mat4(1.0f), p);
                    renderer->drawMeshMaterial(sphere, glm::value_ptr(model), mat);
                }
            }

            // 2D HUD labelling the axes.
            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            font.drawText(*renderer, 16.0f, 12.0f,
                          "MAZ ENGINE  -  PBR BALLS (Cook-Torrance metallic/roughness)",
                          render::Color{1, 1, 1, 1}, 0.55f);
            font.drawText(*renderer, 16.0f, 44.0f,
                          "columns: roughness glossy -> rough      rows: dielectric -> metal",
                          render::Color{0.72f, 0.78f, 0.9f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PBR BALLS shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
