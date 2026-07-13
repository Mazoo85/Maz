// Maz Editor — the visual scene editor.
//
// A window with a menu bar, a Hierarchy panel (the list of objects), an Inspector panel (edit the
// selected object's transform), and the live 3D scene behind them. Edits operate on an in-memory
// maz::scene::Scene that can be saved/loaded as .mazscene. See docs/EDITOR.md.
//
// Runs headless as a no-op (no GPU/UI) so CI can smoke-test that it starts and exits cleanly.

#include "maz/Engine.hpp"
#include "maz/assets/Model.hpp"
#include "maz/scene/Camera.hpp"
#include "maz/scene/Scene.hpp"

// See note in VulkanRenderer.cpp: quiet third-party ImGui header warnings under -Werror.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <SDL3/SDL_events.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

using namespace maz;

namespace {

// Draw the editor panels. Mutates the scene (add/remove/select/edit) in response to input.
void buildEditorUI(scene::Scene& scn, int& selected, bool& running, const std::string& savePath) {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Scene")) {
            if (ImGui::MenuItem("New")) {
                scn.entities.clear();
                scn.name = "Untitled Scene";
                selected = -1;
            }
            if (ImGui::MenuItem("Reload")) {
                scene::Scene tmp;
                std::string serr;
                if (scene::loadScene(savePath, tmp, &serr)) {
                    scn = tmp;
                    selected = scn.entities.empty() ? -1 : 0;
                }
            }
            if (ImGui::MenuItem("Save")) {
                std::string serr;
                if (scene::saveScene(savePath, scn, &serr)) {
                    MAZ_LOG_INFO("saved scene to %s", savePath.c_str());
                } else {
                    MAZ_LOG_ERROR("save failed: %s", serr.c_str());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                running = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Cube")) {
                scene::Entity ent;
                ent.name = "Cube " + std::to_string(scn.entities.size());
                ent.modelPath = "assets/models/cube.gltf";
                scn.entities.push_back(ent);
                selected = static_cast<int>(scn.entities.size()) - 1;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    const int count = static_cast<int>(scn.entities.size());

    ImGui::Begin("Hierarchy");
    ImGui::TextDisabled("%s", scn.name.c_str());
    ImGui::Separator();
    for (int i = 0; i < count; ++i) {
        const std::string label =
            scn.entities[static_cast<size_t>(i)].name + "##ent" + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), i == selected)) {
            selected = i;
        }
    }
    ImGui::Separator();
    if (ImGui::Button("Add Cube")) {
        scene::Entity ent;
        ent.name = "Cube " + std::to_string(scn.entities.size());
        ent.modelPath = "assets/models/cube.gltf";
        scn.entities.push_back(ent);
        selected = static_cast<int>(scn.entities.size()) - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selected >= 0 && selected < count) {
        scn.entities.erase(scn.entities.begin() + selected);
        selected = -1;
    }
    ImGui::End();

    ImGui::Begin("Inspector");
    if (selected >= 0 && selected < count) {
        scene::Entity& ent = scn.entities[static_cast<size_t>(selected)];
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", ent.name.c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            ent.name = nameBuf;
        }
        ImGui::DragFloat3("Position", &ent.transform.position.x, 0.05f);
        ImGui::DragFloat3("Rotation", &ent.transform.rotationEuler.x, 0.5f);
        ImGui::DragFloat3("Scale", &ent.transform.scale.x, 0.02f, 0.001f, 100.0f);
        ImGui::TextDisabled("model: %s",
                            ent.modelPath.empty() ? "(none)" : ent.modelPath.c_str());
    } else {
        ImGui::TextDisabled("Select an object in the Hierarchy.");
    }
    ImGui::End();
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("Maz Editor starting (headless=%d)", cfg.headless);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Editor";
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
    rc.enableValidation = !cfg.headless;
#endif
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        MAZ_LOG_ERROR("renderer init failed");
        return 1;
    }

    const bool gui = renderer->initGui(window);
    if (!gui && !cfg.headless) {
        MAZ_LOG_WARN("editor UI unavailable (no GPU?); rendering scene only");
    }

    const std::string savePath = cfg.scenePath ? cfg.scenePath : "assets/scenes/demo.mazscene";
    scene::Scene scn;
    std::string err;
    if (scene::loadScene(savePath, scn, &err)) {
        MAZ_LOG_INFO("loaded scene \"%s\": %zu entities", scn.name.c_str(), scn.entities.size());
    } else {
        MAZ_LOG_WARN("starting with an empty scene (%s)", err.c_str());
        scn = scene::Scene{};
    }

    // Upload-on-demand cache: model path -> renderer handle.
    std::unordered_map<std::string, int> pathToHandle;
    auto ensureHandle = [&](const std::string& path) -> int {
        auto it = pathToHandle.find(path);
        if (it != pathToHandle.end()) {
            return it->second;
        }
        int handle = -1;
        assets::Model model;
        std::string loadErr;
        if (assets::loadModel(path, model, &loadErr)) {
            handle = renderer->uploadModel(model);
        } else {
            MAZ_LOG_ERROR("model '%s': %s", path.c_str(), loadErr.c_str());
        }
        pathToHandle[path] = handle;
        return handle;
    };

    scene::Camera camera;
    core::Clock clock(1.0 / 60.0);
    int selected = scn.entities.empty() ? -1 : 0;
    float orbit = 0.0f;
    bool running = true;
    int rendered = 0;

    while (running && !window.shouldClose()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (gui) {
                ImGui_ImplSDL3_ProcessEvent(&ev);
            }
            if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                       ev.type == SDL_EVENT_WINDOW_RESIZED) {
                uint32_t w = 0, h = 0;
                window.drawableSize(w, h);
                renderer->onResize(w, h);
            }
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            orbit += static_cast<float>(clock.fixedDelta()) * 0.25f;
        }

        uint32_t dw = 0, dh = 0;
        window.drawableSize(dw, dh);
        const float aspect = dh > 0 ? static_cast<float>(dw) / static_cast<float>(dh) : 1.0f;
        camera.setPerspective(glm::radians(55.0f), aspect, 0.1f, 100.0f);
        camera.setPosition({std::sin(orbit) * 7.0f, 3.5f, std::cos(orbit) * 7.0f});
        camera.setTarget({0.0f, 0.5f, 0.0f});

        renderer->setClearColor({0.10f, 0.11f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            if (gui) {
                renderer->guiNewFrame();
                buildEditorUI(scn, selected, running, savePath);
            }
            for (const scene::Entity& ent : scn.entities) {
                if (ent.modelPath.empty()) {
                    continue;
                }
                const int handle = ensureHandle(ent.modelPath);
                const math::mat4 modelMatrix = ent.transform.matrix();
                const math::mat4 mvp = camera.viewProjection() * modelMatrix;
                renderer->drawModel(handle, mvp, modelMatrix);
            }
            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            MAZ_LOG_INFO("reached frame cap (%d); exiting", cfg.frames);
            running = false;
        }
    }

    MAZ_LOG_INFO("Maz Editor shutting down after %d frames", rendered);
    renderer->shutdown();
    window.shutdown();
    return 0;
}
