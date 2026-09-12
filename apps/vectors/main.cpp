// Maz Engine — "VECTORS" (filled 2D polygons, toward Godot's Polygon2D / draw_colored_polygon)
// The renderer gained drawConvexPolygon: an arbitrary convex polygon, triangulated on the fly and
// flat-shaded, streamed through the same batched 2D pipeline as sprites. This demo draws a row of
// regular N-gons (triangle through octagon), a many-sided disc (a polygon approximating a circle),
// and three overlapping translucent triangles to show alpha compositing — none of which the old
// quad-only sprite path could do. The scene is static, so the render is golden-stable.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

// A regular polygon of `sides` around (cx,cy), radius r, with a start-angle offset.
std::vector<render::Point2> regularPolygon(float cx, float cy, float r, int sides, float rot) {
    std::vector<render::Point2> pts;
    pts.reserve(static_cast<size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const float a = rot + 6.28318530718f * static_cast<float>(i) / static_cast<float>(sides);
        pts.push_back(render::Point2{cx + std::cos(a) * r, cy + std::sin(a) * r});
    }
    return pts;
}

render::Color hueColor(float h, float a = 1.0f) {
    h = h - std::floor(h);
    const float r = std::fabs(h * 6.0f - 3.0f) - 1.0f;
    const float g = 2.0f - std::fabs(h * 6.0f - 2.0f);
    const float b = 2.0f - std::fabs(h * 6.0f - 4.0f);
    auto cl = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    return render::Color{cl(r), cl(g), cl(b), a};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("VECTORS (filled polygons) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Polygons";
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

    const float sw = static_cast<float>(cfg.width);

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

            // Row of regular N-gons: triangle (3) .. octagon (8).
            const int firstSides = 3, lastSides = 8;
            const int n = lastSides - firstSides + 1;
            const float gap = sw / static_cast<float>(n + 1);
            for (int s = firstSides; s <= lastSides; ++s) {
                const int idx = s - firstSides;
                const float cx = gap * static_cast<float>(idx + 1);
                const float cy = 210.0f;
                const auto poly =
                    regularPolygon(cx, cy, 66.0f, s, -1.5708f /* point up */);
                renderer->drawConvexPolygon(poly.data(), static_cast<uint32_t>(poly.size()),
                                            hueColor(static_cast<float>(idx) / static_cast<float>(n)));
                char lbl[8];
                std::snprintf(lbl, sizeof(lbl), "%d", s);
                font.drawTextCentered(*renderer, cx, cy + 84.0f, lbl,
                                      render::Color{0.8f, 0.85f, 0.92f, 1}, 0.5f);
            }

            // A 64-sided polygon approximating a filled circle.
            {
                const auto circle = regularPolygon(gap * 2.0f, 470.0f, 90.0f, 64, 0.0f);
                renderer->drawConvexPolygon(circle.data(), static_cast<uint32_t>(circle.size()),
                                            render::Color{0.45f, 0.7f, 1.0f, 1.0f});
                font.drawTextCentered(*renderer, gap * 2.0f, 578.0f, "64-gon (circle)",
                                      render::Color{0.7f, 0.78f, 0.88f, 1}, 0.42f);
            }

            // Three overlapping translucent triangles -> alpha compositing (a quad-only path can't do
            // arbitrary triangles).
            {
                const float bx = sw * 0.62f, by = 470.0f, r = 130.0f;
                const render::Point2 t1[3] = {{bx, by - r},
                                              {bx - r * 0.87f, by + r * 0.5f},
                                              {bx + r * 0.87f, by + r * 0.5f}};
                const render::Point2 t2[3] = {{bx + r, by},
                                              {bx - r * 0.5f, by - r * 0.87f},
                                              {bx - r * 0.5f, by + r * 0.87f}};
                const render::Point2 t3[3] = {{bx - r, by},
                                              {bx + r * 0.5f, by - r * 0.87f},
                                              {bx + r * 0.5f, by + r * 0.87f}};
                renderer->drawConvexPolygon(t1, 3, render::Color{1.0f, 0.3f, 0.3f, 0.55f});
                renderer->drawConvexPolygon(t2, 3, render::Color{0.3f, 1.0f, 0.4f, 0.55f});
                renderer->drawConvexPolygon(t3, 3, render::Color{0.4f, 0.5f, 1.0f, 0.55f});
                font.drawTextCentered(*renderer, bx, by + r + 24.0f, "translucent overlap",
                                      render::Color{0.7f, 0.78f, 0.88f, 1}, 0.42f);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  FILLED POLYGONS (drawConvexPolygon)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "arbitrary convex polygons, triangulated + batched like sprites",
                          render::Color{0.7f, 0.85f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("VECTORS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
