// Maz Engine — "NAVMESH" (navigation-mesh pathfinding, toward Godot's NavigationServer)
// A room with a solid pillar in the middle, whose walkable area is described as a navigation mesh of
// convex polygon cells (game::NavMesh). findPath() A*-searches the cell graph for the corridor between
// two points, then runs the funnel algorithm to string-pull a short, smooth path that HUGS the pillar's
// corners — the polygon-based navigation Godot's NavigationServer provides, a step up from grid A*
// (which zig-zags). The scene is static, so the render is golden-stable. Run --headless / --frames N.

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

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.85f ? (1.0f - d) / 0.15f : 1.0f);
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

// A thick line from p0 to p1 as a rotated quad.
void drawLine(render::Renderer& r, render::TextureHandle white, math::vec2 p0, math::vec2 p1,
              float thick, render::Color col) {
    const float dx = p1.x - p0.x, dy = p1.y - p0.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    render::SpriteDesc d;
    d.x = (p0.x + p1.x) * 0.5f - len * 0.5f;
    d.y = (p0.y + p1.y) * 0.5f - thick * 0.5f;
    d.width = len;
    d.height = thick;
    d.rotation = std::atan2(dy, dx);
    d.color = col;
    r.drawSprite(white, d);
}

std::vector<math::vec2> square(float x0, float y0, float x1, float y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("NAVMESH (navigation-mesh pathfinding) starting");

    // --- Build the navigation mesh: a room around a central pillar, as a grid-aligned convex mesh --
    // Columns x: 100 480 800 1180   Rows y: 140 300 460 620   Pillar: x[480..800] y[300..460].
    game::NavMesh nav;
    nav.addPolygon(square(100, 140, 480, 300)); // left-top
    nav.addPolygon(square(100, 300, 480, 460)); // left-mid
    nav.addPolygon(square(100, 460, 480, 620)); // left-bot
    nav.addPolygon(square(480, 140, 800, 300)); // mid-top
    nav.addPolygon(square(480, 460, 800, 620)); // mid-bot   (mid-mid is the pillar: omitted)
    nav.addPolygon(square(800, 140, 1180, 300)); // right-top
    nav.addPolygon(square(800, 300, 1180, 460)); // right-mid
    nav.addPolygon(square(800, 460, 1180, 620)); // right-bot
    nav.build();

    const math::vec2 start{200.0f, 560.0f}, goal{1060.0f, 210.0f};
    const std::vector<math::vec2> path = nav.findPath(start, goal);
    MAZ_LOG_INFO("navmesh: %zu cells, path with %zu waypoints", nav.polyCount(), path.size());

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — NavMesh";
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
    render::TextureHandle disc = discTex(*renderer, 48);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
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

            // Walkable cells: faint fill + edge outlines (the navigation mesh).
            for (size_t i = 0; i < nav.polyCount(); ++i) {
                const game::NavPoly& p = nav.poly(i);
                // Fill (axis-aligned cells -> a rect from the bounding of the 4 verts).
                float minx = p.verts[0].x, miny = p.verts[0].y, maxx = minx, maxy = miny;
                for (const math::vec2& v : p.verts) {
                    minx = v.x < minx ? v.x : minx;
                    miny = v.y < miny ? v.y : miny;
                    maxx = v.x > maxx ? v.x : maxx;
                    maxy = v.y > maxy ? v.y : maxy;
                }
                render::SpriteDesc d;
                d.x = minx;
                d.y = miny;
                d.width = maxx - minx;
                d.height = maxy - miny;
                d.color = render::Color{0.16f, 0.20f, 0.28f, 1.0f};
                renderer->drawSprite(white, d);
                for (size_t k = 0; k < p.verts.size(); ++k) {
                    drawLine(*renderer, white, p.verts[k], p.verts[(k + 1) % p.verts.size()], 2.0f,
                             render::Color{0.32f, 0.40f, 0.52f, 1.0f});
                }
            }

            // The pillar (the hole in the mesh).
            {
                render::SpriteDesc d;
                d.x = 480.0f;
                d.y = 300.0f;
                d.width = 320.0f;
                d.height = 160.0f;
                d.color = render::Color{0.45f, 0.24f, 0.26f, 1.0f};
                renderer->drawSprite(white, d);
            }

            // The string-pulled path.
            for (size_t i = 0; i + 1 < path.size(); ++i)
                drawLine(*renderer, white, path[i], path[i + 1], 5.0f,
                         render::Color{1.0f, 0.82f, 0.30f, 1.0f});
            for (const math::vec2& w : path) {
                render::SpriteDesc d;
                d.x = w.x - 8.0f;
                d.y = w.y - 8.0f;
                d.width = 16.0f;
                d.height = 16.0f;
                d.color = render::Color{1.0f, 0.9f, 0.5f, 1.0f};
                renderer->drawSprite(disc, d);
            }

            // Start (green) + goal (red).
            auto marker = [&](math::vec2 p, render::Color c) {
                render::SpriteDesc d;
                d.x = p.x - 15.0f;
                d.y = p.y - 15.0f;
                d.width = 30.0f;
                d.height = 30.0f;
                d.color = c;
                renderer->drawSprite(disc, d);
            };
            marker(start, render::Color{0.4f, 0.9f, 0.5f, 1.0f});
            marker(goal, render::Color{1.0f, 0.4f, 0.4f, 1.0f});

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  NAVIGATION MESH (funnel path)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "%zu convex cells  -  A* over cells + funnel string-pull  -  %zu waypoints",
                          nav.polyCount(), path.size());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("NAVMESH shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
