// Maz Engine — "RAYQUERY" (2D physics-space queries, toward Godot's PhysicsDirectSpaceState2D)
// The engine could simulate 2D bodies; this ASKS the collider set spatial questions without stepping the
// sim: cast a fan of rays from a muzzle and stop each at the first shape it hits (hitscan / line-of-sight),
// and point-pick a shape under a cursor. All geometry is static → deterministic, golden-stable. The scene
// has circles and oriented boxes on two collision layers; the ray fan is filtered to a mask so it passes
// THROUGH the "glass" layer and stops on "solid". Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

// Fill a circle as a triangle fan.
void fillCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col, int segs = 48) {
    std::vector<render::Point2> pts;
    pts.reserve(static_cast<std::size_t>(segs) + 2);
    pts.push_back({c.x, c.y});
    for (int i = 0; i <= segs; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(segs) * 6.2831853f;
        pts.push_back({c.x + std::cos(a) * radius, c.y + std::sin(a) * radius});
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(pts.size()), col);
}

// Fill an oriented box.
void fillBox(render::Renderer& r, math::vec2 c, math::vec2 half, float angle, render::Color col) {
    const float ca = std::cos(angle), sa = std::sin(angle);
    const math::vec2 ax(ca, sa), ay(-sa, ca);
    const math::vec2 corners[4] = {c - ax * half.x - ay * half.y, c + ax * half.x - ay * half.y,
                                   c + ax * half.x + ay * half.y, c - ax * half.x + ay * half.y};
    const render::Point2 p[4] = {{corners[0].x, corners[0].y},
                                 {corners[1].x, corners[1].y},
                                 {corners[2].x, corners[2].y},
                                 {corners[3].x, corners[3].y}};
    r.drawConvexPolygon(p, 4, col);
}

// Stroke a line segment as a thin ribbon via buildPolyline.
void drawSeg(render::Renderer& r, math::vec2 a, math::vec2 b, float width, render::Color col) {
    render::PolylineStyle s;
    s.width = width;
    s.cap = render::CapMode::Box;
    const std::vector<math::vec2> tris = render::buildPolyline({a, b}, s);
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const render::Point2 tri[3] = {{tris[i].x, tris[i].y},
                                       {tris[i + 1].x, tris[i + 1].y},
                                       {tris[i + 2].x, tris[i + 2].y}};
        r.drawConvexPolygon(tri, 3, col);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RAYQUERY (2D physics queries) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2D Physics Queries";
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

    // Two collision layers: SOLID (walls the rays stop on) and GLASS (rays pass through).
    const uint32_t kSolid = 0x1;
    const uint32_t kGlass = 0x2;

    // Static scene, in pixel space. A muzzle on the left fans rays to the right past a glass pane and
    // into a field of solid circles + oriented boxes.
    std::vector<game::QueryShape2D> shapes;
    auto addCircle = [&](float x, float y, float r, uint32_t layer) {
        game::QueryShape2D s;
        s.kind = game::QueryShape2D::Circle;
        s.pos = math::vec2(x, y);
        s.radius = r;
        s.layer = layer;
        s.id = static_cast<int>(shapes.size());
        shapes.push_back(s);
    };
    auto addBox = [&](float x, float y, float hx, float hy, float ang, uint32_t layer) {
        game::QueryShape2D s;
        s.kind = game::QueryShape2D::Box;
        s.pos = math::vec2(x, y);
        s.half = math::vec2(hx, hy);
        s.angle = ang;
        s.layer = layer;
        s.id = static_cast<int>(shapes.size());
        shapes.push_back(s);
    };

    // A glass pane the rays should pierce.
    addBox(430.0f, 360.0f, 12.0f, 220.0f, 0.0f, kGlass);
    // Solid obstacles.
    addCircle(700.0f, 220.0f, 60.0f, kSolid);
    addCircle(940.0f, 470.0f, 80.0f, kSolid);
    addBox(760.0f, 470.0f, 55.0f, 55.0f, 0.6f, kSolid);
    addBox(1090.0f, 250.0f, 40.0f, 90.0f, -0.4f, kSolid);
    addCircle(560.0f, 560.0f, 45.0f, kSolid);

    const math::vec2 muzzle(120.0f, 360.0f);
    // The point we "mouse-pick" (static, for a deterministic golden): inside the big lower-right circle.
    const math::vec2 pickPoint(940.0f, 470.0f);
    const std::vector<int> picked = game::queryPoint(pickPoint, shapes);

    const render::Color kBg{0.07f, 0.08f, 0.11f, 1.0f};
    const render::Color kSolidCol{0.34f, 0.40f, 0.52f, 1.0f};
    const render::Color kGlassCol{0.30f, 0.55f, 0.70f, 0.45f};
    const render::Color kRay{1.0f, 0.82f, 0.30f, 0.9f};
    const render::Color kHit{1.0f, 0.35f, 0.30f, 1.0f};
    const render::Color kMuzzle{0.95f, 0.85f, 0.4f, 1.0f};
    const render::Color kPick{0.4f, 0.95f, 0.55f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D PHYSICS QUERIES (intersect_ray)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "hitscan ray fan stops on SOLID, passes through GLASS  -  green = point-pick",
                          render::Color{0.78f, 0.83f, 0.95f, 1}, 0.34f);

            // Draw the shapes.
            for (const game::QueryShape2D& s : shapes) {
                const bool isPicked =
                    std::find(picked.begin(), picked.end(), s.id) != picked.end();
                render::Color col = (s.layer == kGlass) ? kGlassCol : kSolidCol;
                if (isPicked) {
                    col = kPick;
                }
                if (s.kind == game::QueryShape2D::Circle) {
                    fillCircle(*renderer, s.pos, s.radius, col);
                } else {
                    fillBox(*renderer, s.pos, s.half, s.angle, col);
                }
            }

            // Cast a fan of rays that ignore the glass layer (mask = solid only).
            const int rays = 15;
            for (int i = 0; i < rays; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(rays - 1);
                const float ang = (t - 0.5f) * 1.0f; // +/- 0.5 rad spread
                const math::vec2 dir(std::cos(ang), std::sin(ang));
                const game::RayHit2D h = game::queryRay(muzzle, dir, shapes, 2000.0f, kSolid);
                const math::vec2 end = h.hit ? h.point : muzzle + dir * 1400.0f;
                drawSeg(*renderer, muzzle, end, 2.5f, kRay);
                if (h.hit) {
                    fillCircle(*renderer, h.point, 6.0f, kHit, 12);
                    // Draw the surface normal as a short stub.
                    drawSeg(*renderer, h.point, h.point + h.normal * 26.0f, 2.0f, kHit);
                }
            }

            // The muzzle.
            fillCircle(*renderer, muzzle, 12.0f, kMuzzle);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RAYQUERY shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
