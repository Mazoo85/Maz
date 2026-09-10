// Maz Engine — "SENTINEL" — a stylized humanoid android built entirely from the engine's own
// procedural mesh primitives (see character.hpp), drawn with the Vulkan PBR path: brushed-metal
// plates, matte charcoal joints, and cyan-emissive accents, lit by a warm sun + a cool point light
// over a studio sky gradient, turning slowly on a turntable above a shadow-catching ground plane.
// It is the "build a good-looking 3D character with no external model file" showcase. A fixed initial
// framing keeps --headless --frames N deterministic for CI.

#include "maz/Engine.hpp"

#include "character.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {
// A tiny flat white texture so the per-vertex part colours show through unmodulated.
std::vector<uint8_t> makeWhite() { return std::vector<uint8_t>(4 * 4 * 4, 255); }

render::MeshHandle upload(render::Renderer& r, const render::shapes::MeshData& m) {
    return r.createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                        m.indices.data(), static_cast<uint32_t>(m.indices.size()));
}
} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SENTINEL (procedural 3D character) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — SENTINEL";
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
    double simTime = 0.0; // accumulated fixed-step time, drives the turntable

    // Build the character (3 material groups) and upload each as one mesh.
    const character::CharacterModel model = character::buildCharacter();
    const render::MeshHandle bodyMesh = upload(*renderer, model.body);
    const render::MeshHandle darkMesh = upload(*renderer, model.dark);
    const render::MeshHandle glowMesh = upload(*renderer, model.glow);
    const render::MeshHandle ground = upload(*renderer, render::shapes::makePlane(6.0f, render::Color{0.10f, 0.11f, 0.14f, 1.0f}));
    const render::TextureHandle white = renderer->createTexture(4, 4, makeWhite().data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    // Materials for the three groups (albedo comes from the baked vertex colours × white texture).
    render::Renderer::Material bodyMat;
    bodyMat.albedo = white;
    bodyMat.specular = 1.0f;
    bodyMat.metallic = 0.9f;
    bodyMat.roughness = 0.30f;

    render::Renderer::Material darkMat;
    darkMat.albedo = white;
    darkMat.specular = 0.5f;
    darkMat.metallic = 0.1f;
    darkMat.roughness = 0.75f;

    render::Renderer::Material glowMat;
    glowMat.albedo = white;
    glowMat.emissive[0] = 0.25f;
    glowMat.emissive[1] = 1.30f;
    glowMat.emissive[2] = 1.60f;

    render::Renderer::Material groundMat;
    groundMat.albedo = white;
    groundMat.specular = 0.2f;
    groundMat.metallic = 0.0f;
    groundMat.roughness = 0.9f;

    // Studio lighting: warm sun + a cool point light, over a blue-to-warm sky gradient the metal reflects.
    render::SceneLighting light;
    light.ambient[0] = 0.10f;
    light.ambient[1] = 0.12f;
    light.ambient[2] = 0.16f;
    light.sunDir[0] = -0.45f;
    light.sunDir[1] = 0.78f;
    light.sunDir[2] = 0.55f;
    light.sunColor[0] = 1.00f;
    light.sunColor[1] = 0.94f;
    light.sunColor[2] = 0.84f;
    light.pointCount = 1;
    light.points[0].pos[0] = -4.0f;
    light.points[0].pos[1] = 2.5f;
    light.points[0].pos[2] = 3.5f;
    light.points[0].range = 20.0f;
    light.points[0].color[0] = 0.45f;
    light.points[0].color[1] = 0.62f;
    light.points[0].color[2] = 1.00f;
    light.points[0].intensity = 2.2f;
    light.skyZenith[0] = 0.10f;
    light.skyZenith[1] = 0.16f;
    light.skyZenith[2] = 0.34f;
    light.skyHorizon[0] = 0.56f;
    light.skyHorizon[1] = 0.60f;
    light.skyHorizon[2] = 0.70f;
    light.skyGround[0] = 0.05f;
    light.skyGround[1] = 0.05f;
    light.skyGround[2] = 0.07f;
    renderer->setLighting(light);

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
            simTime += clock.fixedDelta();
        }

        // Camera frames the full figure; the character turns slowly on the spot (turntable).
        const glm::vec3 eye(0.0f, 1.15f, 4.7f);
        const glm::vec3 look(0.0f, 1.0f, 0.0f);
        const glm::mat4 proj = math::perspective(glm::radians(30.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, look, glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        const float angle = static_cast<float>(simTime) * 0.6f;
        const glm::mat4 spin = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
        const glm::mat4 identity(1.0f);

        renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            renderer->drawMeshMaterial(ground, glm::value_ptr(identity), groundMat);
            renderer->drawMeshMaterial(bodyMesh, glm::value_ptr(spin), bodyMat);
            renderer->drawMeshMaterial(darkMesh, glm::value_ptr(spin), darkMat);
            renderer->drawMeshMaterial(glowMesh, glm::value_ptr(spin), glowMat);

            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SENTINEL",
                          render::Color{1, 1, 1, 1}, 0.55f);
            font.drawText(*renderer, 16.0f, 44.0f, "a 3D character built from procedural primitives",
                          render::Color{0.72f, 0.82f, 0.95f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SENTINEL shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
