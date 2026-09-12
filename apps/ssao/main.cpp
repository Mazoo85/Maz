// Maz Engine — "SSAO" (screen-space ambient occlusion showcase, toward Godot's SSAO)
// A cluster of matte boxes and spheres sitting on a floor, deliberately arranged so objects touch
// each other and the ground. SSAO darkens the contact creases and corners — the soft "dirt" in the
// gaps that grounds objects and reads as depth. Toggle with `--noao` to compare. Fixed camera =>
// deterministic golden. Run --headless / --frames N for CI.

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
    return std::vector<uint8_t>(static_cast<size_t>(size) * size * 4, 230);
}
} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    bool ao = true;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--noao") == 0) ao = false;
    }
    MAZ_LOG_INFO("SSAO showcase starting (AO %s)", ao ? "on" : "off");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — SSAO";
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
    auto upload = [&](const sh::MeshData& m) {
        return renderer->createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                                    m.indices.data(), static_cast<uint32_t>(m.indices.size()));
    };
    const render::MeshHandle boxMesh = upload(sh::makeBox(1.0f, render::Color{0.82f, 0.82f, 0.85f, 1}));
    const render::MeshHandle sphereMesh =
        upload(sh::makeSphere(0.5f, 32, 40, render::Color{0.85f, 0.83f, 0.80f, 1}));
    const render::MeshHandle ground =
        upload(sh::makePlane(10.0f, render::Color{0.72f, 0.72f, 0.74f, 1}));
    const render::TextureHandle white = renderer->createTexture(4, 4, makeWhite(4).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    render::SceneLighting light;
    light.ambient[0] = light.ambient[1] = light.ambient[2] = 0.55f; // high ambient so AO is what reads
    light.sunDir[0] = 0.35f;
    light.sunDir[1] = 0.85f;
    light.sunDir[2] = 0.4f;
    light.sunColor[0] = light.sunColor[1] = light.sunColor[2] = 0.55f;
    renderer->setLighting(light);
    renderer->setSsao(ao, 0.6f, 0.9f);

    // A huddle of boxes and spheres touching each other and the floor, so creases are everywhere.
    struct Obj {
        render::MeshHandle mesh;
        glm::vec3 pos;
        glm::vec3 scale;
        float rotY;
    };
    const Obj objs[] = {
        {boxMesh, {-1.4f, 0.5f, -0.6f}, {1.0f, 1.0f, 1.0f}, 0.3f},
        {boxMesh, {-0.2f, 0.75f, -1.1f}, {1.5f, 1.5f, 1.5f}, -0.5f},
        {boxMesh, {1.5f, 0.5f, -0.4f}, {1.0f, 1.0f, 1.0f}, 0.8f},
        {boxMesh, {0.5f, 0.35f, 0.9f}, {0.7f, 0.7f, 0.7f}, 0.1f},
        {sphereMesh, {-0.9f, 0.5f, 0.6f}, {1.0f, 1.0f, 1.0f}, 0.0f},
        {sphereMesh, {0.9f, 0.4f, 0.2f}, {0.8f, 0.8f, 0.8f}, 0.0f},
        {sphereMesh, {2.2f, 0.6f, -1.0f}, {1.2f, 1.2f, 1.2f}, 0.0f},
        {sphereMesh, {-2.2f, 0.45f, -1.3f}, {0.9f, 0.9f, 0.9f}, 0.0f},
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

        const glm::vec3 eye(0.0f, 3.4f, 6.6f);
        const glm::mat4 proj = math::perspective(glm::radians(48.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.4f, -0.3f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.62f, 0.66f, 0.72f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            render::Renderer::Material mat;
            mat.albedo = white;
            {
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
                renderer->drawMeshMaterial(ground, glm::value_ptr(m), mat);
            }
            for (const Obj& o : objs) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), o.pos);
                m = glm::rotate(m, o.rotY, glm::vec3(0, 1, 0));
                m = glm::scale(m, o.scale);
                renderer->drawMeshMaterial(o.mesh, glm::value_ptr(m), mat);
            }

            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SCREEN-SPACE AMBIENT OCCLUSION",
                          render::Color{1, 1, 1, 1}, 0.55f);
            font.drawText(*renderer, 16.0f, 44.0f,
                          ao ? "SSAO ON  -  contact creases darkened (run --noao to compare)"
                             : "SSAO OFF  -  flat ambient, no contact shading",
                          render::Color{0.85f, 0.9f, 1.0f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SSAO shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
