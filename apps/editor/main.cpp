// Maz Engine — "EDITOR" (a minimal in-engine scene editor, toward Godot's editor)
// A 3D viewport showing an editable scene, a scene-tree panel listing the nodes (click a row to
// select), and an inspector panel that live-edits the selected node's transform and PBR material.
// Click an object in the viewport to select it too. The selection is outlined with a wire box.
// Fixed camera => deterministic golden. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::vector<uint8_t> solidRGBA(uint8_t r, uint8_t g, uint8_t b) {
    return {r, g, b, 255, r, g, b, 255, r, g, b, 255, r, g, b, 255};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("EDITOR starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Editor";
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
    const std::vector<render::MeshHandle> meshes = {
        upload(sh::makeBox(1.0f, render::Color{1, 1, 1, 1})),
        upload(sh::makeSphere(0.5f, 32, 40, render::Color{1, 1, 1, 1})),
    };
    const render::MeshHandle ground = upload(sh::makePlane(9.0f, render::Color{1, 1, 1, 1}));

    // A small material swatch palette (1x1 albedo textures the nodes index by colorIndex).
    const std::vector<render::TextureHandle> swatches = {
        renderer->createTexture(2, 2, solidRGBA(210, 90, 80).data()),   // red
        renderer->createTexture(2, 2, solidRGBA(90, 170, 220).data()),  // blue
        renderer->createTexture(2, 2, solidRGBA(120, 200, 120).data()), // green
        renderer->createTexture(2, 2, solidRGBA(225, 200, 110).data()), // gold
        renderer->createTexture(2, 2, solidRGBA(210, 210, 215).data()), // white
    };
    const render::TextureHandle groundTex = renderer->createTexture(2, 2, solidRGBA(150, 150, 155).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 32.0f);
    }
    const render::TextureHandle white = swatches.back();
    ui::Context gui;
    gui.init(*renderer, font, white);

    // Build the editable scene.
    editor::Scene scene;
    auto addNode = [&](const char* name, uint32_t mesh, math::vec3 pos, int color, float rough,
                       float metal) {
        editor::Node n;
        n.name = name;
        n.meshId = mesh;
        n.position = pos;
        n.colorIndex = color;
        n.roughness = rough;
        n.metallic = metal;
        n.specular = 1.0f;
        if (mesh == 1) { // sphere: local AABB radius 0.5
            n.localMin = math::vec3(-0.5f);
            n.localMax = math::vec3(0.5f);
        }
        scene.nodes.push_back(n);
    };
    addNode("Crate", 0, math::vec3(-1.6f, 0.5f, 0.0f), 3, 0.7f, 0.0f);
    addNode("BigBox", 0, math::vec3(0.4f, 0.75f, -1.0f), 1, 0.4f, 0.0f);
    scene.nodes.back().scale = math::vec3(1.5f);
    scene.nodes.back().localMax = math::vec3(0.5f); // keep local unit box; scale handles size
    addNode("Ball", 1, math::vec3(-0.6f, 0.5f, 1.1f), 0, 0.25f, 1.0f);
    addNode("Sphere2", 1, math::vec3(1.7f, 0.5f, 0.4f), 2, 0.5f, 0.0f);
    addNode("SmallCube", 0, math::vec3(1.1f, 0.35f, 1.4f), 4, 0.3f, 0.0f);
    scene.nodes.back().scale = math::vec3(0.7f);
    scene.selected = 0;

    bool dragging = false;         // translate-gizmo drag in progress
    math::vec3 dragOffset{0, 0, 0}; // node pos minus ground-plane hit at grab time
    editor::History history;        // undo/redo snapshots

    // Where Ctrl+S / Ctrl+O save and load the scene (a guaranteed-writable per-user dir).
    std::string scenePath;
    {
        char* pref = SDL_GetPrefPath("MazEngine", "editor");
        scenePath = (pref ? std::string(pref) : std::string()) + "scene.json";
        if (pref) {
            SDL_free(pref);
        }
    }

    render::SceneLighting light;
    light.ambient[0] = light.ambient[1] = light.ambient[2] = 0.30f;
    light.sunDir[0] = 0.35f;
    light.sunDir[1] = 0.85f;
    light.sunDir[2] = 0.45f;
    light.sunColor[0] = light.sunColor[1] = light.sunColor[2] = 0.85f;
    renderer->setLighting(light);

    // Discrete scene edits (add / duplicate / delete) commit one undo step immediately. commitEdit
    // pushes the before-state and, if a mouse gesture happens to be open, re-bases its pending
    // snapshot so the gesture's own end() doesn't record the same change a second time.
    auto commitEdit = [&](const std::vector<editor::Node>& before) {
        history.commit(before, scene.nodes);
        if (history.inGesture) {
            history.pending = scene.nodes;
        }
    };
    auto addPrimitive = [&](uint32_t mesh) {
        std::vector<editor::Node> before = scene.nodes;
        editor::Node n;
        n.name = std::string(mesh == 1 ? "Sphere" : "Box") + std::to_string(scene.nodes.size());
        n.meshId = mesh;
        n.position = math::vec3(0.0f, 0.5f, 0.0f);
        n.colorIndex = static_cast<int>(scene.nodes.size()) % static_cast<int>(swatches.size());
        n.roughness = 0.6f;
        n.specular = 1.0f;
        scene.nodes.push_back(n); // default local AABB ±0.5 suits both box and sphere
        scene.selected = static_cast<int>(scene.nodes.size()) - 1;
        commitEdit(before);
    };
    auto duplicateSelected = [&]() {
        editor::Node* s = scene.selectedNode();
        if (!s) {
            return;
        }
        std::vector<editor::Node> before = scene.nodes;
        editor::Node copy = *s;
        copy.name += " copy";
        copy.position.x += 0.6f;
        copy.position.z += 0.6f;
        scene.nodes.push_back(copy);
        scene.selected = static_cast<int>(scene.nodes.size()) - 1;
        commitEdit(before);
    };
    auto deleteSelected = [&]() {
        if (scene.selected < 0 || scene.selected >= static_cast<int>(scene.nodes.size())) {
            return;
        }
        std::vector<editor::Node> before = scene.nodes;
        scene.nodes.erase(scene.nodes.begin() + scene.selected);
        if (scene.selected >= static_cast<int>(scene.nodes.size())) {
            scene.selected = static_cast<int>(scene.nodes.size()) - 1;
        }
        commitEdit(before);
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float fw = static_cast<float>(bw), fh = static_cast<float>(bh);
        const float aspect = bh > 0 ? fw / fh : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        const glm::vec3 eye(3.6f, 3.4f, 6.4f);
        const glm::mat4 proj = math::perspective(glm::radians(46.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.2f, 0.3f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        const float mx = input.mouseX(), my = input.mouseY();
        const bool down = input.mouseDown(0);

        // Undo / redo (Ctrl+Z / Ctrl+Y). Handled outside gesture bracketing below.
        const bool ctrl =
            input.keyDown(SDL_SCANCODE_LCTRL) || input.keyDown(SDL_SCANCODE_RCTRL);
        if (ctrl && input.keyPressed(SDL_SCANCODE_Z)) {
            history.undo(scene.nodes);
        }
        if (ctrl && input.keyPressed(SDL_SCANCODE_Y)) {
            history.redo(scene.nodes);
        }
        // Node ops via keyboard: Ctrl+D duplicates the selection, Delete removes it.
        if (ctrl && input.keyPressed(SDL_SCANCODE_D)) {
            duplicateSelected();
        }
        if (input.keyPressed(SDL_SCANCODE_DELETE)) {
            deleteSelected();
        }
        if (scene.selected >= static_cast<int>(scene.nodes.size())) {
            scene.selected = -1;
        }
        // Save / load the scene (Ctrl+S / Ctrl+O) as human-readable JSON.
        if (ctrl && input.keyPressed(SDL_SCANCODE_S)) {
            io::writeTextFile(scenePath, editor::toJson(scene).dump(2));
            MAZ_LOG_INFO("saved scene -> %s", scenePath.c_str());
        }
        if (ctrl && input.keyPressed(SDL_SCANCODE_O)) {
            std::string txt;
            const io::JsonParseResult pr =
                io::readTextFile(scenePath, txt) ? io::parseJson(txt) : io::JsonParseResult{};
            if (pr.ok) {
                editor::fromJson(pr.value, scene);
                if (scene.selected >= static_cast<int>(scene.nodes.size())) {
                    scene.selected = -1;
                }
            }
        }

        // Undo bracketing: a drag / keyboard-nudge / slider grab is one undo step. Snapshot the scene
        // when such a gesture starts and commit it (if anything changed) when the gesture ends.
        const bool nudgeHeld =
            scene.selectedNode() != nullptr &&
            (input.keyDown(SDL_SCANCODE_LEFT) || input.keyDown(SDL_SCANCODE_RIGHT) ||
             input.keyDown(SDL_SCANCODE_UP) || input.keyDown(SDL_SCANCODE_DOWN) ||
             input.keyDown(SDL_SCANCODE_Q) || input.keyDown(SDL_SCANCODE_E));
        const bool gestureActive = down || nudgeHeld;
        if (gestureActive && !history.inGesture) {
            history.begin(scene.nodes);
        } else if (!gestureActive && history.inGesture) {
            history.end(scene.nodes);
        }

        // Viewport click-to-pick + translate gizmo: a left click in the 3D viewport (not over a
        // panel) casts a ray; it selects the nearest node it hits and begins a ground-plane drag.
        const float treeW = 240.0f, inspW = 288.0f;
        const bool inViewport = mx > treeW + 8.0f && mx < fw - inspW - 8.0f;
        const glm::mat4 invVP = glm::inverse(viewProj);
        if (input.mousePressed(0) && inViewport) {
            math::vec3 ro, rd;
            editor::screenRay(invVP, mx, my, fw, fh, ro, rd);
            const int hit = editor::pickNode(scene, ro, rd);
            if (hit >= 0) {
                scene.selected = hit;
                math::vec3 planeHit;
                if (editor::rayPlaneY(ro, rd, scene.nodes[static_cast<size_t>(hit)].position.y,
                                      planeHit)) {
                    dragging = true;
                    const math::vec3& p = scene.nodes[static_cast<size_t>(hit)].position;
                    dragOffset = math::vec3(p.x - planeHit.x, 0.0f, p.z - planeHit.z);
                }
            }
        }
        // Continue dragging: move the selected node so its grab point tracks the cursor on its plane.
        if (dragging && input.mouseDown(0)) {
            if (editor::Node* s = scene.selectedNode()) {
                math::vec3 ro, rd, planeHit;
                editor::screenRay(invVP, mx, my, fw, fh, ro, rd);
                if (editor::rayPlaneY(ro, rd, s->position.y, planeHit)) {
                    s->position.x = planeHit.x + dragOffset.x;
                    s->position.z = planeHit.z + dragOffset.z;
                }
            }
        }
        if (!input.mouseDown(0)) {
            dragging = false;
        }
        // Keyboard nudge of the selected node: arrows move on the ground plane, Q/E rotate.
        if (editor::Node* s = scene.selectedNode()) {
            const float step = 0.06f;
            if (input.keyDown(SDL_SCANCODE_LEFT)) s->position.x -= step;
            if (input.keyDown(SDL_SCANCODE_RIGHT)) s->position.x += step;
            if (input.keyDown(SDL_SCANCODE_UP)) s->position.z -= step;
            if (input.keyDown(SDL_SCANCODE_DOWN)) s->position.z += step;
            if (input.keyDown(SDL_SCANCODE_Q)) s->euler.y -= 2.0f;
            if (input.keyDown(SDL_SCANCODE_E)) s->euler.y += 2.0f;
        }

        renderer->setClearColor(render::Color{0.10f, 0.11f, 0.14f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            // Ground.
            {
                render::Renderer::Material gm;
                gm.albedo = groundTex;
                gm.roughness = 0.9f;
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
                renderer->drawMeshMaterial(ground, glm::value_ptr(m), gm);
            }
            // Scene nodes.
            for (const editor::Node& n : scene.nodes) {
                if (!n.visible) {
                    continue;
                }
                render::Renderer::Material mat;
                mat.albedo = swatches[static_cast<size_t>(n.colorIndex) % swatches.size()];
                mat.roughness = n.roughness;
                mat.metallic = n.metallic;
                mat.specular = n.specular;
                mat.emissive[0] = n.emissive.x;
                mat.emissive[1] = n.emissive.y;
                mat.emissive[2] = n.emissive.z;
                const glm::mat4 model = n.modelMatrix();
                renderer->drawMeshMaterial(meshes[n.meshId % meshes.size()], glm::value_ptr(model),
                                           mat);
            }
            // Selection outline: a yellow wire AABB around the selected node.
            if (editor::Node* sel = scene.selectedNode()) {
                math::vec3 mn, mxb;
                sel->worldAabb(mn, mxb);
                const float col[4] = {1.0f, 0.85f, 0.2f, 1.0f};
                renderer->drawAabb(glm::value_ptr(mn), glm::value_ptr(mxb), col);
                // RGB translate-gizmo axes from the node's origin (X red, Y green, Z blue).
                const glm::vec3 c = sel->position;
                const float L = 1.4f;
                const float rx[4] = {1.0f, 0.3f, 0.25f, 1.0f};
                const float gy[4] = {0.35f, 1.0f, 0.35f, 1.0f};
                const float bz[4] = {0.35f, 0.55f, 1.0f, 1.0f};
                const glm::vec3 ax = c + glm::vec3(L, 0, 0), ay = c + glm::vec3(0, L, 0),
                                az = c + glm::vec3(0, 0, L);
                renderer->drawLine(glm::value_ptr(c), glm::value_ptr(ax), rx);
                renderer->drawLine(glm::value_ptr(c), glm::value_ptr(ay), gy);
                renderer->drawLine(glm::value_ptr(c), glm::value_ptr(az), bz);
            }

            // ---- 2D editor UI (pixel space) ----
            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            gui.begin(mx, my, down);

            // Scene-tree panel on the left.
            const float panelW = 240.0f;
            gui.panel(ui::Rect{0, 0, panelW, fh}, gui.colBg);
            font.drawText(*renderer, 16.0f, 14.0f, "SCENE", render::Color{1, 1, 1, 1}, 0.55f);
            // Toolbar: add a box / sphere, duplicate or delete the selected node.
            {
                const float bwid = 52.0f, gap = 4.0f, by = 42.0f, bhgt = 26.0f;
                float bx = 10.0f;
                if (gui.button(70u, ui::Rect{bx, by, bwid, bhgt}, "+Box", 0.34f)) {
                    addPrimitive(0);
                }
                bx += bwid + gap;
                if (gui.button(71u, ui::Rect{bx, by, bwid, bhgt}, "+Sph", 0.34f)) {
                    addPrimitive(1);
                }
                bx += bwid + gap;
                if (gui.button(72u, ui::Rect{bx, by, bwid, bhgt}, "Dup", 0.34f)) {
                    duplicateSelected();
                }
                bx += bwid + gap;
                if (gui.button(73u, ui::Rect{bx, by, bwid, bhgt}, "Del", 0.34f)) {
                    deleteSelected();
                }
            }
            float ty = 82.0f;
            for (size_t i = 0; i < scene.nodes.size(); ++i) {
                const ui::Rect row{10.0f, ty, panelW - 20.0f, 30.0f};
                const bool isSel = static_cast<int>(i) == scene.selected;
                if (isSel) {
                    gui.panel(row, gui.colActive);
                }
                if (gui.button(static_cast<uint32_t>(100 + i), row, scene.nodes[i].name.c_str(),
                               0.42f)) {
                    scene.selected = static_cast<int>(i);
                }
                ty += 36.0f;
            }

            // Inspector panel on the right: live-edit the selected node.
            if (editor::Node* sel = scene.selectedNode()) {
                const float iw = 288.0f;
                const float ix = fw - iw;
                gui.panel(ui::Rect{ix, 0, iw, fh}, gui.colBg);
                font.drawText(*renderer, ix + 16.0f, 14.0f, "INSPECTOR",
                              render::Color{1, 1, 1, 1}, 0.55f);
                font.drawText(*renderer, ix + 16.0f, 46.0f, sel->name.c_str(), gui.colAccent, 0.42f);

                float y = 84.0f;
                uint32_t id = 200;
                auto row = [&](const char* label, float& value, float lo, float hi) {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%s: %.2f", label, static_cast<double>(value));
                    font.drawText(*renderer, ix + 14.0f, y, buf, gui.colText, 0.34f);
                    gui.slider(id++, ui::Rect{ix + 14.0f, y + 20.0f, iw - 28.0f, 16.0f}, value, lo,
                               hi);
                    y += 46.0f;
                };
                font.drawText(*renderer, ix + 14.0f, y, "TRANSFORM", gui.colAccent, 0.32f);
                y += 24.0f;
                row("Pos X", sel->position.x, -4.0f, 4.0f);
                row("Pos Y", sel->position.y, 0.0f, 4.0f);
                row("Pos Z", sel->position.z, -4.0f, 4.0f);
                row("Rot Y", sel->euler.y, 0.0f, 360.0f);
                float uniform = sel->scale.x;
                row("Scale", uniform, 0.2f, 2.5f);
                sel->scale = math::vec3(uniform);

                y += 6.0f;
                font.drawText(*renderer, ix + 14.0f, y, "MATERIAL", gui.colAccent, 0.32f);
                y += 24.0f;
                row("Rough", sel->roughness, 0.05f, 1.0f);
                row("Metal", sel->metallic, 0.0f, 1.0f);
                float glow = sel->emissive.x;
                row("Glow", glow, 0.0f, 3.0f);
                sel->emissive = math::vec3(glow);

                // Color swatches: click one to recolor the node.
                font.drawText(*renderer, ix + 14.0f, y, "COLOR", gui.colText, 0.32f);
                for (int c = 0; c < static_cast<int>(swatches.size()); ++c) {
                    const ui::Rect sw{ix + 74.0f + static_cast<float>(c) * 38.0f, y - 4.0f, 30.0f,
                                      24.0f};
                    // Draw the swatch using its palette color (approximate the 1x1 texture color).
                    static const render::Color pal[5] = {{0.82f, 0.35f, 0.31f, 1}, {0.35f, 0.67f, 0.86f, 1},
                                                         {0.47f, 0.78f, 0.47f, 1}, {0.88f, 0.78f, 0.43f, 1},
                                                         {0.82f, 0.82f, 0.84f, 1}};
                    gui.panel(sw, pal[c]);
                    if (sel->colorIndex == c) { // outline the active swatch
                        gui.panel(ui::Rect{sw.x - 2.0f, sw.y - 2.0f, sw.w + 4.0f, 2.0f}, gui.colAccent);
                    }
                    if (input.mousePressed(0) && sw.contains(mx, my)) {
                        sel->colorIndex = c;
                    }
                }
                y += 40.0f;
                gui.toggle(500u, ui::Rect{ix + 14.0f, y, 22.0f, 22.0f}, "Visible", sel->visible,
                           0.34f);
            }

            font.drawText(*renderer, 16.0f, fh - 30.0f,
                          "MAZ ENGINE  -  EDITOR   (drag/arrows move; +Box/+Sph/Dup/Del or Ctrl+D/Del; Ctrl+Z/Y undo; Ctrl+S/O save/load)",
                          render::Color{0.7f, 0.75f, 0.85f, 1}, 0.34f);
            gui.end();

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("EDITOR shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
