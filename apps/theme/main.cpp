// Maz Engine — "THEME" (StyleBoxFlat + Theme server, toward Godot StyleBoxFlat/Theme)
// The other half of Godot's theming (M103 gave the nine-patch StyleBoxTexture): a StyleBoxFlat is a
// procedurally-drawn rounded-corner panel — background fill + border + per-corner radius + soft drop
// shadow, no texture. A Theme names those styles per control class + state. This demo builds one dark
// Theme, then draws a button in each state (normal / hover / pressed / disabled) resolved THROUGH the
// theme, plus a gallery showing each StyleBoxFlat feature on its own (sharp vs rounded, border, shadow,
// per-corner "tab" radius). Everything is static, so the render is deterministic and golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;

namespace {

ui::StyleBoxFlat makeStyle(render::Color bg, render::Color border, float borderWidth, float radius,
                           render::Color shadow, float shadowSize, math::vec2 shadowOffset) {
    ui::StyleBoxFlat s;
    s.bg = bg;
    s.border = border;
    s.borderWidth = borderWidth;
    s.radius = ui::Corners{radius};
    s.shadow = shadow;
    s.shadowSize = shadowSize;
    s.shadowOffset = shadowOffset;
    s.contentMargin = ui::Border{12.0f, 8.0f, 12.0f, 8.0f};
    return s;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("THEME (StyleBoxFlat + Theme) starting");

    // --- Build a dark theme. -------------------------------------------------------------------------
    ui::Theme theme;
    const render::Color shadowCol{0.0f, 0.0f, 0.0f, 0.35f};
    theme.setStyleBox("Panel/normal",
                      makeStyle({0.13f, 0.15f, 0.20f, 1.0f}, {0.30f, 0.34f, 0.44f, 1.0f}, 1.5f, 12.0f,
                                shadowCol, 14.0f, {0.0f, 6.0f}));
    theme.setStyleBox("Button/normal",
                      makeStyle({0.22f, 0.34f, 0.52f, 1.0f}, {0.42f, 0.58f, 0.82f, 1.0f}, 2.0f, 8.0f,
                                shadowCol, 8.0f, {0.0f, 4.0f}));
    theme.setStyleBox("Button/hover",
                      makeStyle({0.32f, 0.48f, 0.70f, 1.0f}, {0.55f, 0.74f, 1.00f, 1.0f}, 2.0f, 8.0f,
                                shadowCol, 11.0f, {0.0f, 5.0f}));
    theme.setStyleBox("Button/pressed",
                      makeStyle({0.16f, 0.26f, 0.40f, 1.0f}, {0.35f, 0.50f, 0.72f, 1.0f}, 2.0f, 8.0f,
                                {0.0f, 0.0f, 0.0f, 0.25f}, 3.0f, {0.0f, 1.0f}));
    theme.setStyleBox("Button/disabled",
                      makeStyle({0.24f, 0.25f, 0.28f, 1.0f}, {0.34f, 0.35f, 0.38f, 1.0f}, 2.0f, 8.0f,
                                {0.0f, 0.0f, 0.0f, 0.0f}, 0.0f, {0.0f, 0.0f}));
    theme.setColor("Button/font", render::Color{0.96f, 0.98f, 1.0f, 1.0f});
    theme.setColor("Button/font_disabled", render::Color{0.55f, 0.57f, 0.60f, 1.0f});

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — StyleBoxFlat + Theme";
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

    const char* states[4] = {"normal", "hover", "pressed", "disabled"};
    const char* labels[4] = {"Normal", "Hover", "Pressed", "Disabled"};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  STYLEBOXFLAT + THEME",
                          render::Color{1, 1, 1, 1}, 0.62f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "procedural rounded-corner panels (fill + border + radius + drop shadow) named "
                          "per control class + state, Godot Theme-style",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            // --- Left: a themed panel holding a button in each state. --------------------------------
            const ui::Rect panelRect{40.0f, 110.0f, 520.0f, 560.0f};
            ui::drawStyleBoxFlat(*renderer, panelRect, theme.styleBox("Panel/normal"));
            font.drawText(*renderer, panelRect.x + 24.0f, panelRect.y + 20.0f,
                          "Theme: \"Dark\"  -  Button states", render::Color{0.82f, 0.88f, 0.98f, 1},
                          0.44f);

            for (int i = 0; i < 4; ++i) {
                const ui::Rect btn{panelRect.x + 40.0f, panelRect.y + 90.0f + static_cast<float>(i) * 110.0f,
                                   440.0f, 78.0f};
                const ui::StyleBoxFlat& style = theme.styleBox("Button", states[i]);
                ui::drawStyleBoxFlat(*renderer, btn, style);
                const render::Color txt = (i == 3) ? theme.color("Button/font_disabled")
                                                   : theme.color("Button/font");
                const ui::Rect content = style.contentRect(btn);
                font.drawText(*renderer, content.x + 16.0f, content.y + 20.0f, labels[i], txt, 0.5f);
                font.drawText(*renderer, content.x + 16.0f, content.y + 46.0f,
                              (std::string("theme.styleBox(\"Button\", \"") + states[i] + "\")").c_str(),
                              render::Color{txt.r, txt.g, txt.b, 0.7f}, 0.3f);
            }

            // --- Right: a gallery of individual StyleBoxFlat features. --------------------------------
            const float gx = 610.0f;
            font.drawText(*renderer, gx, 118.0f, "StyleBoxFlat gallery",
                          render::Color{0.7f, 0.8f, 1.0f, 1}, 0.44f);

            struct Sample {
                const char* caption;
                ui::StyleBoxFlat style;
            };
            Sample samples[6] = {
                {"sharp corners",
                 makeStyle({0.30f, 0.32f, 0.40f, 1}, {0, 0, 0, 0}, 0.0f, 0.0f, {0, 0, 0, 0}, 0.0f,
                           {0, 0})},
                {"rounded 16px",
                 makeStyle({0.30f, 0.32f, 0.40f, 1}, {0, 0, 0, 0}, 0.0f, 16.0f, {0, 0, 0, 0}, 0.0f,
                           {0, 0})},
                {"thick border",
                 makeStyle({0.24f, 0.30f, 0.42f, 1}, {0.5f, 0.7f, 1.0f, 1}, 5.0f, 12.0f, {0, 0, 0, 0},
                           0.0f, {0, 0})},
                {"soft drop shadow",
                 makeStyle({0.34f, 0.30f, 0.26f, 1}, {0, 0, 0, 0}, 0.0f, 12.0f, {0, 0, 0, 0.45f}, 16.0f,
                           {0.0f, 8.0f})},
                {"pill (max radius)",
                 makeStyle({0.26f, 0.40f, 0.30f, 1}, {0.4f, 0.7f, 0.5f, 1}, 2.0f, 999.0f, {0, 0, 0, 0},
                           0.0f, {0, 0})},
                {"tab (top corners)", {}},
            };
            // The tab: only the top two corners rounded.
            samples[5].style = makeStyle({0.40f, 0.28f, 0.40f, 1}, {0.7f, 0.5f, 0.7f, 1}, 2.0f, 0.0f,
                                         {0, 0, 0, 0}, 0.0f, {0, 0});
            samples[5].style.radius = ui::Corners{16.0f, 16.0f, 0.0f, 0.0f};

            for (int i = 0; i < 6; ++i) {
                const float col = static_cast<float>(i % 2);
                const float row = static_cast<float>(i / 2);
                const ui::Rect box{gx + col * 320.0f, 160.0f + row * 165.0f, 250.0f, 96.0f};
                ui::drawStyleBoxFlat(*renderer, box, samples[i].style);
                font.drawText(*renderer, box.x + 6.0f, box.y + box.h + 8.0f, samples[i].caption,
                              render::Color{0.78f, 0.82f, 0.9f, 1}, 0.34f);
            }

            font.drawText(*renderer, gx, 668.0f,
                          "one drawStyleBoxFlat call layers shadow -> border -> fill; the Theme resolves "
                          "\"Button/state\" (missing -> normal -> default).",
                          render::Color{0.68f, 0.74f, 0.84f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("THEME shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
