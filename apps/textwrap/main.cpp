// Maz Engine — "TEXTWRAP" (text layout: word-wrap + alignment, toward Godot's Label autowrap)
// The Font can draw and measure a single line; ui::layoutText FITS a paragraph into a box — breaking it
// across lines at word boundaries so it never overflows, and aligning each line left / center / right.
// This demo wraps the same paragraph into three fixed-width panels (one per alignment) and a fourth panel
// that shows hard '\n' breaks preserved. All static → deterministic, golden-stable. Run --headless.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;

namespace {

void fillBox(render::Renderer& r, float x, float y, float w, float h, render::Color col) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, col);
}

// Lay out `text` into a box of `boxW` and draw each wrapped line at (x0 + line.x, y0 + line.y).
void drawWrapped(render::Renderer& r, ui::Font& font, float scale, const std::string& text, float x0,
                 float y0, float boxW, ui::TextAlign align, render::Color col) {
    const float lh = font.lineHeight(scale) * 1.15f;
    const auto measure = [&](std::string_view s) {
        return font.textWidth(std::string(s).c_str(), scale);
    };
    const ui::TextLayout layout = ui::layoutText(text, boxW, measure, lh, align);
    for (const ui::TextLine& line : layout.lines) {
        if (!line.text.empty()) {
            font.drawText(r, x0 + line.x, y0 + line.y, line.text.c_str(), col, scale);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TEXTWRAP (text layout) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Text Layout";
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
        font.load(*renderer, fontPath.c_str(), 32.0f);
    }

    const std::string body =
        "The ancient gate groaned open, and a gust of cold air spilled from the dark beyond. "
        "Torchlight flickered across carved stone as the party stepped inside, weapons ready.";
    const std::string hard =
        "QUEST LOG\n"
        "- Reach the inner sanctum\n"
        "- Recover the sunstone\n"
        "- Return before nightfall";

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kPanel{0.14f, 0.16f, 0.22f, 1.0f};
    const render::Color kText{0.90f, 0.93f, 1.0f, 1.0f};
    const render::Color kDim{0.68f, 0.74f, 0.86f, 1.0f};
    const render::Color kHead{0.55f, 0.78f, 1.0f, 1.0f};

    const float panelW = 384.0f;
    const float pad = 20.0f;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TEXT LAYOUT (word-wrap + align)", kText,
                          0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "one paragraph fit into a box: wrapped at word boundaries, aligned 3 ways",
                          kDim, 0.34f);

            struct Col {
                float x;
                const char* label;
                ui::TextAlign align;
            };
            const Col cols[3] = {{24.0f, "LEFT", ui::TextAlign::Left},
                                 {440.0f, "CENTER", ui::TextAlign::Center},
                                 {856.0f, "RIGHT", ui::TextAlign::Right}};
            for (const Col& c : cols) {
                fillBox(*renderer, c.x, 96.0f, panelW, 360.0f, kPanel);
                font.drawText(*renderer, c.x + pad, 108.0f, c.label, kHead, 0.4f);
                drawWrapped(*renderer, font, 0.42f, body, c.x + pad, 152.0f, panelW - 2.0f * pad,
                            c.align, kText);
            }

            // Bottom panel: hard '\n' breaks preserved (a quest log).
            fillBox(*renderer, 24.0f, 480.0f, 1216.0f, 210.0f, kPanel);
            font.drawText(*renderer, 44.0f, 492.0f, "HARD LINE BREAKS ('\\n' preserved)", kHead, 0.4f);
            drawWrapped(*renderer, font, 0.44f, hard, 44.0f, 536.0f, 1180.0f, ui::TextAlign::Left, kText);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TEXTWRAP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
