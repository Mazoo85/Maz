// Maz Engine — "UILAYOUT" (retained anchor + container UI, toward Godot's Control system)
// A responsive interface laid out entirely by ui::LayoutNode: a top bar anchored across the width, a
// body that fills below it, split by an HBox into a fixed-width sidebar (a VBox of buttons) and an
// expanding content panel, plus a fixed-size modal pinned to the screen center via anchors — with the
// modal's own VBox stacking a title, body, and an HBox of OK/Cancel buttons. No pixel coordinate is
// hand-typed: change the window size and every rect recomputes. The layout is deterministic, so the
// render is golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

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

void panel(render::Renderer& r, render::TextureHandle white, const ui::Rect& rc, render::Color fill,
           render::Color border, float bw = 2.0f) {
    render::SpriteDesc d;
    d.x = rc.x;
    d.y = rc.y;
    d.width = rc.w;
    d.height = rc.h;
    d.color = fill;
    r.drawSprite(white, d);
    if (bw > 0.0f) {
        auto bar = [&](float x, float y, float w, float h) {
            render::SpriteDesc b;
            b.x = x;
            b.y = y;
            b.width = w;
            b.height = h;
            b.color = border;
            r.drawSprite(white, b);
        };
        bar(rc.x, rc.y, rc.w, bw);
        bar(rc.x, rc.bottom() - bw, rc.w, bw);
        bar(rc.x, rc.y, bw, rc.h);
        bar(rc.right() - bw, rc.y, bw, rc.h);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("UILAYOUT (retained UI layout) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — UI Layout";
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

    // --- Build the layout tree once (nodes are owned here; layout() recomputes rects each frame) ---
    using Node = ui::LayoutNode;
    Node root(Node::Mode::Anchor);

    Node topBar;
    topBar.anchorTop(56.0f, 12.0f);

    Node body(Node::Mode::HBox);
    body.setAnchors(0, 0, 1, 1);
    body.setOffsets(12.0f, 80.0f, -12.0f, -12.0f); // below the top bar, margins all round
    body.spacing = 12.0f;

    Node sidebar(Node::Mode::VBox);
    sidebar.minW = 240.0f;
    sidebar.spacing = 10.0f;
    sidebar.pad = 12.0f;

    const char* kButtons[] = {"New", "Open", "Save", "Settings", "Quit"};
    const int kNumButtons = 5;
    std::vector<Node> buttons(static_cast<size_t>(kNumButtons));
    for (int i = 0; i < kNumButtons; ++i) buttons[static_cast<size_t>(i)].minH = 46.0f;
    Node sidebarSpacer;
    sidebarSpacer.expand = true;

    Node content(Node::Mode::Anchor);
    content.expand = true;

    Node modal(Node::Mode::VBox);
    modal.setAnchors(0.5f, 0.5f, 0.5f, 0.5f);
    modal.setOffsets(-190.0f, -120.0f, 190.0f, 120.0f); // 380x240 centered
    modal.pad = 18.0f;
    modal.spacing = 14.0f;
    Node modalTitle;
    modalTitle.minH = 34.0f;
    Node modalBody;
    modalBody.expand = true;
    Node modalButtons(Node::Mode::HBox);
    modalButtons.minH = 46.0f;
    modalButtons.spacing = 14.0f;
    Node okBtn, cancelBtn;
    okBtn.expand = true;
    cancelBtn.expand = true;
    modalButtons.add(&okBtn).add(&cancelBtn);
    modal.add(&modalTitle).add(&modalBody).add(&modalButtons);

    for (int i = 0; i < kNumButtons; ++i) sidebar.add(&buttons[static_cast<size_t>(i)]);
    sidebar.add(&sidebarSpacer);
    body.add(&sidebar).add(&content);
    root.add(&topBar).add(&body).add(&modal);

    const render::Color kBg{0.09f, 0.10f, 0.13f, 1.0f};
    const render::Color kPanel{0.16f, 0.18f, 0.23f, 1.0f};
    const render::Color kPanel2{0.20f, 0.23f, 0.29f, 1.0f};
    const render::Color kBorder{0.32f, 0.36f, 0.44f, 1.0f};
    const render::Color kAccent{0.30f, 0.55f, 0.90f, 1.0f};
    const render::Color kText{0.90f, 0.93f, 0.97f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
        const float sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        // Recompute every rect for the current framebuffer size.
        root.layout(ui::Rect{0, 0, sw, sh});

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Top bar.
            panel(*renderer, white, topBar.rect, kPanel, kBorder);
            font.drawText(*renderer, topBar.rect.x + 16.0f, topBar.rect.y + 14.0f,
                          "MAZ ENGINE  -  UI LAYOUT (anchors + containers)", kText, 0.6f);

            // Sidebar + its buttons.
            panel(*renderer, white, sidebar.rect, kPanel, kBorder);
            for (int i = 0; i < kNumButtons; ++i) {
                const ui::Rect& b = buttons[static_cast<size_t>(i)].rect;
                const bool active = (i == 2); // highlight "Save"
                panel(*renderer, white, b, active ? kAccent : kPanel2, kBorder);
                font.drawTextCentered(*renderer, b.centerX(), b.centerY() - 9.0f, kButtons[i],
                                      active ? render::Color{1, 1, 1, 1} : kText, 0.5f);
            }

            // Content panel.
            panel(*renderer, white, content.rect, kPanel, kBorder);
            font.drawText(*renderer, content.rect.x + 16.0f, content.rect.y + 14.0f,
                          "Content area", kText, 0.55f);
            font.drawText(*renderer, content.rect.x + 16.0f, content.rect.y + 44.0f,
                          "fills the space the sidebar leaves - resize and it follows",
                          render::Color{0.65f, 0.72f, 0.82f, 1}, 0.42f);

            // Modal dialog (drawn last, on top).
            panel(*renderer, white, modal.rect, kPanel2, kAccent, 3.0f);
            font.drawTextCentered(*renderer, modalTitle.rect.centerX(), modalTitle.rect.y + 2.0f,
                                  "Confirm", kText, 0.62f);
            font.drawTextCentered(*renderer, modalBody.rect.centerX(),
                                  modalBody.rect.centerY() - 10.0f, "Apply your changes?",
                                  render::Color{0.8f, 0.85f, 0.92f, 1}, 0.5f);
            panel(*renderer, white, okBtn.rect, kAccent, kBorder);
            font.drawTextCentered(*renderer, okBtn.rect.centerX(), okBtn.rect.centerY() - 9.0f, "OK",
                                  render::Color{1, 1, 1, 1}, 0.52f);
            panel(*renderer, white, cancelBtn.rect, kPanel, kBorder);
            font.drawTextCentered(*renderer, cancelBtn.rect.centerX(), cancelBtn.rect.centerY() - 9.0f,
                                  "Cancel", kText, 0.52f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("UILAYOUT shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
