// Maz Engine — "TREE" (Tree / TreeItem widget, toward Godot's Tree control)
// Godot's scene dock, inspector, and FileSystem dock are all Trees: a hierarchy of collapsible rows. This
// draws a project file tree (ui::Tree) inside a StyleBoxFlat panel (M109): each visible row is indented by
// its depth, folders get a fold arrow (right = collapsed, down = expanded) and a warm tint, files a
// neutral tint, and the selected row gets a rounded highlight. Two folders start collapsed, so their
// subtrees are hidden — flip a `collapsed` flag and the whole branch appears or vanishes. Static tree +
// fixed selection -> deterministic, golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;

namespace {

const render::Color kFolder{0.95f, 0.82f, 0.5f, 1.0f};
const render::Color kFile{0.78f, 0.82f, 0.9f, 1.0f};

void fillTri(render::Renderer& r, math::vec2 a, math::vec2 b, math::vec2 c, render::Color col) {
    const render::Point2 p[3] = {{a.x, a.y}, {b.x, b.y}, {c.x, c.y}};
    r.drawConvexPolygon(p, 3, col);
}

ui::TreeItem& folder(ui::TreeItem& parent, const std::string& name) {
    ui::TreeItem& f = parent.addChild(name + "/");
    f.color = kFolder;
    return f;
}

void file(ui::TreeItem& parent, const std::string& name) {
    ui::TreeItem& f = parent.addChild(name);
    f.color = kFile;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TREE (Tree/TreeItem widget) starting");

    // Build a project file tree.
    ui::Tree tree;
    ui::TreeItem& res = folder(tree.root, "project");
    res.color = kFolder;
    {
        ui::TreeItem& scenes = folder(res, "scenes");
        file(scenes, "Main.scene");
        file(scenes, "Player.scene");
        ui::TreeItem& enemies = folder(scenes, "enemies");
        file(enemies, "Slime.scene");
        file(enemies, "Boss.scene");
        enemies.collapsed = true; // start collapsed -> Slime/Boss hidden

        ui::TreeItem& scripts = folder(res, "scripts");
        file(scripts, "player.maz");
        file(scripts, "enemy.maz");
        file(scripts, "world.maz");

        ui::TreeItem& assets = folder(res, "assets");
        file(assets, "hero.png");
        file(assets, "tiles.png");
        file(assets, "theme.ttf");
        assets.collapsed = true; // start collapsed

        file(res, "project.cfg");
    }
    tree.selected = 3; // highlight "Player.scene" (row index in the visible list)

    // A rounded highlight style for the selected row (StyleBoxFlat, M109).
    ui::StyleBoxFlat selStyle;
    selStyle.bg = render::Color{0.22f, 0.34f, 0.52f, 1.0f};
    selStyle.border = render::Color{0.45f, 0.65f, 0.95f, 1.0f};
    selStyle.borderWidth = 1.5f;
    selStyle.radius = ui::Corners{5.0f};

    ui::StyleBoxFlat panelStyle;
    panelStyle.bg = render::Color{0.11f, 0.13f, 0.17f, 1.0f};
    panelStyle.border = render::Color{0.28f, 0.32f, 0.42f, 1.0f};
    panelStyle.borderWidth = 1.5f;
    panelStyle.radius = ui::Corners{10.0f};
    panelStyle.shadow = render::Color{0.0f, 0.0f, 0.0f, 0.35f};
    panelStyle.shadowSize = 14.0f;
    panelStyle.shadowOffset = math::vec2(0.0f, 6.0f);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Tree Widget";
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TREE WIDGET",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "a hierarchy of collapsible rows - Godot's scene dock / inspector / file "
                          "browser control (ui::Tree)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            // Panel.
            const ui::Rect panel{60.0f, 110.0f, 520.0f, 540.0f};
            ui::drawStyleBoxFlat(*renderer, panel, panelStyle);

            const float rowH = 34.0f, x0 = panel.x + 20.0f, y0 = panel.y + 22.0f, indent = 26.0f;
            const auto rows = tree.visibleRows();
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const ui::TreeRow& row = rows[i];
                const float ry = y0 + static_cast<float>(i) * rowH;
                const float rx = x0 + static_cast<float>(row.depth) * indent;

                // Selection highlight spanning the panel width.
                if (static_cast<int>(i) == tree.selected) {
                    ui::drawStyleBoxFlat(
                        *renderer,
                        ui::Rect{panel.x + 8.0f, ry - 4.0f, panel.w - 16.0f, rowH - 4.0f}, selStyle);
                }

                // Fold arrow for items with children.
                if (row.hasChildren) {
                    const float ax = rx - 16.0f, ay = ry + 9.0f;
                    if (row.collapsed) { // right-pointing triangle
                        fillTri(*renderer, math::vec2(ax, ay - 5.0f), math::vec2(ax, ay + 5.0f),
                                math::vec2(ax + 7.0f, ay), render::Color{0.7f, 0.75f, 0.85f, 1.0f});
                    } else { // down-pointing triangle
                        fillTri(*renderer, math::vec2(ax - 4.0f, ay - 3.0f),
                                math::vec2(ax + 6.0f, ay - 3.0f), math::vec2(ax + 1.0f, ay + 4.0f),
                                render::Color{0.7f, 0.75f, 0.85f, 1.0f});
                    }
                }

                // A small square "icon": filled for folders, hollow for files.
                const bool isFolder = row.hasChildren || (!row.item->text.empty() &&
                                                          row.item->text.back() == '/');
                const render::Color ic = isFolder ? kFolder : kFile;
                const render::Point2 sq[4] = {{rx, ry + 4.0f},
                                              {rx + 12.0f, ry + 4.0f},
                                              {rx + 12.0f, ry + 16.0f},
                                              {rx, ry + 16.0f}};
                renderer->drawConvexPolygon(sq, 4, isFolder ? ic
                                                            : render::Color{0.3f, 0.34f, 0.42f, 1.0f});
                if (!isFolder) {
                    const render::Point2 in[4] = {{rx + 3.0f, ry + 7.0f},
                                                  {rx + 9.0f, ry + 7.0f},
                                                  {rx + 9.0f, ry + 13.0f},
                                                  {rx + 3.0f, ry + 13.0f}};
                    renderer->drawConvexPolygon(in, 4, ic);
                }

                font.drawText(*renderer, rx + 22.0f, ry, row.item->text.c_str(), row.item->color, 0.4f);
            }

            // Side note.
            font.drawText(*renderer, 620.0f, 130.0f, "how it works",
                          render::Color{0.7f, 0.8f, 1.0f, 1}, 0.44f);
            const char* notes[] = {
                "* each TreeItem holds text + children + a",
                "  'collapsed' flag; visibleRows() flattens",
                "  the expanded items depth-first into rows.",
                "",
                "* a row carries its DEPTH (indent) and whether",
                "  it hasChildren (draw a fold arrow).",
                "",
                "* folding a branch hides its whole subtree in",
                "  one flag - here 'enemies/' and 'assets/'",
                "  are collapsed, so their files don't show.",
                "",
                "* the selected row gets a rounded StyleBoxFlat",
                "  highlight (composed with M109's theming).",
            };
            for (std::size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
                font.drawText(*renderer, 620.0f, 172.0f + static_cast<float>(i) * 26.0f, notes[i],
                              render::Color{0.75f, 0.79f, 0.88f, 1}, 0.32f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TREE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
