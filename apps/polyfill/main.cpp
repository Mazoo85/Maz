// Maz Engine — "POLYFILL" (render::triangulatePolygon, toward Godot's Polygon2D fill)
// Maz could already fill CONVEX polygons (drawConvexPolygon fans from vertex 0), but a fan spills
// triangles outside any CONCAVE outline. Ear-clipping triangulation fixes that: it tiles an arbitrary
// simple polygon into triangles, each of which the convex-fill path can draw. This demo fills four
// concave shapes a fan cannot — a five-point star, a block arrow, a plus/cross, and a thick C-ring —
// and overlays the triangle mesh (faint) + outline (bright) so the triangulation is visible. Fixed
// shapes -> deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr float kPi = 3.14159265358979f;

void thickLine(render::Renderer& r, math::vec2 a, math::vec2 b, float w, render::Color c) {
    math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y * w * 0.5f, d.x * w * 0.5f);
    const render::Point2 q[4] = {{a.x + n.x, a.y + n.y},
                                 {b.x + n.x, b.y + n.y},
                                 {b.x - n.x, b.y - n.y},
                                 {a.x - n.x, a.y - n.y}};
    r.drawConvexPolygon(q, 4, c);
}

// Fill an arbitrary simple polygon by ear-clipping it and drawing each triangle through the convex path.
void fillPolygon(render::Renderer& r, const std::vector<math::vec2>& poly, render::Color fill) {
    const std::vector<std::uint32_t> idx = render::triangulatePolygon(poly);
    for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
        const render::Point2 t[3] = {{poly[idx[i]].x, poly[idx[i]].y},
                                     {poly[idx[i + 1]].x, poly[idx[i + 1]].y},
                                     {poly[idx[i + 2]].x, poly[idx[i + 2]].y}};
        r.drawConvexPolygon(t, 3, fill);
    }
}

void wireTriangles(render::Renderer& r, const std::vector<math::vec2>& poly, render::Color c) {
    const std::vector<std::uint32_t> idx = render::triangulatePolygon(poly);
    for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
        thickLine(r, poly[idx[i]], poly[idx[i + 1]], 1.0f, c);
        thickLine(r, poly[idx[i + 1]], poly[idx[i + 2]], 1.0f, c);
        thickLine(r, poly[idx[i + 2]], poly[idx[i]], 1.0f, c);
    }
}

void outline(render::Renderer& r, const std::vector<math::vec2>& poly, float w, render::Color c) {
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        thickLine(r, poly[i], poly[(i + 1) % n], w, c);
    }
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

// --- concave shape generators (centred at c, in pixel space) --------------------------------------

std::vector<math::vec2> star(math::vec2 c, float outer, float inner, int points) {
    std::vector<math::vec2> v;
    for (int i = 0; i < points * 2; ++i) {
        const float ang = -kPi * 0.5f + kPi * static_cast<float>(i) / static_cast<float>(points);
        const float rad = (i % 2 == 0) ? outer : inner;
        v.push_back({c.x + std::cos(ang) * rad, c.y + std::sin(ang) * rad});
    }
    return v;
}

std::vector<math::vec2> arrow(math::vec2 c, float s) {
    // Right-pointing block arrow: shaft rectangle + triangular head (concave at the head/shaft joins).
    const float pts[7][2] = {{-1.0f, -0.35f}, {0.15f, -0.35f}, {0.15f, -0.8f}, {1.0f, 0.0f},
                             {0.15f, 0.8f},   {0.15f, 0.35f},  {-1.0f, 0.35f}};
    std::vector<math::vec2> v;
    for (const auto& p : pts) {
        v.push_back({c.x + p[0] * s, c.y + p[1] * s});
    }
    return v;
}

std::vector<math::vec2> plus(math::vec2 c, float s) {
    // 12-vertex cross, arm half-width 1/3 of the reach.
    const float a = s / 3.0f;
    const float pts[12][2] = {{-a, -s}, {a, -s}, {a, -a}, {s, -a}, {s, a},  {a, a},
                              {a, s},   {-a, s}, {-a, a}, {-s, a}, {-s, -a}, {-a, -a}};
    std::vector<math::vec2> v;
    for (const auto& p : pts) {
        v.push_back({c.x + p[0], c.y + p[1]});
    }
    return v;
}

std::vector<math::vec2> cRing(math::vec2 c, float outer, float inner, float gapDeg) {
    // A thick "C": outer arc across the top, inner arc back — a simple concave polygon (no hole).
    std::vector<math::vec2> v;
    const int seg = 22;
    const float a0 = gapDeg * kPi / 180.0f;
    const float a1 = (360.0f - gapDeg) * kPi / 180.0f;
    for (int i = 0; i <= seg; ++i) {
        const float t = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(seg);
        v.push_back({c.x + std::cos(t) * outer, c.y + std::sin(t) * outer});
    }
    for (int i = 0; i <= seg; ++i) {
        const float t = a1 + (a0 - a1) * static_cast<float>(i) / static_cast<float>(seg);
        v.push_back({c.x + std::cos(t) * inner, c.y + std::sin(t) * inner});
    }
    return v;
}

struct Panel {
    std::string title;
    std::vector<math::vec2> shape;
    render::Color color;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("POLYFILL (render::triangulatePolygon) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Polygon Fill";
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

    const float colX[2] = {340.0f, 940.0f};
    const float rowY[2] = {235.0f, 520.0f};

    std::vector<Panel> panels;
    panels.push_back({"5-point star", star({colX[0], rowY[0]}, 120.0f, 48.0f, 5),
                      rgba(0.55f, 0.8f, 1.0f, 1)});
    panels.push_back({"block arrow", arrow({colX[1], rowY[0]}, 120.0f), rgba(0.6f, 0.95f, 0.65f, 1)});
    panels.push_back({"plus / cross", plus({colX[0], rowY[1]}, 115.0f), rgba(1.0f, 0.8f, 0.45f, 1)});
    panels.push_back(
        {"C-ring (thick arc)", cRing({colX[1], rowY[1]}, 120.0f, 62.0f, 55.0f), rgba(1.0f, 0.6f, 0.75f, 1)});

    const render::Color kMesh{1.0f, 1.0f, 1.0f, 0.16f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  POLYGON FILL", rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "concave shapes ear-clipped into triangles (render::triangulatePolygon, "
                          "Godot Polygon2D) — a convex fan cannot fill these",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.32f);

            for (const Panel& p : panels) {
                fillPolygon(*renderer, p.shape, p.color);
                wireTriangles(*renderer, p.shape, kMesh);       // reveal the triangle mesh
                outline(*renderer, p.shape, 2.0f, rgba(1, 1, 1, 0.9f));
            }

            // Labels under each shape.
            font.drawText(*renderer, colX[0] - 150.0f, rowY[0] + 150.0f, panels[0].title.c_str(),
                          panels[0].color, 0.34f);
            font.drawText(*renderer, colX[1] - 150.0f, rowY[0] + 150.0f, panels[1].title.c_str(),
                          panels[1].color, 0.34f);
            font.drawText(*renderer, colX[0] - 150.0f, rowY[1] + 150.0f, panels[2].title.c_str(),
                          panels[2].color, 0.34f);
            font.drawText(*renderer, colX[1] - 150.0f, rowY[1] + 150.0f, panels[3].title.c_str(),
                          panels[3].color, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("POLYFILL shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
