// Maz Engine — "CONTAINERS" (auto-layout container controls, toward Godot's Container nodes)
// Four labeled cards, each demonstrating one ui::Container layout with NO hand-typed child coordinates —
// every tile's rect is computed by the container from the card's inner area + the tiles' size flags:
//   * HBoxContainer  — two Fill tiles bracket two Expand tiles that split the leftover 1:2 by stretch;
//   * GridContainer  — nine tiles flow into 3 columns, all Expand so the grid fills the card evenly;
//   * VBoxContainer  — a fixed header + an Expand body + a fixed footer stack to fill the height;
//   * Margin+Center  — a MarginContainer insets the card, and a CenterContainer pins a fixed box mid-way.
// Layout is computed once and drawn statically, so the render is deterministic + golden-stable.
// Resize the window and every rect recomputes. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, const ui::Rect& rc, render::Color c) {
    render::SpriteDesc d;
    d.x = rc.x;
    d.y = rc.y;
    d.width = rc.w;
    d.height = rc.h;
    d.color = c;
    r.drawSprite(white, d);
}

void panel(render::Renderer& r, render::TextureHandle white, const ui::Rect& rc, render::Color fill,
           render::Color border, float bw = 2.0f) {
    fillRect(r, white, rc, fill);
    if (bw > 0.0f) {
        fillRect(r, white, {rc.x, rc.y, rc.w, bw}, border);
        fillRect(r, white, {rc.x, rc.bottom() - bw, rc.w, bw}, border);
        fillRect(r, white, {rc.x, rc.y, bw, rc.h}, border);
        fillRect(r, white, {rc.right() - bw, rc.y, bw, rc.h}, border);
    }
}

// A colored tile with a small caption in its top-left — used to make each computed rect visible.
void tile(render::Renderer& r, render::TextureHandle white, ui::Font& font, const ui::Rect& rc,
          render::Color fill, const char* label) {
    panel(r, white, rc, fill, render::Color{1, 1, 1, 0.28f}, 1.5f);
    font.drawText(r, rc.x + 8.0f, rc.y + 6.0f, label, render::Color{1, 1, 1, 0.95f}, 0.30f);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CONTAINERS (auto-layout containers) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Auto-layout Containers";
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
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    const render::Color kTileA{0.30f, 0.52f, 0.85f, 1.0f};
    const render::Color kTileB{0.85f, 0.55f, 0.28f, 1.0f};
    const render::Color kTileC{0.35f, 0.72f, 0.52f, 1.0f};
    const render::Color kCard{0.13f, 0.15f, 0.19f, 1.0f};
    const render::Color kBorder{0.34f, 0.38f, 0.46f, 1.0f};

    // Card rectangles (2x2). Each card has a title strip + an inner content area the container fills.
    struct Card {
        ui::Rect rc;
        const char* title;
    };
    const Card cards[4] = {
        {{16.0f, 104.0f, 610.0f, 288.0f}, "HBoxContainer  -  Fill | Expand x1 | Expand x2 | Fill"},
        {{642.0f, 104.0f, 622.0f, 288.0f}, "GridContainer  -  9 tiles, 3 columns, all Expand"},
        {{16.0f, 408.0f, 610.0f, 288.0f}, "VBoxContainer  -  header | body (Expand) | footer"},
        {{642.0f, 408.0f, 622.0f, 288.0f}, "MarginContainer + CenterContainer"},
    };

    auto innerOf = [](const ui::Rect& card) {
        return ui::Rect{card.x + 14.0f, card.y + 48.0f, card.w - 28.0f, card.h - 62.0f};
    };

    // --- HBox card: two Fill tiles bracket two Expand tiles (stretch 1 and 2) ---------------------
    ui::Control hA, hB, hC, hD;
    hA.minW = 90.0f;
    hB.hFlag = ui::SizeFlag::Expand;
    hB.stretch = 1.0f;
    hC.hFlag = ui::SizeFlag::Expand;
    hC.stretch = 2.0f;
    hD.minW = 90.0f;
    std::vector<ui::Control*> hKids{&hA, &hB, &hC, &hD};
    ui::hbox(innerOf(cards[0].rc), hKids, 12.0f);

    // --- Grid card: 9 tiles, 3 columns, every tile Expands on both axes so the grid fills evenly ---
    ui::Control gTiles[9];
    std::vector<ui::Control*> gKids;
    for (auto& t : gTiles) {
        t.hFlag = ui::SizeFlag::Expand;
        t.vFlag = ui::SizeFlag::Expand;
        gKids.push_back(&t);
    }
    ui::grid(innerOf(cards[1].rc), gKids, 3, 12.0f, 12.0f);

    // --- VBox card: fixed header + expanding body + fixed footer ----------------------------------
    ui::Control vHead, vBody, vFoot;
    vHead.minH = 44.0f;
    vBody.vFlag = ui::SizeFlag::Expand;
    vFoot.minH = 44.0f;
    std::vector<ui::Control*> vKids{&vHead, &vBody, &vFoot};
    ui::vbox(innerOf(cards[2].rc), vKids, 12.0f);

    // --- Margin + Center card ---------------------------------------------------------------------
    ui::Control marginChild;
    ui::margin(innerOf(cards[3].rc), marginChild, 28.0f, 28.0f, 28.0f, 28.0f);
    ui::Control centerBox;
    centerBox.minW = 220.0f;
    centerBox.minH = 96.0f;
    ui::center(marginChild.rect, centerBox);

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  AUTO-LAYOUT CONTAINERS",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "size flags + stretch ratios + grid + margin/center (ui::Container)",
                          render::Color{0.78f, 0.83f, 0.93f, 1}, 0.34f);

            for (const Card& c : cards) {
                panel(*renderer, white, c.rc, kCard, kBorder, 2.0f);
                font.drawText(*renderer, c.rc.x + 14.0f, c.rc.y + 12.0f, c.title,
                              render::Color{0.86f, 0.9f, 0.98f, 1}, 0.3f);
            }

            // HBox tiles.
            tile(*renderer, white, font, hA.rect, kTileA, "Fill");
            tile(*renderer, white, font, hB.rect, kTileB, "Expand x1");
            tile(*renderer, white, font, hC.rect, kTileB, "Expand x2");
            tile(*renderer, white, font, hD.rect, kTileA, "Fill");

            // Grid tiles.
            for (std::size_t i = 0; i < 9; ++i) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%zu", i + 1);
                tile(*renderer, white, font, gTiles[i].rect, kTileC, buf);
            }

            // VBox tiles.
            tile(*renderer, white, font, vHead.rect, kTileA, "header");
            tile(*renderer, white, font, vBody.rect, kTileB, "body (Expand)");
            tile(*renderer, white, font, vFoot.rect, kTileA, "footer");

            // Margin frame (dashed look via a thin outline) + centered box.
            panel(*renderer, white, marginChild.rect, render::Color{0.10f, 0.12f, 0.15f, 1.0f},
                  render::Color{0.5f, 0.55f, 0.62f, 0.7f}, 1.5f);
            font.drawText(*renderer, marginChild.rect.x + 8.0f, marginChild.rect.y + 6.0f,
                          "margin inset", render::Color{0.7f, 0.75f, 0.82f, 1}, 0.28f);
            tile(*renderer, white, font, centerBox.rect, kTileC, "centered box");

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CONTAINERS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
