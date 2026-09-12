// Maz Engine — "LINE2D" (2D polyline stroking, toward Godot's Line2D)
// The engine could FILL polygons; this STROKES a path — turning a point list into a thick ribbon with
// shaped corners (joints) and ends (caps) via render::buildPolyline, then drawing the resulting triangle
// soup with drawConvexPolygon. The gallery shows the same sharp zig-zag under all three JOINT modes
// (miter / bevel / round), a short bar under all three CAP modes, a smooth sampled sine CURVE, and a
// closed STAR loop. All geometry is static → deterministic, golden-stable. Run --headless / --frames N.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

// Draw a stroked polyline: build its triangle soup and fill each triangle.
void drawPoly(render::Renderer& r, const std::vector<math::vec2>& pts, const render::PolylineStyle& style,
              render::Color col) {
    const std::vector<math::vec2> tris = render::buildPolyline(pts, style);
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const render::Point2 tri[3] = {{tris[i].x, tris[i].y},
                                       {tris[i + 1].x, tris[i + 1].y},
                                       {tris[i + 2].x, tris[i + 2].y}};
        r.drawConvexPolygon(tri, 3, col);
    }
}

// A sharp zig-zag path anchored at (ox, oy).
std::vector<math::vec2> zigzag(float ox, float oy) {
    return {math::vec2(ox, oy),           math::vec2(ox + 70.0f, oy - 70.0f), math::vec2(ox + 140.0f, oy),
            math::vec2(ox + 210.0f, oy - 70.0f), math::vec2(ox + 280.0f, oy)};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LINE2D (polyline stroking) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2D Polylines";
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

    const render::Color kBlue{0.42f, 0.72f, 1.0f, 1.0f};
    const render::Color kOrange{1.0f, 0.66f, 0.34f, 1.0f};
    const render::Color kGreen{0.5f, 0.82f, 0.5f, 1.0f};
    const render::Color kViolet{0.72f, 0.6f, 0.95f, 1.0f};

    // Precompute the sine curve points once.
    std::vector<math::vec2> sine;
    for (int i = 0; i <= 80; ++i) {
        const float t = static_cast<float>(i) / 80.0f;
        const float x = 70.0f + t * 1140.0f;
        const float y = 486.0f - std::sin(t * 6.2831853f * 1.5f) * 42.0f;
        sine.push_back(math::vec2(x, y));
    }

    // A closed 5-point star loop centered at (c).
    std::vector<math::vec2> star;
    {
        const math::vec2 c(640.0f, 618.0f);
        for (int i = 0; i < 10; ++i) {
            const float ang = -1.5707963f + static_cast<float>(i) * 3.14159265f / 5.0f;
            const float rad = (i % 2 == 0) ? 62.0f : 26.0f;
            star.push_back(math::vec2(c.x + std::cos(ang) * rad, c.y + std::sin(ang) * rad));
        }
    }

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D POLYLINES (Line2D)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "stroke a path into a thick ribbon with joints + caps (render::buildPolyline)",
                          render::Color{0.78f, 0.83f, 0.95f, 1}, 0.34f);

            // Row 1 — joint modes on an identical zig-zag.
            const float jy = 190.0f;
            const render::JointMode joints[3] = {render::JointMode::Miter, render::JointMode::Bevel,
                                                 render::JointMode::Round};
            const char* jnames[3] = {"miter joints", "bevel joints", "round joints"};
            for (int i = 0; i < 3; ++i) {
                render::PolylineStyle s;
                s.width = 20.0f;
                s.joint = joints[i];
                s.cap = render::CapMode::Box;
                drawPoly(*renderer, zigzag(40.0f + static_cast<float>(i) * 410.0f, jy), s, kBlue);
                font.drawText(*renderer, 40.0f + static_cast<float>(i) * 410.0f, jy + 34.0f, jnames[i],
                              render::Color{0.8f, 0.84f, 0.92f, 1}, 0.3f);
            }

            // Row 2 — cap modes on a short bar.
            const float cyy = 320.0f;
            const render::CapMode caps[3] = {render::CapMode::None, render::CapMode::Box,
                                             render::CapMode::Round};
            const char* cnames[3] = {"no caps", "box caps", "round caps"};
            for (int i = 0; i < 3; ++i) {
                render::PolylineStyle s;
                s.width = 28.0f;
                s.cap = caps[i];
                const float x0 = 90.0f + static_cast<float>(i) * 410.0f;
                drawPoly(*renderer, {math::vec2(x0, cyy), math::vec2(x0 + 220.0f, cyy)}, s, kOrange);
                font.drawText(*renderer, x0, cyy + 34.0f, cnames[i], render::Color{0.8f, 0.84f, 0.92f, 1},
                              0.3f);
            }

            // Row 3 — a smooth sampled curve.
            {
                render::PolylineStyle s;
                s.width = 8.0f;
                s.joint = render::JointMode::Round;
                s.cap = render::CapMode::Round;
                drawPoly(*renderer, sine, s, kGreen);
                font.drawText(*renderer, 40.0f, 408.0f, "sampled curve (80-point polyline, round joints)",
                              render::Color{0.8f, 0.84f, 0.92f, 1}, 0.3f);
            }

            // Row 4 — a closed star loop.
            {
                render::PolylineStyle s;
                s.width = 10.0f;
                s.joint = render::JointMode::Miter;
                s.closed = true;
                drawPoly(*renderer, star, s, kViolet);
                font.drawText(*renderer, 540.0f, 692.0f, "closed loop (miter joints)",
                              render::Color{0.8f, 0.84f, 0.92f, 1}, 0.3f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LINE2D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
