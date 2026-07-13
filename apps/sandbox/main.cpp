// Maz Engine — Sandbox
// The M0 walking skeleton: open a window, run a fixed-timestep loop, clear the screen to an
// animated color, and shut down cleanly. Run with --headless (or --frames N) for CI.

#include "maz/Engine.hpp"
#include "maz/assets/Model.hpp"
#include "maz/scene/Camera.hpp"
#include "maz/scene/Scene.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

using namespace maz;

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
