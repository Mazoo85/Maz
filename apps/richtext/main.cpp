// Maz Engine — "RICHTEXT" (BBCode rich text, toward Godot's RichTextLabel)
// A plain label draws one string in one style. A RichTextLabel mixes styles WITHIN a string via BBCode:
// [b]bold[/b], [i]/[u], [color=...] and [size=...]. This parses several BBCode strings with ui::parseBBCode
// into styled runs, then lays each out left-to-right — applying each run's colour and pixel size, faking
// bold with a double-draw and drawing an underline bar — so you can see markup become formatted text.
// Everything is fixed → deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

// Draw one line of styled spans starting at (x, y). Returns nothing; advances internally.
void drawRich(render::Renderer& r, render::TextureHandle white, const ui::Font& font, float x, float y,
              const std::vector<ui::RichSpan>& spans, float basePx, render::Color defColor) {
    float cursor = x;
    for (const ui::RichSpan& s : spans) {
        const float px = s.sizePx > 0.0f ? s.sizePx : basePx;
        const float scale = px / font.lineHeight(1.0f);
        render::Color col =
            s.hasColor ? render::Color{s.r, s.g, s.b, s.a} : defColor;
        // Vertically align smaller/larger runs to a common baseline (place by baseline-ish top).
        const float ty = y + (basePx - px) * 0.7f;
        font.drawText(r, cursor, ty, s.text.c_str(), col, scale);
        if (s.bold) {
            // Fake weight: redraw shifted by a fraction of a pixel.
            font.drawText(r, cursor + 0.8f, ty, s.text.c_str(), col, scale);
        }
        const float w = font.textWidth(s.text.c_str(), scale);
        if (s.underline) {
            fillRect(r, white, cursor, ty + px * 0.92f, w, 2.0f, col);
        }
        cursor += w;
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RICHTEXT (BBCode) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Rich Text (BBCode)";
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
    render::TextureHandle white = whiteTex(*renderer);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 40.0f);
    }

    // Source BBCode strings + the parsed spans (parse once — deterministic).
    struct Example {
        std::string source;
        std::vector<ui::RichSpan> spans;
    };
    std::vector<std::string> sources = {
        "Plain text with a [b]bold[/b] word and an [i]italic[/i] one.",
        "Mix [b][color=#ff5555]bold red[/color][/b] and [u]underlined[/u] runs.",
        "Sizes: normal then [size=48]BIG[/size] then [size=20]small[/size] again.",
        "Colours: [color=red]red[/color] [color=green]green[/color] [color=cyan]cyan[/color] "
        "[color=orange]orange[/color].",
        "Nested [b]bold [i]and italic [color=yellow]and yellow[/color][/i][/b] together.",
        "Literal brackets [lb]like this[rb] and an [unknown]unknown tag[/unknown] pass through.",
    };
    std::vector<Example> examples;
    for (const std::string& src : sources) {
        examples.push_back({src, ui::parseBBCode(src)});
    }

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.6f, 0.66f, 0.78f, 1};
    const render::Color kSrc{0.5f, 0.72f, 0.55f, 1};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RICH TEXT (BBCode)", kText, 0.5f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "ui::parseBBCode turns markup into styled runs  -  RichTextLabel-style",
                          kDim, 0.28f);

            float y = 104.0f;
            for (const Example& ex : examples) {
                // The raw BBCode source (dim, monospace-ish) ...
                font.drawText(*renderer, 40.0f, y, ex.source.c_str(), kSrc, 0.26f);
                // ... then the formatted result just below it.
                drawRich(*renderer, white, font, 40.0f, y + 34.0f, ex.spans, 26.0f, kText);
                y += 98.0f;
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RICHTEXT shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
