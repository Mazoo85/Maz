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

    render::SceneLighting light;
    light.ambient[0] = light.ambient[1] = light.ambient[2] = 0.30f;
    light.sunDir[0] = 0.35f;
    light.sunDir[1] = 0.85f;
    light.sunDir[2] = 0.45f;
    light.sunColor[0] = light.sunColor[1] = light.sunColor[2] = 0.85f;
    renderer->setLighting(light);

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
            float ty = 56.0f;
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

            font.drawText(*renderer, 16.0f, fh - 30.0f, "MAZ ENGINE  -  EDITOR",
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
