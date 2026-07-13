// Maz Engine — "RECTS" (Rect2 geometry, toward Godot's Rect2)
// A static gallery of the axis-aligned rectangle operations: two overlapping rects show their
// intersection (clip) filled bright; a third joins them and the whole set's merge (union) is outlined;
// one rect's grow() is shown as a faint halo; and two probe points are coloured by hasPoint(). All values
// come from math::Rect2, so the picture IS the test of the geometry. Fixed layout -> deterministic,
// golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void rectFill(render::Renderer& r, const math::Rect2& box, render::Color c) {
    const render::Point2 p[4] = {{box.left(), box.top()},
                                 {box.right(), box.top()},
                                 {box.right(), box.bottom()},
                                 {box.left(), box.bottom()}};
    r.drawConvexPolygon(p, 4, c);
}

void bar(render::Renderer& r, float x0, float y0, float x1, float y1, float w, render::Color c) {
    math::vec2 d(x1 - x0, y1 - y0);
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y * w * 0.5f, d.x * w * 0.5f);
    const render::Point2 q[4] = {{x0 + n.x, y0 + n.y},
                                 {x1 + n.x, y1 + n.y},
                                 {x1 - n.x, y1 - n.y},
                                 {x0 - n.x, y0 - n.y}};
    r.drawConvexPolygon(q, 4, c);
}

void rectOutline(render::Renderer& r, const math::Rect2& box, float w, render::Color c) {
    bar(r, box.left(), box.top(), box.right(), box.top(), w, c);
    bar(r, box.right(), box.top(), box.right(), box.bottom(), w, c);
    bar(r, box.right(), box.bottom(), box.left(), box.bottom(), w, c);
    bar(r, box.left(), box.bottom(), box.left(), box.top(), w, c);
}

void dot(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 18;
    render::Point2 pts[18];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RECTS (Rect2 geometry) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Rect2";
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

    // ---- The rectangles + derived geometry (all from math::Rect2). -----------------------------------
    const math::Rect2 a(180.0f, 190.0f, 320.0f, 220.0f);
    const math::Rect2 b(400.0f, 300.0f, 330.0f, 250.0f);
    const math::Rect2 c(800.0f, 200.0f, 230.0f, 300.0f);

    const math::Rect2 clip = a.intersection(b);        // overlap of A and B
    const math::Rect2 hull = a.merge(b).merge(c);       // union bounds of all three
    const math::Rect2 halo = a.grow(26.0f);             // A grown on all sides

    const math::vec2 pIn(300.0f, 260.0f);   // inside A
    const math::vec2 pOut(650.0f, 620.0f);  // outside every rect
    const bool inA = a.hasPoint(pIn);
    const bool outAll = !a.hasPoint(pOut) && !b.hasPoint(pOut) && !c.hasPoint(pOut);

    const render::Color kColA = rgba(0.42f, 0.62f, 1.0f, 1.0f);
    const render::Color kColB = rgba(1.0f, 0.62f, 0.36f, 1.0f);
    const render::Color kColC = rgba(0.5f, 0.85f, 0.55f, 1.0f);

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RECT2", rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "intersection (clip), merge (union), grow, and hasPoint — all from math::Rect2",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.32f);

            // grow() halo around A (faint, behind everything).
            rectOutline(*renderer, halo, 2.0f, rgba(0.42f, 0.62f, 1.0f, 0.35f));

            // The three rectangles: translucent fills + solid outlines.
            rectFill(*renderer, a, rgba(0.42f, 0.62f, 1.0f, 0.20f));
            rectFill(*renderer, b, rgba(1.0f, 0.62f, 0.36f, 0.20f));
            rectFill(*renderer, c, rgba(0.5f, 0.85f, 0.55f, 0.20f));

            // The intersection of A and B, filled bright.
            if (clip.hasArea()) {
                rectFill(*renderer, clip, rgba(1.0f, 0.92f, 0.35f, 0.85f));
            }

            rectOutline(*renderer, a, 3.0f, kColA);
            rectOutline(*renderer, b, 3.0f, kColB);
            rectOutline(*renderer, c, 3.0f, kColC);

            // The union bounds of all three (bright cyan outline enclosing everything).
            rectOutline(*renderer, hull, 2.0f, rgba(0.35f, 0.95f, 0.95f, 0.9f));

            // Probe points coloured by hasPoint().
            dot(*renderer, pIn, 8.0f, inA ? rgba(0.4f, 1.0f, 0.5f, 1) : rgba(1, 0.3f, 0.3f, 1));
            dot(*renderer, pOut, 8.0f, outAll ? rgba(1.0f, 0.4f, 0.4f, 1) : rgba(0.4f, 1.0f, 0.5f, 1));

            font.drawText(*renderer, a.left() + 8.0f, a.top() + 6.0f, "A", kColA, 0.4f);
            font.drawText(*renderer, b.right() - 24.0f, b.bottom() - 30.0f, "B", kColB, 0.4f);
            font.drawText(*renderer, c.left() + 8.0f, c.top() + 6.0f, "C", kColC, 0.4f);
            font.drawText(*renderer, clip.left() + 6.0f, clip.center().y - 10.0f, "A n B",
                          rgba(0.15f, 0.14f, 0.1f, 1), 0.32f);

            font.drawText(*renderer, 24.0f, 636.0f,
                          "cyan = merge(A,B,C) union;  yellow = A n B;  faint blue = A.grow(26)",
                          rgba(0.7f, 0.76f, 0.86f, 1), 0.3f);
            font.drawText(*renderer, 24.0f, 664.0f,
                          "green dot inside A (hasPoint true), red dot outside every rect",
                          rgba(0.6f, 0.64f, 0.72f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RECTS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
