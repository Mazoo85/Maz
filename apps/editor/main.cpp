// Maz Editor — the visual scene editor.
//
// A window with a menu bar, a Hierarchy panel (the list of objects), an Inspector panel (edit the
// selected object's transform), and the live 3D scene behind them. Edits operate on an in-memory
// maz::scene::Scene that can be saved/loaded as .mazscene. See docs/EDITOR.md.
//
// Runs headless as a no-op (no GPU/UI) so CI can smoke-test that it starts and exits cleanly.

#include "maz/Engine.hpp"
#include "maz/assets/CompositeAsset.hpp"
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

#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

using namespace maz;

namespace {

// State for the Creator panel: the composite asset being authored plus the currently edited part.
struct CreatorState {
    assets::AssetDoc doc;
    int selectedPart = -1;
};

// Reduce a display name to a safe file stem (alphanumerics kept; spaces/dashes -> underscore).
std::string sanitizeStem(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out += c;
        } else if (c == ' ' || c == '-' || c == '_') {
            out += '_';
        }
    }
    return out.empty() ? std::string("untitled") : out;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The directory a kind of asset saves into, and the full .mazasset path for a document.
std::string assetDir(assets::AssetKind kind) {
    return kind == assets::AssetKind::Character ? "assets/characters" : "assets/items";
}
std::string assetPath(const assets::AssetDoc& doc) {
    return assetDir(doc.kind) + "/" + sanitizeStem(doc.name) + ".mazasset";
}

// Append a new part of the given primitive shape with sensible defaults, and select it.
void addPart(CreatorState& creator, assets::PrimitiveKind kind, const char* label) {
    assets::Part part;
    part.name = std::string(label) + " " + std::to_string(creator.doc.parts.size());
    part.kind = kind;
    creator.doc.parts.push_back(part);
    creator.selectedPart = static_cast<int>(creator.doc.parts.size()) - 1;
}

// Draw the Creator panel. Authors the composite asset; on Save/Bake it writes a .mazasset and
// (for Bake) adds/updates a scene entity referencing it, pushing its path to `bakeInvalidate` so
// the caller drops any stale upload cache entry and re-bakes the fresh geometry.
void buildCreatorUI(CreatorState& creator, scene::Scene& scn, int& selected,
                    std::vector<std::string>& bakeInvalidate) {
    assets::AssetDoc& doc = creator.doc;

    ImGui::Begin("Creator");

    char nameBuf[128];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", doc.name.c_str());
    if (ImGui::InputText("Asset Name", nameBuf, sizeof(nameBuf))) {
        doc.name = nameBuf;
    }
    int kindIdx = static_cast<int>(doc.kind);
    ImGui::RadioButton("Item", &kindIdx, static_cast<int>(assets::AssetKind::Item));
    ImGui::SameLine();
    ImGui::RadioButton("Character", &kindIdx, static_cast<int>(assets::AssetKind::Character));
    doc.kind = static_cast<assets::AssetKind>(kindIdx);

    ImGui::Separator();
    ImGui::TextDisabled("Parts");
    const int partCount = static_cast<int>(doc.parts.size());
    for (int i = 0; i < partCount; ++i) {
        const std::string label =
            doc.parts[static_cast<size_t>(i)].name + "##part" + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), i == creator.selectedPart)) {
            creator.selectedPart = i;
        }
    }

    if (ImGui::Button("+Box")) addPart(creator, assets::PrimitiveKind::Box, "Box");
    ImGui::SameLine();
    if (ImGui::Button("+Sphere")) addPart(creator, assets::PrimitiveKind::Sphere, "Sphere");
    ImGui::SameLine();
    if (ImGui::Button("+Cylinder")) addPart(creator, assets::PrimitiveKind::Cylinder, "Cylinder");
    ImGui::SameLine();
    if (ImGui::Button("+Plane")) addPart(creator, assets::PrimitiveKind::Plane, "Plane");
    ImGui::SameLine();
    if (ImGui::Button("Delete Part") && creator.selectedPart >= 0 &&
        creator.selectedPart < partCount) {
        doc.parts.erase(doc.parts.begin() + creator.selectedPart);
        creator.selectedPart = -1;
    }

    ImGui::Separator();
    if (creator.selectedPart >= 0 && creator.selectedPart < partCount) {
        assets::Part& part = doc.parts[static_cast<size_t>(creator.selectedPart)];
        char partName[128];
        std::snprintf(partName, sizeof(partName), "%s", part.name.c_str());
        if (ImGui::InputText("Part Name", partName, sizeof(partName))) {
            part.name = partName;
        }
        const char* shapes[] = {"Box", "Sphere", "Cylinder", "Plane"};
        int shapeIdx = static_cast<int>(part.kind);
        if (ImGui::Combo("Shape", &shapeIdx, shapes, IM_ARRAYSIZE(shapes))) {
            part.kind = static_cast<assets::PrimitiveKind>(shapeIdx);
        }
        // Only the fields this shape uses.
        switch (part.kind) {
        case assets::PrimitiveKind::Box:
            ImGui::DragFloat3("Size", part.params.size, 0.02f, 0.001f, 100.0f);
            break;
        case assets::PrimitiveKind::Sphere:
            ImGui::DragFloat("Radius", &part.params.radius, 0.02f, 0.001f, 100.0f);
            ImGui::DragInt("Segments", &part.params.segments, 1.0f, 3, 128);
            ImGui::DragInt("Rings", &part.params.rings, 1.0f, 2, 128);
            break;
        case assets::PrimitiveKind::Cylinder:
            ImGui::DragFloat("Radius", &part.params.radius, 0.02f, 0.001f, 100.0f);
            ImGui::DragFloat("Height", &part.params.height, 0.02f, 0.001f, 100.0f);
            ImGui::DragInt("Segments", &part.params.segments, 1.0f, 3, 128);
            break;
        case assets::PrimitiveKind::Plane:
            ImGui::DragFloat3("Size (W,_,D)", part.params.size, 0.02f, 0.001f, 100.0f);
            break;
        }
        ImGui::Separator();
        ImGui::DragFloat3("Position", &part.local.position.x, 0.05f);
        ImGui::DragFloat3("Rotation", &part.local.rotationEuler.x, 0.5f);
        // Phase 1 keeps scale uniform (single slider) so baked normals stay correct.
        float uniform = part.local.scale.x;
        if (ImGui::DragFloat("Scale", &uniform, 0.02f, 0.001f, 100.0f)) {
            part.local.scale = math::vec3(uniform, uniform, uniform);
        }
    } else {
        ImGui::TextDisabled("Add a part, then select it to edit.");
    }

    ImGui::Separator();
    const std::string path = assetPath(doc);
    ImGui::TextDisabled("-> %s", path.c_str());
    if (ImGui::Button("Save Asset")) {
        std::error_code ec;
        std::filesystem::create_directories(assetDir(doc.kind), ec);
        std::string serr;
        if (assets::saveAsset(path, doc, &serr)) {
            MAZ_LOG_INFO("saved asset to %s", path.c_str());
        } else {
            MAZ_LOG_ERROR("save asset failed: %s", serr.c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Bake & Preview") && !doc.parts.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(assetDir(doc.kind), ec);
        std::string serr;
        if (assets::saveAsset(path, doc, &serr)) {
            // Ensure exactly one scene entity references this asset; refresh its geometry.
            bool found = false;
            for (int i = 0; i < static_cast<int>(scn.entities.size()); ++i) {
                if (scn.entities[static_cast<size_t>(i)].modelPath == path) {
                    found = true;
                    selected = i;
                    break;
                }
            }
            if (!found) {
                scene::Entity ent;
                ent.name = doc.name;
                ent.modelPath = path;
                scn.entities.push_back(ent);
                selected = static_cast<int>(scn.entities.size()) - 1;
            }
            bakeInvalidate.push_back(path); // drop stale upload so the new bake is uploaded
            MAZ_LOG_INFO("baked & previewed %s", path.c_str());
        } else {
            MAZ_LOG_ERROR("bake failed: %s", serr.c_str());
        }
    }

    ImGui::End();
}

// Draw the editor panels. Mutates the scene (add/remove/select/edit) in response to input.
void buildEditorUI(scene::Scene& scn, int& selected, bool& running, const std::string& savePath,
                   CreatorState& creator, std::vector<std::string>& bakeInvalidate) {
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

    buildCreatorUI(creator, scn, selected, bakeInvalidate);
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

    // Upload-on-demand cache: model path -> renderer handle. A `.mazasset` path is baked from its
    // composite document; anything else is loaded as glTF. (Re-baking uploads a fresh model; the old
    // GPU handle is orphaned — the Renderer has no release yet. Acceptable for editor previews.)
    std::unordered_map<std::string, int> pathToHandle;
    auto ensureHandle = [&](const std::string& path) -> int {
        auto it = pathToHandle.find(path);
        if (it != pathToHandle.end()) {
            return it->second;
        }
        int handle = -1;
        assets::Model model;
        std::string loadErr;
        bool ok = false;
        if (endsWith(path, ".mazasset")) {
            assets::AssetDoc doc;
            if (assets::loadAsset(path, doc, &loadErr)) {
                model = assets::buildModel(doc);
                ok = true;
            }
        } else {
            ok = assets::loadModel(path, model, &loadErr);
        }
        if (ok) {
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

    CreatorState creator;
    std::vector<std::string> bakeInvalidate; // asset paths whose cached upload must be refreshed

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
                buildEditorUI(scn, selected, running, savePath, creator, bakeInvalidate);
                // Drop cache entries for freshly baked assets so their new geometry is uploaded.
                for (const std::string& p : bakeInvalidate) {
                    pathToHandle.erase(p);
                }
                bakeInvalidate.clear();
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
