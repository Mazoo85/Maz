// Maz Engine — "TONEMAP" (filmic tonemap operator showcase, toward Godot's tonemapper)
// A deliberately high-dynamic-range scene: bright saturated emissive spheres over a dark floor under
// a bright sky, so colors push well past 1.0 and the tonemap operator's shoulder is visible. The
// operator is selectable with `--op N` (0 = ACES Narkowicz, 1 = ACES fitted, 2 = AgX) and defaults
// to AgX — Godot 4.2+'s default, which desaturates highlights gracefully and avoids ACES' hue shift.
// Fixed camera => deterministic golden. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::vector<uint8_t> makeWhite(uint32_t size) {
    return std::vector<uint8_t>(static_cast<size_t>(size) * size * 4, 255);
}

const char* opName(int op) {
    return op == 2 ? "AgX (Godot 4.2+ default)" : (op == 1 ? "ACES (fitted)" : "ACES (Narkowicz)");
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    // Manual --op N override (0 = ACES, 1 = ACES-fitted, 2 = AgX); default AgX.
    int op = 2;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--op") == 0 && i + 1 < argc) {
            op = std::atoi(argv[i + 1]);
        }
    }
    auto tmOp = op == 2 ? render::Renderer::TonemapOp::AgX
                        : (op == 1 ? render::Renderer::TonemapOp::ACESFitted
                                   : render::Renderer::TonemapOp::ACES);
    MAZ_LOG_INFO("TONEMAP starting (operator: %s)", opName(op));

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Tonemap";
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

    const render::MeshHandle sphere = [&] {
        auto m = render::shapes::makeSphere(0.6f, 40, 56, render::Color{1, 1, 1, 1});
        return renderer->createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                                    m.indices.data(), static_cast<uint32_t>(m.indices.size()));
    }();
    const render::MeshHandle ground = [&] {
        auto m = render::shapes::makePlane(9.0f, render::Color{0.10f, 0.10f, 0.12f, 1.0f});
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

    // Bright sky so reflections/ambient are strong; a warm intense sun for hot highlights.
    render::SceneLighting light;
    light.ambient[0] = light.ambient[1] = light.ambient[2] = 0.14f;
    light.sunDir[0] = -0.3f;
    light.sunDir[1] = 0.7f;
    light.sunDir[2] = 0.5f;
    light.sunColor[0] = 3.4f; // > 1 : blows past white so the shoulder matters
    light.sunColor[1] = 3.2f;
    light.sunColor[2] = 2.7f;
    light.skyZenith[0] = 0.25f;
    light.skyZenith[1] = 0.42f;
    light.skyZenith[2] = 0.80f;
    light.skyHorizon[0] = 0.95f;
    light.skyHorizon[1] = 0.92f;
    light.skyHorizon[2] = 0.88f;
    light.skyGround[0] = 0.10f;
    light.skyGround[1] = 0.10f;
    light.skyGround[2] = 0.11f;
    renderer->setLighting(light);

    // HDR pipeline: bloom on the hot pixels, then the chosen tonemap with a slight exposure lift.
    renderer->setBloom(0.7f, 1.0f);
    renderer->setTonemap(1.1f, true, tmOp);

    // A row of emissive spheres in saturated hues pushed above 1.0 — these are what separates the
    // operators (ACES twists saturated brights toward orange/purple; AgX keeps the hue and desaturates).
    struct Ball {
        float x;
        glm::vec3 emissive; // self-illumination, well above 1.0
    };
    const Ball balls[5] = {
        {-4.4f, {3.6f, 0.20f, 0.20f}},  // hot red
        {-2.2f, {0.20f, 3.4f, 0.25f}},  // hot green
        {0.0f, {0.25f, 0.35f, 3.8f}},   // hot blue
        {2.2f, {3.4f, 2.4f, 0.30f}},    // hot yellow/orange
        {4.4f, {3.2f, 3.2f, 3.4f}},     // near-white
    };

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

        const glm::vec3 eye(0.0f, 2.2f, 9.2f);
        const glm::mat4 proj = math::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.02f, 0.02f, 0.03f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            {
                render::Renderer::Material gm;
                gm.albedo = white;
                gm.roughness = 0.9f;
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.6f, 0.0f));
                renderer->drawMeshMaterial(ground, glm::value_ptr(m), gm);
            }
            for (const Ball& b : balls) {
                render::Renderer::Material mat;
                mat.albedo = white;
                mat.roughness = 0.35f;
                mat.specular = 1.0f;
                mat.emissive[0] = b.emissive.r;
                mat.emissive[1] = b.emissive.g;
                mat.emissive[2] = b.emissive.b;
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(b.x, 0.3f, 0.0f));
                renderer->drawMeshMaterial(sphere, glm::value_ptr(m), mat);
            }

            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  FILMIC TONEMAP",
                          render::Color{1, 1, 1, 1}, 0.55f);
            font.drawText(*renderer, 16.0f, 44.0f, opName(op), render::Color{0.85f, 0.9f, 1.0f, 1},
                          0.42f);
            font.drawText(*renderer, 16.0f, 74.0f,
                          "HDR emissive spheres rolled off the shoulder (bloom + tonemap)",
                          render::Color{0.7f, 0.75f, 0.85f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TONEMAP shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
