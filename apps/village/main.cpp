// Maz Engine — "VILLAGE" (glTF scene demo)
// Loads a whole scene from one glTF file (assets/models/village.gltf) via maz::render::loadGltfScene
// — a ground plane, several textured houses, and trees, each a node with its own transform — and
// renders them with shadows, sky, and a 2D HUD. Fly through with WASD + mouse-look (Space/Shift for
// up/down). Run --headless / --frames N for CI; the scene still loads with no GPU.

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

std::string assetPath(const char* rel) {
    const char* base = SDL_GetBasePath();
    return (base ? std::string(base) : std::string()) + rel;
}

// A drawable scene object: a GPU mesh, its texture, and its world transform.
struct Placed {
    render::MeshHandle mesh;
    render::TextureHandle tex;
    float model[16];
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("VILLAGE (glTF scene demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — glTF Village";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }
    if (!cfg.headless) {
        window.setRelativeMouse(true); // mouse-look
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle blank = renderer->createTexture(1, 1, white);

    // Load the scene: one Placed per node, uploading its mesh and (if any) its texture once.
    std::vector<Placed> scene;
    render::SceneData sceneData;
    if (render::loadGltfScene(assetPath("assets/models/village.gltf").c_str(), sceneData)) {
        for (const render::SceneNode& node : sceneData.nodes) {
            Placed pl;
            pl.mesh = renderer->createMesh(
                node.mesh.vertices.data(), static_cast<uint32_t>(node.mesh.vertices.size()),
                node.mesh.indices.data(), static_cast<uint32_t>(node.mesh.indices.size()));
            pl.tex = node.hasTexture()
                         ? renderer->createTexture(node.textureWidth, node.textureHeight,
                                                   node.texturePixels.data())
                         : blank;
            std::memcpy(pl.model, node.model, sizeof(pl.model));
            scene.push_back(pl);
        }
    } else {
        MAZ_LOG_WARN("no village scene loaded; rendering empty");
    }

    ui::Font font;
    font.load(*renderer, assetPath("assets/fonts/DejaVuSans.ttf").c_str(), 36.0f);

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    // Camera framed to overlook the village; explorable with WASD + mouse.
    game::FlyCamera camera;
    camera.setPosition(glm::vec3(0.0f, 6.5f, 20.0f));
    camera.setYawPitch(-1.5708f, -0.30f); // face -Z, tilted down

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
            const float dt = static_cast<float>(clock.fixedDelta());
            const float fwd = (input.keyDown(SDL_SCANCODE_W) ? 1.0f : 0.0f) -
                              (input.keyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f);
            const float strafe = (input.keyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f) -
                                 (input.keyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);
            const float rise = (input.keyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f) -
                               (input.keyDown(SDL_SCANCODE_LSHIFT) ? 1.0f : 0.0f);
            camera.move(fwd, strafe, rise, dt, 8.0f);
        }
        const float sens = 0.0025f;
        camera.look(input.mouseDX() * sens, -input.mouseDY() * sens);

        const glm::mat4 proj = math::perspective(glm::radians(55.0f), aspect, 0.1f, 200.0f);
        const glm::mat4 viewProj = proj * camera.view();

        renderer->setClearColor(render::Color{0.10f, 0.12f, 0.16f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            for (const Placed& pl : scene) {
                renderer->drawMesh(pl.mesh, pl.model, pl.tex);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  glTF VILLAGE",
                          render::Color{1, 1, 1, 1}, 0.75f);
            font.drawText(*renderer, 16.0f, 46.0f, "scene loaded from one glTF file",
                          render::Color{0.75f, 0.8f, 0.9f, 1}, 0.5f);
            font.drawText(*renderer, 16.0f, 72.0f, "WASD move   mouse look   SPACE/SHIFT up-down",
                          render::Color{0.7f, 0.75f, 0.85f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("VILLAGE shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
