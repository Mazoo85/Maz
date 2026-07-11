// Maz Engine — "WATER" (dynamic-mesh demo)
// A lit grid whose vertices are re-uploaded every frame with summed sine waves, proving the
// engine's dynamic-mesh path (createDynamicMesh + updateMesh with per-frame-in-flight buffers).
// The surface ripples under the shadow-mapped sun with distance fog fading into the sky.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr uint32_t kGrid = 64;         // vertices per side
constexpr float kExtent = 24.0f;       // world size of the water plane (centered on origin)

// One wave train: direction (dx,dz), spatial frequency, temporal speed, amplitude.
struct Wave {
    float dx, dz, freq, speed, amp;
};

const Wave kWaves[] = {
    {1.0f, 0.0f, 0.55f, 1.1f, 0.55f},
    {0.4f, 0.9f, 0.85f, 1.7f, 0.32f},
    {-0.7f, 0.5f, 1.35f, 2.3f, 0.18f},
    {0.2f, -1.0f, 2.10f, 3.1f, 0.10f},
};

// Water height and its analytic normal at (x,z) and time t (summed sines; normal from the
// partial derivatives so lighting responds to the ripples).
float waveHeight(float x, float z, float t) {
    float h = 0.0f;
    for (const Wave& w : kWaves) {
        h += w.amp * std::sin((x * w.dx + z * w.dz) * w.freq + t * w.speed);
    }
    return h;
}
glm::vec3 waveNormal(float x, float z, float t) {
    float dhdx = 0.0f, dhdz = 0.0f;
    for (const Wave& w : kWaves) {
        const float phase = (x * w.dx + z * w.dz) * w.freq + t * w.speed;
        const float c = std::cos(phase) * w.amp * w.freq;
        dhdx += c * w.dx;
        dhdz += c * w.dz;
    }
    return glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
}

// Fill a grid vertex buffer for time t. Positions span [-kExtent/2, +kExtent/2] on X/Z, height
// from the waves; color tints deep water toward the crests for a little sparkle.
void buildSurface(std::vector<render::MeshVertex>& verts, float t) {
    const float half = kExtent * 0.5f;
    const float step = kExtent / static_cast<float>(kGrid - 1);
    size_t i = 0;
    for (uint32_t gz = 0; gz < kGrid; ++gz) {
        for (uint32_t gx = 0; gx < kGrid; ++gx) {
            const float x = -half + static_cast<float>(gx) * step;
            const float z = -half + static_cast<float>(gz) * step;
            const float y = waveHeight(x, z, t);
            const glm::vec3 n = waveNormal(x, z, t);
            const float crest = glm::clamp(0.5f + 0.6f * y, 0.0f, 1.0f); // 0 trough .. 1 crest
            const glm::vec3 deep(0.03f, 0.14f, 0.32f);
            const glm::vec3 foam(0.30f, 0.55f, 0.78f);
            const glm::vec3 col = glm::mix(deep, foam, crest * crest); // bias toward deep blue
            render::MeshVertex& v = verts[i++];
            v = {x,   y,   z,   n.x, n.y, n.z, col.r, col.g, col.b,
                 static_cast<float>(gx) / static_cast<float>(kGrid - 1),
                 static_cast<float>(gz) / static_cast<float>(kGrid - 1)};
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("WATER (dynamic-mesh demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Water";
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

    // Grid geometry: kGrid×kGrid vertices, two triangles per cell. Vertices restream each frame;
    // the index buffer is fixed.
    std::vector<render::MeshVertex> verts(static_cast<size_t>(kGrid) * kGrid);
    std::vector<uint32_t> idx;
    idx.reserve(static_cast<size_t>(kGrid - 1) * (kGrid - 1) * 6);
    for (uint32_t gz = 0; gz < kGrid - 1; ++gz) {
        for (uint32_t gx = 0; gx < kGrid - 1; ++gx) {
            const uint32_t a = gz * kGrid + gx;
            const uint32_t b = a + 1;
            const uint32_t c = a + kGrid;
            const uint32_t d = c + 1;
            idx.insert(idx.end(), {a, c, b, b, c, d});
        }
    }
    buildSurface(verts, 0.0f);

    render::MeshHandle water =
        renderer->createDynamicMesh(verts.data(), static_cast<uint32_t>(verts.size()), idx.data(),
                                    static_cast<uint32_t>(idx.size()));
    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx); // untinted albedo

    // Sunlit sky with distance fog fading the far ripples into the horizon.
    render::SceneLighting lighting;
    lighting.ambient[0] = 0.16f;
    lighting.ambient[1] = 0.22f;
    lighting.ambient[2] = 0.30f;
    lighting.sunDir[0] = 0.35f;
    lighting.sunDir[1] = 0.55f;
    lighting.sunDir[2] = 0.75f;
    lighting.sunColor[0] = 1.0f;
    lighting.sunColor[1] = 0.96f;
    lighting.sunColor[2] = 0.85f;
    lighting.fogColor[0] = 0.62f;
    lighting.fogColor[1] = 0.74f;
    lighting.fogColor[2] = 0.88f;
    lighting.fogDensity = 0.020f;
    lighting.skyHorizon[0] = 0.62f;
    lighting.skyHorizon[1] = 0.74f;
    lighting.skyHorizon[2] = 0.88f;
    renderer->setLighting(lighting);
    renderer->setBloom(0.18f, 0.72f); // gentle sparkle on the crests

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }
    ui::DebugOverlay overlay;

    float t = 0.0f;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }
        if (input.keyPressed(SDL_SCANCODE_F3)) {
            overlay.toggle();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            t += static_cast<float>(clock.fixedDelta());
        }
        overlay.update(clock.frameDelta());
        buildSurface(verts, t); // animate the grid

        // Camera: a low, orbiting perspective skimming the water so the ripples catch the light.
        const float orbit = t * 0.12f;
        const glm::vec3 eye(std::sin(orbit) * 14.0f, 6.0f, std::cos(orbit) * 14.0f);
        const glm::mat4 proj = math::perspective(glm::radians(52.0f), aspect, 0.1f, 120.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.2f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;
        const glm::mat4 model(1.0f);

        renderer->setClearColor(render::Color{0.62f, 0.74f, 0.88f, 1.0f}); // horizon sky
        if (renderer->beginFrame()) {
            renderer->updateMesh(water, verts.data(), static_cast<uint32_t>(verts.size()));
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));
            renderer->drawMesh(water, glm::value_ptr(model), white);

            render::Camera2D uiCam;
            uiCam.usePixelSpace = true;
            renderer->setCamera2D(uiCam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  WATER",
                          render::Color{1, 1, 1, 1}, 0.75f);
            font.drawText(*renderer, 16.0f, 46.0f, "dynamic mesh: summed sine waves, per-frame vertices",
                          render::Color{0.85f, 0.9f, 0.98f, 1}, 0.46f);
            overlay.draw(*renderer, font, 16.0f, static_cast<float>(bh) - 40.0f, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("WATER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
