// Maz Engine — "CURVE" (cubic Bézier path, toward Godot's Curve2D / Path2D)
// A Curve2D is authored from points with in/out control handles and drawn as a smooth spline. On top of the
// raw curve this shows the two things that make it useful: the control points + their handles (how the shape
// is authored), and the ARC-LENGTH-BAKED points (green) spaced evenly along the path — proof the curve can be
// travelled at constant speed. A yellow traveller sits at a fixed baked distance with its tangent arrow.
// Everything is static → deterministic, golden-stable. Run --headless / --frames N for CI.

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

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

void dot(render::Renderer& r, render::TextureHandle white, math::vec2 p, float radius, render::Color c) {
    fillRect(r, white, p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f, c);
}

void stroke(render::Renderer& r, const std::vector<math::vec2>& pts, float width, render::Color col,
            render::JointMode joint = render::JointMode::Round, render::CapMode cap = render::CapMode::Round) {
    render::PolylineStyle s;
    s.width = width;
    s.joint = joint;
    s.cap = cap;
    const std::vector<math::vec2> tris = render::buildPolyline(pts, s);
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const render::Point2 t[3] = {{tris[i].x, tris[i].y},
                                     {tris[i + 1].x, tris[i + 1].y},
                                     {tris[i + 2].x, tris[i + 2].y}};
        r.drawConvexPolygon(t, 3, col);
    }
}

void arrow(render::Renderer& r, math::vec2 a, math::vec2 b, render::Color col) {
    stroke(r, {a, b}, 4.0f, col, render::JointMode::Miter, render::CapMode::Box);
    const math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > 1e-3f) {
        const math::vec2 dir = d / len;
        const math::vec2 perp(-dir.y, dir.x);
        stroke(r, {b - dir * 16.0f + perp * 9.0f, b}, 4.0f, col, render::JointMode::Miter,
               render::CapMode::Box);
        stroke(r, {b - dir * 16.0f - perp * 9.0f, b}, 4.0f, col, render::JointMode::Miter,
               render::CapMode::Box);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CURVE (Bezier path) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Curve2D (Bezier path)";
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

    // Author a wavy path with in/out handles.
    math::Curve2D curve;
    curve.addPoint(math::vec2(150.0f, 420.0f), math::vec2(0.0f), math::vec2(150.0f, -190.0f));
    curve.addPoint(math::vec2(490.0f, 240.0f), math::vec2(-150.0f, -70.0f), math::vec2(150.0f, 70.0f));
    curve.addPoint(math::vec2(820.0f, 540.0f), math::vec2(-150.0f, 70.0f), math::vec2(150.0f, -70.0f));
    curve.addPoint(math::vec2(1150.0f, 320.0f), math::vec2(-150.0f, -150.0f), math::vec2(0.0f));
    curve.bake(38.0f);

    // Precompute the smooth curve polyline (fofs 0..count-1).
    std::vector<math::vec2> path;
    const int samples = 240;
    const float maxOfs = static_cast<float>(curve.pointCount() - 1);
    for (int i = 0; i <= samples; ++i) {
        const float f = maxOfs * static_cast<float>(i) / static_cast<float>(samples);
        path.push_back(curve.sample(f));
    }

    const float travelDist = curve.bakedLength() * 0.62f;
    const math::vec2 traveler = curve.sampleBaked(travelDist);
    const math::vec2 tang = curve.tangent(0.62f * maxOfs);

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.62f, 0.68f, 0.8f, 1};
    const render::Color kCurve{0.45f, 0.8f, 1.0f, 1.0f};
    const render::Color kHandleLine{0.5f, 0.5f, 0.6f, 1.0f};
    const render::Color kHandleDot{1.0f, 0.65f, 0.3f, 1.0f};
    const render::Color kPoint{0.95f, 0.95f, 1.0f, 1.0f};
    const render::Color kBaked{0.5f, 0.92f, 0.6f, 1.0f};
    const render::Color kTravel{1.0f, 0.85f, 0.3f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CURVE2D (cubic Bezier path)", kText, 0.55f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "authored points + handles -> smooth spline; green = arc-length-baked "
                          "(constant-speed) samples",
                          kDim, 0.3f);

            // Control handles (behind the curve).
            for (std::size_t i = 0; i < curve.pointCount(); ++i) {
                const math::CurvePoint2D& cp = curve.point(i);
                if (cp.in.x != 0.0f || cp.in.y != 0.0f) {
                    stroke(*renderer, {cp.position, cp.position + cp.in}, 2.0f, kHandleLine);
                    dot(*renderer, white, cp.position + cp.in, 5.0f, kHandleDot);
                }
                if (cp.out.x != 0.0f || cp.out.y != 0.0f) {
                    stroke(*renderer, {cp.position, cp.position + cp.out}, 2.0f, kHandleLine);
                    dot(*renderer, white, cp.position + cp.out, 5.0f, kHandleDot);
                }
            }

            // The smooth curve.
            stroke(*renderer, path, 5.0f, kCurve);

            // Constant-speed baked samples.
            for (const math::vec2& b : curve.bakedPoints()) {
                dot(*renderer, white, b, 4.0f, kBaked);
            }

            // Control points on top.
            for (std::size_t i = 0; i < curve.pointCount(); ++i) {
                dot(*renderer, white, curve.point(i).position, 7.0f, kPoint);
            }

            // The traveller + its tangent.
            dot(*renderer, white, traveler, 10.0f, kTravel);
            arrow(*renderer, traveler, traveler + tang * 70.0f, kTravel);

            char info[128];
            std::snprintf(info, sizeof(info),
                          "%zu points   arc length %.0f px   %zu baked samples @ ~38px   traveller @ %.0f px",
                          curve.pointCount(), static_cast<double>(curve.bakedLength()),
                          curve.bakedPoints().size(), static_cast<double>(travelDist));
            font.drawText(*renderer, 40.0f, 662.0f, info, render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CURVE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
