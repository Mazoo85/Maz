// Maz Engine — "STYLEBOX" (nine-patch / StyleBox UI, toward Godot's StyleBoxTexture / theming)
// A themed-panel gallery. Each panel is drawn with ui::ninePatch: the destination rectangle is sliced
// into a 3x3 grid by fixed border insets, and the nine cells are filled — the four CORNERS (gold) keep
// their exact size at every panel size, the four EDGES (blue) stretch along one axis, and the CENTER
// (dark) fills the rest. So the same "style" scales to a small square, a wide bar, a tall column, or a
// big box without distorting its corner art — exactly how a Godot StyleBox themes Panels and Buttons.
// The scene is static, so the render is deterministic and golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, const ui::Rect& q, render::Color c) {
    const render::Point2 p[4] = {
        {q.x, q.y}, {q.x + q.w, q.y}, {q.x + q.w, q.y + q.h}, {q.x, q.y + q.h}};
    r.drawConvexPolygon(p, 4, c);
}

// Draw a StyleBox: fill the nine-patch cells so corners stay fixed while edges/center stretch.
void drawStyleBox(render::Renderer& r, const ui::Rect& dst, const ui::Border& border,
                  render::Color corner, render::Color edge, render::Color center) {
    const ui::Rect src{0, 0, 64, 64}; // nominal "texture" size (only the region layout matters here)
    const auto patches = ui::ninePatch(dst, border, src);
    for (const ui::Patch& p : patches) {
        const render::Color c = p.isCorner() ? corner : (p.isCenter() ? center : edge);
        fillRect(r, p.dst, c);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("STYLEBOX (nine-patch UI) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Nine-Patch StyleBox";
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

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    const render::Color corner{1.0f, 0.82f, 0.42f, 1.0f}; // gold "corner art"
    const render::Color edge{0.28f, 0.42f, 0.62f, 1.0f};  // blue frame edges
    const render::Color fill{0.13f, 0.15f, 0.20f, 1.0f};  // dark panel body
    const ui::Border panelBorder{22.0f};

    struct Panel {
        ui::Rect rect;
        const char* label;
    };
    const Panel panels[4] = {
        {{40.0f, 110.0f, 180.0f, 130.0f}, "SMALL"},
        {{260.0f, 110.0f, 420.0f, 130.0f}, "WIDE  BAR"},
        {{40.0f, 280.0f, 180.0f, 300.0f}, "TALL"},
        {{260.0f, 280.0f, 420.0f, 300.0f}, "LARGE  PANEL"},
    };

    // Buttons: the same nine-patch style at a smaller border, themed consistently.
    const render::Color btnCorner{0.55f, 1.0f, 0.6f, 1.0f};
    const render::Color btnEdge{0.25f, 0.55f, 0.35f, 1.0f};
    const render::Color btnFill{0.12f, 0.2f, 0.15f, 1.0f};
    const ui::Border btnBorder{10.0f};
    const char* btnLabels[3] = {"OK", "CANCEL", "APPLY"};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            for (const Panel& pn : panels) {
                drawStyleBox(*renderer, pn.rect, panelBorder, corner, edge, fill);
                font.drawText(*renderer, pn.rect.x + 30.0f, pn.rect.y + 26.0f, pn.label,
                              render::Color{0.9f, 0.93f, 1.0f, 1}, 0.42f);
            }

            // A row of themed buttons on the right, all sharing one StyleBox.
            const float bx = 720.0f;
            for (int i = 0; i < 3; ++i) {
                const ui::Rect b{bx, 300.0f + static_cast<float>(i) * 70.0f, 380.0f, 52.0f};
                drawStyleBox(*renderer, b, btnBorder, btnCorner, btnEdge, btnFill);
                font.drawText(*renderer, b.x + 24.0f, b.y + 14.0f, btnLabels[i],
                              render::Color{0.85f, 1.0f, 0.9f, 1}, 0.42f);
            }
            font.drawText(*renderer, 720.0f, 250.0f, "buttons share one StyleBox",
                          render::Color{0.7f, 0.8f, 0.7f, 1}, 0.38f);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  NINE-PATCH STYLEBOX (THEMING)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 52.0f,
                          "gold CORNERS stay fixed, blue EDGES stretch, dark CENTER fills — one style at "
                          "any size",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("STYLEBOX shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
