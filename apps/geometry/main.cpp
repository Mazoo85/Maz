// Maz Engine — "GEOMETRY" (math::Geometry2D helpers, toward Godot's Geometry2D class)
// The workhorse 2D geometry queries behind AI line-of-sight, mouse picking, and trigger zones. This
// demo shows three of them: LEFT, a web of segments with every pairwise intersection marked (red dots);
// CENTRE, a concave polygon with a grid of test points coloured green (inside) / grey (outside) by the
// even-odd point-in-polygon test; RIGHT, a query point with a projection line to the closest point on
// each of several segments, plus a segment coloured by whether it crosses a circle. Fixed geometry ->
// deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace maz;

namespace {

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

void dot(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 12;
    render::Point2 pts[12];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

void ring(render::Renderer& r, math::vec2 c, float radius, float w, render::Color col) {
    const int n = 48;
    math::vec2 prev;
    for (int i = 0; i <= n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        const math::vec2 p(c.x + std::cos(a) * radius, c.y + std::sin(a) * radius);
        if (i > 0) {
            thickLine(r, prev, p, w, col);
        }
        prev = p;
    }
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

void panelBox(render::Renderer& r, float x0, float y0, float x1, float y1) {
    const render::Point2 q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    r.drawConvexPolygon(q, 4, rgba(0.12f, 0.13f, 0.17f, 1));
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GEOMETRY (math::Geometry2D) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Geometry2D";
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

    using math::vec2;

    // Panel 1: a web of segments.
    const std::vector<std::pair<vec2, vec2>> segs = {
        {vec2(60, 180), vec2(400, 520)}, {vec2(60, 520), vec2(400, 180)},
        {vec2(60, 350), vec2(400, 350)}, {vec2(230, 160), vec2(230, 540)},
        {vec2(90, 200), vec2(380, 480)},
    };

    // Panel 2: a concave arrow polygon (centre panel), plus a grid of test points.
    const vec2 c2(640.0f, 350.0f);
    const std::vector<vec2> poly = {
        {c2.x - 130, c2.y - 40}, {c2.x + 10, c2.y - 40}, {c2.x + 10, c2.y - 95}, {c2.x + 130, c2.y},
        {c2.x + 10, c2.y + 95},  {c2.x + 10, c2.y + 40}, {c2.x - 130, c2.y + 40},
    };

    // Panel 3: a query point + segments + a circle.
    const vec2 query(1040.0f, 350.0f);
    const std::vector<std::pair<vec2, vec2>> segs3 = {
        {vec2(880, 200), vec2(1040, 200)},
        {vec2(1180, 240), vec2(1120, 500)},
        {vec2(900, 500), vec2(1120, 470)},
    };
    const vec2 circleC(960.0f, 470.0f);
    const float circleR = 46.0f;
    const std::pair<vec2, vec2> circleSeg = {vec2(890, 540), vec2(1010, 400)};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  GEOMETRY2D", rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "segment intersection / point-in-polygon / closest-point + circle "
                          "(math::Geometry2D, Godot Geometry2D)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // ---- Panel 1: segment intersections ----------------------------------------------------
            panelBox(*renderer, 40.0f, 120.0f, 420.0f, 560.0f);
            for (const auto& s : segs) {
                thickLine(*renderer, s.first, s.second, 2.0f, rgba(0.55f, 0.7f, 0.95f, 1));
            }
            for (std::size_t i = 0; i < segs.size(); ++i) {
                for (std::size_t j = i + 1; j < segs.size(); ++j) {
                    const math::SegmentHit h =
                        math::segmentIntersect(segs[i].first, segs[i].second, segs[j].first,
                                               segs[j].second);
                    if (h.hit) {
                        dot(*renderer, h.point, 4.0f, rgba(1.0f, 0.4f, 0.4f, 1));
                    }
                }
            }
            font.drawText(*renderer, 48.0f, 96.0f, "segment intersections", rgba(0.75f, 0.8f, 0.9f, 1),
                          0.3f);

            // ---- Panel 2: point in polygon ---------------------------------------------------------
            panelBox(*renderer, 450.0f, 120.0f, 830.0f, 560.0f);
            const std::size_t pn = poly.size();
            for (std::size_t i = 0; i < pn; ++i) {
                thickLine(*renderer, poly[i], poly[(i + 1) % pn], 2.0f, rgba(0.6f, 0.95f, 0.65f, 1));
            }
            for (float y = 150.0f; y <= 550.0f; y += 22.0f) {
                for (float x = 470.0f; x <= 820.0f; x += 22.0f) {
                    const vec2 p(x, y);
                    const bool inside = math::pointInPolygon(p, poly);
                    dot(*renderer, p, 2.4f,
                        inside ? rgba(0.5f, 0.95f, 0.6f, 1) : rgba(0.4f, 0.43f, 0.5f, 1));
                }
            }
            font.drawText(*renderer, 458.0f, 96.0f, "point-in-polygon (green = inside)",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            // ---- Panel 3: closest point + circle ---------------------------------------------------
            panelBox(*renderer, 860.0f, 120.0f, 1240.0f, 560.0f);
            for (const auto& s : segs3) {
                thickLine(*renderer, s.first, s.second, 2.0f, rgba(0.55f, 0.7f, 0.95f, 1));
                const vec2 cp = math::closestPointOnSegment(query, s.first, s.second);
                thickLine(*renderer, query, cp, 1.0f, rgba(1.0f, 0.8f, 0.45f, 0.8f));
                dot(*renderer, cp, 3.0f, rgba(1.0f, 0.8f, 0.45f, 1));
            }
            dot(*renderer, query, 5.0f, rgba(1, 1, 1, 1));
            // Circle + a segment coloured by intersection.
            const bool cross =
                math::segmentIntersectsCircle(circleSeg.first, circleSeg.second, circleC, circleR);
            ring(*renderer, circleC, circleR, 2.0f, rgba(0.7f, 0.75f, 0.85f, 1));
            thickLine(*renderer, circleSeg.first, circleSeg.second, 2.5f,
                      cross ? rgba(0.5f, 0.95f, 0.6f, 1) : rgba(0.9f, 0.4f, 0.4f, 1));
            font.drawText(*renderer, 868.0f, 96.0f, "closest point (orange) + circle x segment",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GEOMETRY shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
