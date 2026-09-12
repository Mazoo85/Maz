// Maz Engine — "POLYCOLLIDE" (2D convex polygon collision via SAT, toward Godot's ConvexPolygonShape2D)
// A central PROBE polygon (a pentagon) is tested against a ring of other convex shapes — a triangle, an
// oriented box, a hexagon, a diamond, a big pentagon — using game::satOverlap. Overlapping shapes are
// drawn red with the minimum-translation-vector arrow showing which way (and how far) to push them apart;
// clear shapes are green. All geometry is static → deterministic, golden-stable. Run --headless / --frames.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillPoly(render::Renderer& r, const game::ConvexPoly2D& p, render::Color col) {
    std::vector<render::Point2> pts;
    pts.reserve(p.points.size());
    for (const math::vec2& v : p.points) {
        pts.push_back({v.x, v.y});
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(pts.size()), col);
}

void strokePoly(render::Renderer& r, const game::ConvexPoly2D& p, float width, render::Color col) {
    render::PolylineStyle s;
    s.width = width;
    s.closed = true;
    s.joint = render::JointMode::Miter;
    const std::vector<math::vec2> tris = render::buildPolyline(p.points, s);
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const render::Point2 tri[3] = {{tris[i].x, tris[i].y},
                                       {tris[i + 1].x, tris[i + 1].y},
                                       {tris[i + 2].x, tris[i + 2].y}};
        r.drawConvexPolygon(tri, 3, col);
    }
}

void drawArrow(render::Renderer& r, math::vec2 a, math::vec2 b, render::Color col) {
    render::PolylineStyle s;
    s.width = 4.0f;
    s.cap = render::CapMode::Box;
    auto stroke = [&](const std::vector<math::vec2>& pts) {
        const std::vector<math::vec2> tris = render::buildPolyline(pts, s);
        for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
            const render::Point2 tri[3] = {{tris[i].x, tris[i].y},
                                           {tris[i + 1].x, tris[i + 1].y},
                                           {tris[i + 2].x, tris[i + 2].y}};
            r.drawConvexPolygon(tri, 3, col);
        }
    };
    stroke({a, b});
    // Arrowhead.
    const math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > 1e-3f) {
        const math::vec2 dir = d / len;
        const math::vec2 perp(-dir.y, dir.x);
        const math::vec2 h1 = b - dir * 14.0f + perp * 8.0f;
        const math::vec2 h2 = b - dir * 14.0f - perp * 8.0f;
        stroke({h1, b});
        stroke({h2, b});
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("POLYCOLLIDE (2D convex SAT) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Convex Polygon Collision";
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

    // The probe: a pentagon at screen center.
    const math::vec2 center(640.0f, 380.0f);
    const game::ConvexPoly2D probe = game::makeRegularPoly(center, 95.0f, 5, 0.3f);

    // A ring of test shapes — some deliberately overlapping the probe, some clear.
    struct Shape {
        game::ConvexPoly2D poly;
        const char* label;
        float labelX;
        float labelY;
    };
    std::vector<Shape> shapes = {
        // Two shapes deliberately OVERLAP the probe (their centers sit within the probe's radius) ...
        {game::makeRegularPoly(math::vec2(560.0f, 330.0f), 70.0f, 3, 0.4f), "triangle", 470.0f, 250.0f},
        {game::makeBoxPoly(math::vec2(720.0f, 360.0f), math::vec2(80.0f, 45.0f), 0.5f), "box", 800.0f,
         300.0f},
        // ... and three sit clear of it.
        {game::makeRegularPoly(math::vec2(1030.0f, 250.0f), 80.0f, 6), "hexagon", 990.0f, 120.0f},
        {game::makeBoxPoly(math::vec2(560.0f, 620.0f), math::vec2(60.0f, 60.0f), 0.78f), "diamond",
         500.0f, 690.0f},
        {game::makeRegularPoly(math::vec2(220.0f, 540.0f), 85.0f, 5, 1.0f), "pentagon", 140.0f, 640.0f},
    };

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kProbeFill{0.30f, 0.40f, 0.62f, 0.55f};
    const render::Color kProbeEdge{0.6f, 0.72f, 1.0f, 1.0f};
    const render::Color kHitFill{0.55f, 0.24f, 0.24f, 0.6f};
    const render::Color kHitEdge{1.0f, 0.42f, 0.4f, 1.0f};
    const render::Color kClearFill{0.24f, 0.45f, 0.30f, 0.55f};
    const render::Color kClearEdge{0.5f, 0.9f, 0.55f, 1.0f};
    const render::Color kMtv{1.0f, 0.85f, 0.35f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1.0f};
    const render::Color kDim{0.68f, 0.74f, 0.86f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CONVEX POLYGON COLLISION (SAT)", kText,
                          0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "probe pentagon vs convex shapes  -  red = overlap + MTV arrow, green = clear",
                          kDim, 0.34f);

            for (const Shape& sh : shapes) {
                const game::SatHit2D hit = game::satOverlap(probe, sh.poly);
                fillPoly(*renderer, sh.poly, hit ? kHitFill : kClearFill);
                strokePoly(*renderer, sh.poly, 3.0f, hit ? kHitEdge : kClearEdge);
                font.drawText(*renderer, sh.labelX, sh.labelY, sh.label, hit ? kHitEdge : kClearEdge,
                              0.36f);
                if (hit) {
                    // Draw the MTV arrow from the shape's center along the push axis, scaled by depth.
                    math::vec2 c(0.0f, 0.0f);
                    for (const math::vec2& v : sh.poly.points) {
                        c += v;
                    }
                    c /= static_cast<float>(sh.poly.points.size());
                    const math::vec2 tip = c + hit.axis * (hit.depth + 24.0f);
                    drawArrow(*renderer, c, tip, kMtv);
                }
            }

            // The probe on top.
            fillPoly(*renderer, probe, kProbeFill);
            strokePoly(*renderer, probe, 4.0f, kProbeEdge);
            font.drawText(*renderer, center.x - 34.0f, center.y - 8.0f, "probe", kText, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("POLYCOLLIDE shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
