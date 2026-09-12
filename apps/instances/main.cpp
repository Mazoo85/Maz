// Maz Engine — "INSTANCES" (instanced-rendering demo)
// A field of hundreds of cubes rendered in a SINGLE instanced draw call (one model matrix per
// instance, supplied as per-instance vertex attributes). Proves the engine scales past one draw
// call per mesh. Run --headless / --frames N for CI.

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

void addFace(std::vector<render::MeshVertex>& verts, std::vector<uint32_t>& idx, glm::vec3 a,
             glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n, glm::vec3 col) {
    const auto base = static_cast<uint32_t>(verts.size());
    const glm::vec3 corners[4] = {a, b, c, d};
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        const glm::vec3 p = corners[i];
        verts.push_back({p.x, p.y, p.z, n.x, n.y, n.z, col.r, col.g, col.b, uv[i][0], uv[i][1]});
    }
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("INSTANCES (instanced-rendering demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Instancing";
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

    // A unit cube with per-face colors.
    const glm::vec3 p[8] = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
    std::vector<render::MeshVertex> verts;
    std::vector<uint32_t> idx;
    addFace(verts, idx, p[1], p[2], p[6], p[5], {1, 0, 0}, {0.85f, 0.4f, 0.4f});
    addFace(verts, idx, p[0], p[4], p[7], p[3], {-1, 0, 0}, {0.4f, 0.8f, 0.5f});
    addFace(verts, idx, p[3], p[7], p[6], p[2], {0, 1, 0}, {0.45f, 0.6f, 0.9f});
    addFace(verts, idx, p[0], p[1], p[5], p[4], {0, -1, 0}, {0.9f, 0.8f, 0.4f});
    addFace(verts, idx, p[4], p[5], p[6], p[7], {0, 0, 1}, {0.85f, 0.5f, 0.85f});
    addFace(verts, idx, p[1], p[0], p[3], p[2], {0, 0, -1}, {0.4f, 0.85f, 0.85f});
    render::MeshHandle cube =
        renderer->createMesh(verts.data(), static_cast<uint32_t>(verts.size()), idx.data(),
                             static_cast<uint32_t>(idx.size()));
    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    // A grid of instances. Each has a fixed base position and a per-instance spin phase.
    constexpr int kSide = 22; // 22*22 = 484 cubes
    struct Inst {
        glm::vec3 pos;
        float phase;
    };
    std::vector<Inst> insts;
    for (int gz = 0; gz < kSide; ++gz) {
        for (int gx = 0; gx < kSide; ++gx) {
            const float x = (static_cast<float>(gx) - kSide * 0.5f) * 1.8f;
            const float z = (static_cast<float>(gz) - kSide * 0.5f) * 1.8f;
            insts.push_back({glm::vec3(x, 0.0f, z), static_cast<float>(gx * 7 + gz * 3)});
        }
    }
    std::vector<glm::mat4> models(insts.size());

    render::SceneLighting lighting;
    lighting.fogColor[0] = 0.72f;
    lighting.fogColor[1] = 0.82f;
    lighting.fogColor[2] = 0.95f;
    lighting.fogDensity = 0.02f;
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
            t += static_cast<float>(clock.fixedDelta());
        }

        // Animate each instance's transform (bob + spin) and pack the matrices.
        for (size_t i = 0; i < insts.size(); ++i) {
            const Inst& in = insts[i];
            const float spin = t * 1.2f + in.phase;
            const float bob = 0.4f * std::sin(t * 2.0f + in.phase * 0.3f);
            glm::mat4 m = glm::translate(glm::mat4(1.0f), in.pos + glm::vec3(0.0f, bob, 0.0f));
            m = glm::rotate(m, spin, glm::vec3(0.3f, 1.0f, 0.2f));
            models[i] = m;
        }

        const glm::vec3 eye(std::sin(t * 0.15f) * 30.0f, 16.0f, std::cos(t * 0.15f) * 30.0f);
        const glm::mat4 proj = math::perspective(glm::radians(55.0f), aspect, 0.1f, 200.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.55f, 0.68f, 0.85f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            render::Renderer::Material mat;
            mat.albedo = white;
            mat.roughness = 0.4f;
            mat.specular = 0.5f;
            renderer->drawMeshInstanced(cube, reinterpret_cast<const float*>(models.data()),
                                        static_cast<uint32_t>(models.size()), mat);

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  INSTANCING",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%zu cubes in 1 instanced draw call", models.size());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.9f, 0.95f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("INSTANCES shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
