// Maz Engine — "CUBE" (3D demo)
// A lit, depth-tested spinning cube rendered through the engine's 3D mesh path, with 2D HUD text
// drawn over it — proving 2D and 3D compose in one frame. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <cstdint>
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

// A 2-color checkerboard RGBA texture.
std::vector<uint8_t> makeChecker(uint32_t size, uint32_t cell) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4, 255);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const bool on = ((x / cell) + (y / cell)) % 2 == 0;
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            const uint8_t c = on ? 235 : 120;
            px[i + 0] = c;
            px[i + 1] = c;
            px[i + 2] = on ? 245 : 130;
        }
    }
    return px;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CUBE (3D demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 3D Cube";
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

    // Build a unit cube: 6 faces, each a distinct color, with outward normals.
    const glm::vec3 p[8] = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
    std::vector<render::MeshVertex> verts;
    std::vector<uint32_t> idx;
    addFace(verts, idx, p[1], p[2], p[6], p[5], {1, 0, 0}, {0.90f, 0.25f, 0.25f});  // +X red
    addFace(verts, idx, p[0], p[4], p[7], p[3], {-1, 0, 0}, {0.30f, 0.80f, 0.35f}); // -X green
    addFace(verts, idx, p[3], p[7], p[6], p[2], {0, 1, 0}, {0.30f, 0.55f, 0.95f});  // +Y blue
    addFace(verts, idx, p[0], p[1], p[5], p[4], {0, -1, 0}, {0.95f, 0.80f, 0.25f}); // -Y yellow
    addFace(verts, idx, p[4], p[5], p[6], p[7], {0, 0, 1}, {0.85f, 0.35f, 0.85f});  // +Z magenta
    addFace(verts, idx, p[1], p[0], p[3], p[2], {0, 0, -1}, {0.30f, 0.85f, 0.85f}); // -Z cyan

    render::MeshHandle cube =
        renderer->createMesh(verts.data(), static_cast<uint32_t>(verts.size()), idx.data(),
                             static_cast<uint32_t>(idx.size()));
    render::TextureHandle checker = renderer->createTexture(64, 64, makeChecker(64, 8).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    float spin = 0.0f;
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
            spin += static_cast<float>(clock.fixedDelta());
        }

        // Camera: perspective (Vulkan-correct) looking at the origin.
        const glm::vec3 eye(2.6f, 2.0f, 3.2f);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;
        const glm::mat4 model =
            glm::rotate(glm::mat4(1.0f), spin, glm::normalize(glm::vec3(0.35f, 1.0f, 0.2f)));

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye)); // for the specular view direction
            render::Renderer::Material mat;
            mat.albedo = checker;
            mat.roughness = 0.45f; // glossy with a broad sheen
            mat.specular = 1.3f;   // bright sun highlight sweeps across the cube as it spins
            renderer->drawMeshMaterial(cube, glm::value_ptr(model), mat);

            // 2D HUD over the 3D scene (same frame).
            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  3D",
                          render::Color{1, 1, 1, 1}, 0.75f);
            font.drawText(*renderer, 16.0f, 46.0f, "lit, depth-tested mesh + 2D HUD",
                          render::Color{0.75f, 0.8f, 0.9f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CUBE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
