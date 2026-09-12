// Maz Engine — "POPUPMENU" (ui::PopupMenu, toward Godot's PopupMenu)
// PopupMenu is the vertical item list behind right-click context menus, OptionButton dropdowns and menu bars.
// This draws one open context menu inside a StyleBoxFlat panel (M109): a hovered row gets an accent
// highlight, checkboxes and a single-choice radio group show their state, a disabled row is dimmed, thin
// separators divide groups, accelerator hints are right-aligned, and a submenu item gets a right-pointing
// arrow. The item model + geometry (itemRect / hover) is pure ui::PopupMenu; the app just draws the rows it
// reports. Static menu + fixed hover -> deterministic, golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, const ui::Rect& b, render::Color col) {
    const render::Point2 p[4] = {{b.x, b.y}, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.h}, {b.x, b.y + b.h}};
    r.drawConvexPolygon(p, 4, col);
}

void thickLine(render::Renderer& r, math::vec2 a, math::vec2 b, float w, render::Color col) {
    const math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-3f) {
        return;
    }
    const math::vec2 n{-d.y / len * w * 0.5f, d.x / len * w * 0.5f};
    const render::Point2 quad[4] = {
        {a.x + n.x, a.y + n.y}, {b.x + n.x, b.y + n.y}, {b.x - n.x, b.y - n.y}, {a.x - n.x, a.y - n.y}};
    r.drawConvexPolygon(quad, 4, col);
}

void disc(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    const int N = 16;
    render::Point2 poly[16];
    for (int i = 0; i < N; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(N) * 6.2831853f;
        poly[i] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(poly, N, col);
}

void checkTick(render::Renderer& r, math::vec2 c, render::Color col) {
    thickLine(r, math::vec2(c.x - 6, c.y), math::vec2(c.x - 1, c.y + 5), 2.4f, col);
    thickLine(r, math::vec2(c.x - 1, c.y + 5), math::vec2(c.x + 7, c.y - 6), 2.4f, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("POPUPMENU (ui::PopupMenu) starting");

    ui::PopupMenu menu;
    menu.position = math::vec2(470.0f, 150.0f);
    menu.width = 320.0f;
    menu.itemHeight = 36.0f;
    menu.separatorHeight = 13.0f;

    const std::size_t cut = menu.addItem("Cut", 1);
    menu.setShortcut(cut, "Ctrl+X");
    const std::size_t copy = menu.addItem("Copy", 2);
    menu.setShortcut(copy, "Ctrl+C");
    const std::size_t paste = menu.addItem("Paste", 3);
    menu.setShortcut(paste, "Ctrl+V");
    menu.setDisabled(paste, true);
    menu.addSeparator();
    const std::size_t wrap = menu.addCheckItem("Word Wrap", 4);
    menu.setChecked(wrap, true);
    menu.addCheckItem("Show Whitespace", 5);
    menu.addSeparator();
    menu.addRadioItem("Theme: Light", 10);
    const std::size_t dark = menu.addRadioItem("Theme: Dark", 11);
    menu.addRadioItem("Theme: System", 12);
    menu.checkRadio(dark);
    menu.addSeparator();
    menu.addSubmenuItem("Export As", 6);
    const std::size_t pref = menu.addItem("Preferences...", 7);
    menu.setShortcut(pref, "Ctrl+,");

    menu.setHovered(static_cast<long>(copy)); // as if the cursor rests on "Copy"

    ui::StyleBoxFlat panel;
    panel.bg = render::Color{0.13f, 0.14f, 0.18f, 1.0f};
    panel.border = render::Color{0.3f, 0.34f, 0.44f, 1.0f};
    panel.borderWidth = 1.5f;
    panel.radius = ui::Corners{8.0f};
    panel.shadow = render::Color{0.0f, 0.0f, 0.0f, 0.4f};
    panel.shadowSize = 18.0f;
    panel.shadowOffset = math::vec2(0.0f, 8.0f);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — PopupMenu";
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

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  POPUPMENU",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "context menu / dropdown items - Godot's PopupMenu (right-click / OptionButton) "
                          "(ui::PopupMenu)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);
            font.drawText(*renderer, 120.0f, 200.0f, "right-click ->", render::Color{0.55f, 0.6f, 0.7f, 1},
                          0.44f);

            // Panel behind the whole menu.
            ui::drawStyleBoxFlat(*renderer, menu.rect(), panel);

            const render::Color accent{0.22f, 0.36f, 0.56f, 1.0f};
            const render::Color txt{0.86f, 0.9f, 0.96f, 1.0f};
            const render::Color dim{0.45f, 0.48f, 0.56f, 1.0f};
            const render::Color hint{0.55f, 0.6f, 0.72f, 1.0f};

            for (std::size_t i = 0; i < menu.count(); ++i) {
                const ui::MenuItem& it = menu.item(i);
                const ui::Rect row = menu.itemRect(i);
                if (it.separator) {
                    fillRect(*renderer,
                             ui::Rect{row.x + 12.0f, row.y + row.h * 0.5f, row.w - 24.0f, 1.5f},
                             render::Color{0.28f, 0.31f, 0.4f, 1.0f});
                    continue;
                }

                // Hover highlight.
                if (menu.hovered() == static_cast<long>(i)) {
                    fillRect(*renderer, ui::Rect{row.x + 4.0f, row.y + 2.0f, row.w - 8.0f, row.h - 4.0f},
                             accent);
                }

                const float cy = row.y + row.h * 0.5f;
                const render::Color label = it.disabled ? dim : txt;

                // Check / radio column.
                if (it.check == ui::MenuCheck::CheckBox && it.checked) {
                    checkTick(*renderer, math::vec2(row.x + 22.0f, cy - 2.0f),
                              render::Color{0.6f, 0.85f, 1.0f, 1.0f});
                } else if (it.check == ui::MenuCheck::Radio) {
                    disc(*renderer, math::vec2(row.x + 22.0f, cy), 6.0f,
                         render::Color{0.3f, 0.34f, 0.42f, 1.0f});
                    if (it.checked) {
                        disc(*renderer, math::vec2(row.x + 22.0f, cy), 3.0f,
                             render::Color{0.6f, 0.85f, 1.0f, 1.0f});
                    }
                }

                // Label (indented past the check column).
                font.drawText(*renderer, row.x + 44.0f, row.y + 8.0f, it.text.c_str(), label, 0.38f);

                // Right-aligned shortcut hint.
                if (!it.shortcut.empty()) {
                    const float sw = font.textWidth(it.shortcut.c_str(), 0.34f);
                    font.drawText(*renderer, row.x + row.w - sw - 34.0f, row.y + 9.0f, it.shortcut.c_str(),
                                  it.disabled ? dim : hint, 0.34f);
                }

                // Submenu arrow.
                if (it.submenu) {
                    const float ax = row.x + row.w - 22.0f;
                    const render::Point2 tri[3] = {{ax, cy - 6.0f}, {ax, cy + 6.0f}, {ax + 7.0f, cy}};
                    renderer->drawConvexPolygon(tri, 3, txt);
                }
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("POPUPMENU shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
