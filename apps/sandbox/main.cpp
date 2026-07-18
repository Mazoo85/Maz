// Maz Engine — Sandbox
// The M0 walking skeleton: open a window, run a fixed-timestep loop, clear the screen to an
// animated color, and shut down cleanly. Run with --headless (or --frames N) for CI.

#include "maz/Engine.hpp"
#include "maz/assets/Model.hpp"
#include "maz/scene/Camera.hpp"
#include "maz/scene/Scene.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using namespace maz;

namespace {

// A 32x32 neon checkerboard texture for the 2D sprite demo — hot pink / cyan, the SEGA-90s vibe.
// Returned as tightly packed RGBA8 (top row first), ready for Renderer::uploadTexture().
std::vector<uint8_t> makeNeonTile(uint32_t size = 32) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4);
    const uint32_t cell = size / 8; // 8x8 checker
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const bool pink = ((x / cell) + (y / cell)) % 2 == 0;
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            px[i + 0] = static_cast<uint8_t>(pink ? 255 : 0);    // R
            px[i + 1] = static_cast<uint8_t>(pink ? 20 : 255);   // G
            px[i + 2] = static_cast<uint8_t>(pink ? 147 : 255);  // B
            px[i + 3] = 255;                                     // A
        }
    }
    return px;
}

// Triangle wave: bounces `t` back and forth within [lo, hi] (like a ball off two walls).
float bounce(float t, float lo, float hi) {
    const float span = hi - lo;
    if (span <= 0.0f) {
        return lo;
    }
    const float phase = std::fmod(std::fabs(t), 2.0f * span);
    return lo + (phase <= span ? phase : 2.0f * span - phase);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("Maz Engine sandbox starting (headless=%d, frames=%d)", cfg.headless, cfg.frames);

    // Decide what to render. --scene loads a full scene (many entities); --load-model wraps a
    // single glTF/GLB in a one-entity scene and spins it. Neither → the plain clear loop.
    scene::Scene scn;
    const bool spinSingle = (cfg.scenePath == nullptr && cfg.modelPath != nullptr);
    if (cfg.scenePath != nullptr) {
        std::string err;
        if (!scene::loadScene(cfg.scenePath, scn, &err)) {
            MAZ_LOG_ERROR("failed to load scene '%s': %s", cfg.scenePath, err.c_str());
            return 1;
        }
        MAZ_LOG_INFO("loaded scene '%s' (\"%s\"): %zu entities", cfg.scenePath, scn.name.c_str(),
                     scn.entities.size());
    } else if (cfg.modelPath != nullptr) {
        scene::Entity e;
        e.name = "model";
        e.modelPath = cfg.modelPath;
        scn.entities.push_back(e);
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = cfg.title;
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        MAZ_LOG_ERROR("window init failed");
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
#if defined(MAZ_DEBUG)
    rc.enableValidation = !cfg.headless; // no ICD under the dummy driver
#endif
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        MAZ_LOG_ERROR("renderer init failed");
        return 1;
    }

    // Upload each unique model the scene references to the GPU, once, and remember its handle.
    // Uploads return -1 when headless/no-GPU, so drawing simply no-ops there (CI/tests still run).
    struct RenderItem {
        int handle = -1;
        scene::Transform transform;
    };
    std::vector<RenderItem> items;
    std::unordered_map<std::string, int> handleByPath;
    for (const scene::Entity& e : scn.entities) {
        if (e.modelPath.empty()) {
            continue;
        }
        auto found = handleByPath.find(e.modelPath);
        int handle = -1;
        if (found != handleByPath.end()) {
            handle = found->second;
        } else {
            assets::Model model;
            std::string err;
            if (assets::loadModel(e.modelPath, model, &err)) {
                handle = renderer->uploadModel(model);
                MAZ_LOG_INFO("model '%s': %zu verts, %zu tris (handle %d)", e.modelPath.c_str(),
                             model.vertexCount(), model.triangleCount(), handle);
            } else {
                MAZ_LOG_ERROR("scene references model '%s': %s", e.modelPath.c_str(), err.c_str());
            }
            handleByPath[e.modelPath] = handle;
        }
        items.push_back({handle, e.transform});
    }

    // 2D sprite demo (--sprite-demo): upload one neon tile and bounce a handful of sprites around
    // the screen over the 3D clear. Upload returns -1 when headless/no-GPU, so this no-ops in CI.
    int spriteTex = -1;
    constexpr int kDemoSprites = 6;
    if (cfg.spriteDemo) {
        const std::vector<uint8_t> tile = makeNeonTile(32);
        spriteTex = renderer->uploadTexture(tile.data(), 32, 32);
        MAZ_LOG_INFO("sprite demo: neon tile texture handle %d", spriteTex);
    }

    scene::Camera camera;

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    // Simulation state advanced on the fixed step (frame-rate independent).
    float hue = 0.0f;
    float spin = 0.0f;

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (window.consumeResized()) {
            uint32_t w = 0, h = 0;
            window.drawableSize(w, h);
            renderer->onResize(w, h);
        }
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            hue += dt;              // one unit ~= 1 rad/sec
            spin += dt * 0.9f;      // model rotation
        }

        // Keep the projection matched to the current drawable aspect ratio.
        uint32_t dw = 0, dh = 0;
        window.drawableSize(dw, dh);
        const float aspect = dh > 0 ? static_cast<float>(dw) / static_cast<float>(dh) : 1.0f;
        camera.setPerspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        camera.setTarget({0.0f, 0.0f, 0.0f});
        if (spinSingle) {
            camera.setPosition({0.0f, 1.4f, 3.0f}); // fixed view; the single model spins
        } else {
            const float a = hue * 0.25f;            // slowly orbit the scene
            camera.setPosition({std::sin(a) * 6.0f, 3.0f, std::cos(a) * 6.0f});
        }

        // A tilted spin applied only in single-model mode.
        const math::mat4 spinMatrix =
            glm::rotate(math::mat4(1.0f), spin, math::normalize(math::vec3(0.3f, 1.0f, 0.15f)));

        render::Color clear;
        clear.r = 0.5f + 0.5f * std::sin(hue);
        clear.g = 0.5f + 0.5f * std::sin(hue + 2.094f); // +120 deg
        clear.b = 0.5f + 0.5f * std::sin(hue + 4.188f); // +240 deg
        renderer->setClearColor(clear);
        if (renderer->beginFrame()) {
            for (const RenderItem& item : items) {
                math::mat4 modelMatrix = item.transform.matrix();
                if (spinSingle) {
                    modelMatrix = spinMatrix * modelMatrix;
                }
                const math::mat4 mvp = camera.viewProjection() * modelMatrix;
                renderer->drawModel(item.handle, mvp, modelMatrix);
            }

            if (spriteTex >= 0) {
                const float sw = 64.0f, sh = 64.0f;
                const float fw = static_cast<float>(dw), fh = static_cast<float>(dh);
                const float maxX = fw > sw ? fw - sw : 0.0f;
                const float maxY = fh > sh ? fh - sh : 0.0f;
                // Neon tint cycle: pink, cyan, purple, acid-green, orange, white.
                const render::Color tints[kDemoSprites] = {
                    {1.0f, 0.1f, 0.6f, 1.0f}, {0.1f, 1.0f, 1.0f, 1.0f}, {0.6f, 0.2f, 1.0f, 1.0f},
                    {0.6f, 1.0f, 0.1f, 1.0f}, {1.0f, 0.6f, 0.1f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
                for (int k = 0; k < kDemoSprites; ++k) {
                    render::Sprite s;
                    s.w = sw;
                    s.h = sh;
                    s.x = bounce(hue * 90.0f + static_cast<float>(k) * 47.0f, 0.0f, maxX);
                    s.y = bounce(hue * 70.0f + static_cast<float>(k) * 83.0f, 0.0f, maxY);
                    s.rotation = spin + static_cast<float>(k);
                    s.tint = tints[k];
                    renderer->drawSprite(spriteTex, s);
                }
            }

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            MAZ_LOG_INFO("reached frame cap (%d); exiting", cfg.frames);
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("shutting down after %d frames (%.2fs, renderer %s)", rendered, clock.elapsed(),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
