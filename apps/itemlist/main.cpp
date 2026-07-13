// Maz Engine — "ITEMLIST" (ui::ItemList, toward Godot's ItemList control)
// Godot's ItemList is the scrollable box of choosable rows behind its FileDialog, audio-bus picker,
// inventory panels and level-select menus. This draws two of them inside StyleBoxFlat panels (M109):
//   * LEFT  — a SINGLE-select list of 14 saved games, scrolled partway down so the selected row (a
//             rounded highlight) sits mid-box, one entry disabled (dimmed), and a scrollbar thumb on the
//             right showing the scroll position + visible fraction.
//   * RIGHT — a MULTI-select equipment list where three rows are selected at once (each highlighted with
//             a check mark), the mode ItemList allows more than one active choice.
// The list logic (selection model, fixed-row geometry, scroll offset, itemRect/visibleRange) is pure
// ui::ItemList; the app just draws the rows it reports. Static content + fixed selection -> deterministic,
// golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, const ui::Rect& b, render::Color col) {
    const render::Point2 p[4] = {{b.x, b.y}, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.h}, {b.x, b.y + b.h}};
    r.drawConvexPolygon(p, 4, col);
}

void fillTri(render::Renderer& r, math::vec2 a, math::vec2 b, math::vec2 c, render::Color col) {
    const render::Point2 p[3] = {{a.x, a.y}, {b.x, b.y}, {c.x, c.y}};
    r.drawConvexPolygon(p, 3, col);
}

// A rounded selection highlight (StyleBoxFlat).
ui::StyleBoxFlat makeSelStyle() {
    ui::StyleBoxFlat s;
    s.bg = render::Color{0.20f, 0.34f, 0.54f, 1.0f};
    s.border = render::Color{0.42f, 0.64f, 0.96f, 1.0f};
    s.borderWidth = 1.5f;
    s.radius = ui::Corners{5.0f};
    return s;
}

// Draw one ItemList: panel, clipped rows (highlight selected, dim disabled), and a scrollbar thumb when
// the content overflows the box. `checks` draws a check mark on selected rows (for the multi-select list).
void drawList(render::Renderer& r, ui::Font& font, const ui::ItemList& list, const ui::StyleBoxFlat& panel,
              const ui::StyleBoxFlat& sel, bool checks) {
    ui::drawStyleBoxFlat(r, list.rect, panel);

    const auto vr = list.visibleRange();
    for (long i = vr.first; i <= vr.second; ++i) {
        const std::size_t u = static_cast<std::size_t>(i);
        const ui::ListItem& it = list.item(u);
        const ui::Rect row = list.itemRect(u);

        // Clip: this 2D path has no scissor, so draw only rows that sit FULLY inside the box (a partially
        // visible row at the top/bottom edge is dropped rather than overhanging the rounded panel).
        if (row.y < list.rect.y || row.y + row.h > list.rect.y + list.rect.h) {
            continue;
        }

        const float pad = 6.0f;
        if (it.selected) {
            ui::drawStyleBoxFlat(
                r, ui::Rect{list.rect.x + pad, row.y + 1.0f, list.rect.w - 2.0f * pad, row.h - 2.0f}, sel);
        }

        // Text: bright when selectable, dim when disabled.
        const render::Color txt = it.disabled ? render::Color{0.45f, 0.48f, 0.55f, 1.0f}
                                              : (it.selected ? render::Color{1, 1, 1, 1}
                                                             : render::Color{0.82f, 0.86f, 0.94f, 1.0f});

        float tx = list.rect.x + 18.0f;
        if (checks) {
            // Leave room for a check box on the left.
            const float cy = row.y + row.h * 0.5f;
            const render::Color boxCol =
                it.selected ? render::Color{0.42f, 0.64f, 0.96f, 1.0f} : render::Color{0.35f, 0.39f, 0.47f, 1.0f};
            fillRect(r, ui::Rect{list.rect.x + 12.0f, cy - 7.0f, 14.0f, 14.0f}, boxCol);
            fillRect(r, ui::Rect{list.rect.x + 14.0f, cy - 5.0f, 10.0f, 10.0f},
                     render::Color{0.12f, 0.14f, 0.18f, 1.0f});
            if (it.selected) {
                // A little check tick.
                fillTri(r, math::vec2(list.rect.x + 15.0f, cy + 1.0f),
                        math::vec2(list.rect.x + 18.0f, cy + 4.0f),
                        math::vec2(list.rect.x + 17.0f, cy + 5.0f),
                        render::Color{0.6f, 0.9f, 1.0f, 1.0f});
                fillTri(r, math::vec2(list.rect.x + 17.0f, cy + 5.0f),
                        math::vec2(list.rect.x + 23.0f, cy - 3.0f),
                        math::vec2(list.rect.x + 18.0f, cy + 4.0f),
                        render::Color{0.6f, 0.9f, 1.0f, 1.0f});
            }
            tx = list.rect.x + 34.0f;
        }

        font.drawText(r, tx, row.y + 4.0f, it.text.c_str(), txt, 0.36f);
    }

    // Scrollbar thumb (only when content overflows).
    const float content = list.contentHeight();
    if (content > list.rect.h) {
        const float trackX = list.rect.x + list.rect.w - 8.0f;
        const float trackY = list.rect.y + 4.0f;
        const float trackH = list.rect.h - 8.0f;
        fillRect(r, ui::Rect{trackX, trackY, 4.0f, trackH}, render::Color{0.16f, 0.18f, 0.24f, 1.0f});
        const float frac = list.rect.h / content;            // visible fraction
        const float thumbH = trackH * frac;
        const float pos = (list.maxScroll() > 0.0f) ? (list.scroll() / list.maxScroll()) : 0.0f;
        const float thumbY = trackY + (trackH - thumbH) * pos;
        fillRect(r, ui::Rect{trackX, thumbY, 4.0f, thumbH}, render::Color{0.45f, 0.55f, 0.75f, 1.0f});
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ITEMLIST (ui::ItemList) starting");

    // --- LEFT: single-select saved-games list, scrolled to show the selection mid-box. ---
    ui::ItemList saves;
    saves.rect = ui::Rect{60.0f, 130.0f, 440.0f, 460.0f};
    saves.itemHeight = 34.0f;
    saves.separation = 4.0f;
    saves.selectMode = ui::ItemSelectMode::Single;
    const char* saveNames[] = {
        "Autosave - Sector 12",   "Quicksave",              "Chapter 1 - Landing",
        "Chapter 2 - The Vault",  "Chapter 3 - Downpour",   "Chapter 4 - Cold Harbor",
        "Chapter 5 - The Spire",  "Corrupted slot",         "Ironman - Day 47",
        "Ironman - Day 51",       "Sandbox - Big Base",     "Sandbox - Test Arena",
        "Cloud sync (offline)",   "New game +",
    };
    for (const char* n : saveNames) {
        saves.addItem(n);
    }
    saves.setDisabled(7, true);  // "Corrupted slot" can't be loaded
    saves.setDisabled(12, true); // "Cloud sync (offline)"
    saves.select(4);             // "Chapter 3 - Downpour"
    saves.ensureVisible(4);
    saves.setScroll(saves.scroll() + 40.0f); // nudge so the selection sits comfortably mid-box

    // --- RIGHT: multi-select equipment list, three rows active at once. ---
    ui::ItemList gear;
    gear.rect = ui::Rect{560.0f, 130.0f, 400.0f, 300.0f};
    gear.itemHeight = 34.0f;
    gear.separation = 4.0f;
    gear.selectMode = ui::ItemSelectMode::Multi;
    const char* gearNames[] = {
        "Combat knife", "Medkit x3", "Frag grenade x2", "Lockpick set",
        "Night vision", "Rope (20m)", "Rations x5",
    };
    for (const char* n : gearNames) {
        gear.addItem(n);
    }
    gear.select(1); // Medkit
    gear.select(4); // Night vision
    gear.select(6); // Rations

    ui::StyleBoxFlat panelStyle;
    panelStyle.bg = render::Color{0.11f, 0.13f, 0.17f, 1.0f};
    panelStyle.border = render::Color{0.28f, 0.32f, 0.42f, 1.0f};
    panelStyle.borderWidth = 1.5f;
    panelStyle.radius = ui::Corners{10.0f};
    panelStyle.shadow = render::Color{0.0f, 0.0f, 0.0f, 0.35f};
    panelStyle.shadowSize = 14.0f;
    panelStyle.shadowOffset = math::vec2(0.0f, 6.0f);

    const ui::StyleBoxFlat selStyle = makeSelStyle();

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — ItemList";
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

        renderer->setClearColor(render::Color{0.06f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ITEMLIST",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "scrollable, selectable rows - Godot's file lists / inventory / level-select "
                          "control (ui::ItemList)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            font.drawText(*renderer, 60.0f, 100.0f, "SINGLE-SELECT  (load game)",
                          render::Color{0.7f, 0.8f, 1.0f, 1}, 0.4f);
            drawList(*renderer, font, saves, panelStyle, selStyle, false);

            font.drawText(*renderer, 560.0f, 100.0f, "MULTI-SELECT  (loadout)",
                          render::Color{0.7f, 0.8f, 1.0f, 1}, 0.4f);
            drawList(*renderer, font, gear, panelStyle, selStyle, true);

            // Side note.
            const char* notes[] = {
                "* each row: text + id + selectable/disabled",
                "  flags; Single mode is a radio (one active),",
                "  Multi lets several rows toggle on at once.",
                "",
                "* fixed row height + separation + a scroll",
                "  offset give itemRect(i), itemAtPoint() for",
                "  clicks, and visibleRange() for the view.",
                "",
                "* disabled rows (dimmed) can't be selected and",
                "  keyboard nav skips them; the scrollbar thumb",
                "  shows scroll position + visible fraction.",
            };
            for (std::size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
                font.drawText(*renderer, 560.0f, 470.0f + static_cast<float>(i) * 22.0f, notes[i],
                              render::Color{0.72f, 0.76f, 0.85f, 1}, 0.3f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ITEMLIST shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
