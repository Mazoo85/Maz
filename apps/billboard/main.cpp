// Maz Engine — "BILLBOARD" (3D billboard modes, toward Godot's SpriteBase3D / GeometryInstance3D billboards)
// Three rows of flat quad "cards" stand in a 3D scene, each row using a different render::BillboardMode:
//   • back  row — Enabled     : every card's plane squarely faces the (elevated) camera
//   • mid   row — YBillboard  : cards yaw to face the camera but stay perfectly upright
//   • front row — Disabled    : fixed orientation, so off-centre cards are seen obliquely
// The billboard model matrix is built by render::buildBillboard from the camera's view matrix. Static scene
// (fixed camera) → deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

// A portrait quad centred at the origin in the XY plane, normal +Z, white vertex colour (the card's colour
// comes from the material's emissive term so it reads flat and bright from any angle).
render::MeshHandle makeCardMesh(render::Renderer& r) {
    const float hw = 0.55f, hh = 0.85f;
    std::vector<render::MeshVertex> v = {
        {-hw, -hh, 0, 0, 0, 1, 1, 1, 1, 0, 1},
        {hw, -hh, 0, 0, 0, 1, 1, 1, 1, 1, 1},
        {hw, hh, 0, 0, 0, 1, 1, 1, 1, 1, 0},
        {-hw, hh, 0, 0, 0, 1, 1, 1, 1, 0, 0},
    };
    std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3};
    return r.createMesh(v.data(), 4, idx.data(), 6);
}

render::Color hue(float t) {
    const float k = 6.2831853f;
    return render::Color{0.5f + 0.5f * std::cos(k * t), 0.5f + 0.5f * std::cos(k * (t + 0.33f)),
                         0.5f + 0.5f * std::cos(k * (t + 0.66f)), 1.0f};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BILLBOARD (3D billboard modes) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Billboard Modes";
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

    render::MeshHandle card = makeCardMesh(*renderer);

    // A saturated 1x1 albedo texture per column, so cards read as solid colours rather than washing out.
    const int cardsPerRow = 5;
    std::vector<render::TextureHandle> colorTex;
    for (int c = 0; c < cardsPerRow; ++c) {
        const render::Color h = hue(static_cast<float>(c) / static_cast<float>(cardsPerRow));
        const uint8_t px[4] = {static_cast<uint8_t>(h.r * 235.0f), static_cast<uint8_t>(h.g * 235.0f),
                               static_cast<uint8_t>(h.b * 235.0f), 255};
        colorTex.push_back(renderer->createTexture(1, 1, px));
    }

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    struct Row {
        render::BillboardMode mode;
        float z;
    };
    const Row rows[3] = {
        {render::BillboardMode::Enabled, -2.5f},
        {render::BillboardMode::YBillboard, 0.0f},
        {render::BillboardMode::Disabled, 2.5f},
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect = bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        // Elevated camera looking down the rows, so the upright-vs-tilt difference is visible.
        const glm::vec3 eye(0.0f, 5.5f, 12.0f);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.14f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < cardsPerRow; ++c) {
                    const float x = (static_cast<float>(c) - (cardsPerRow - 1) * 0.5f) * 2.6f;
                    const glm::vec3 pos(x, 1.2f, rows[r].z);
                    const glm::mat4 model =
                        render::buildBillboard(pos, glm::vec3(1.0f), view, rows[r].mode);
                    const render::Color col = hue(static_cast<float>(c) / static_cast<float>(cardsPerRow));
                    render::Renderer::Material mat;
                    mat.albedo = colorTex[static_cast<std::size_t>(c)];
                    // A gentle self-glow keeps colours vivid without washing to white.
                    mat.emissive[0] = col.r * 0.22f;
                    mat.emissive[1] = col.g * 0.22f;
                    mat.emissive[2] = col.b * 0.22f;
                    renderer->drawMeshMaterial(card, glm::value_ptr(model), mat);
                }
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  BILLBOARD MODES (3D)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "render::buildBillboard orients quads toward the camera from the view matrix",
                          render::Color{0.72f, 0.78f, 0.9f, 1}, 0.32f);
            font.drawText(*renderer, 16.0f, 660.0f,
                          "back = ENABLED (full face)   middle = Y-BILLBOARD (upright)   front = DISABLED "
                          "(fixed)",
                          render::Color{0.62f, 0.68f, 0.8f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BILLBOARD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
