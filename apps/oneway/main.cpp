// Maz Engine — "ONEWAY" (one-way platforms, toward Godot's one_way_collision)
// A one-way platform is solid only from above: a ball dropped onto it LANDS, but a ball launched up from
// below passes straight THROUGH it. This runs a fixed-step physics sim: three balls fall onto three ledges
// and rest on top, while a fourth ball is launched upward and punches through the ledge above it (its trail
// crosses the platform bar). The resolve is game::resolveOneWayPlatforms. Fixed initial state + fixed step
// count -> deterministic, golden-stable. Run --headless / --frames N for CI.

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

void fillCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 20;
    render::Point2 pts[20];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

struct Ball {
    math::vec2 pos;
    math::vec2 vel;
    bool resting = false;
    render::Color color;
    std::vector<math::vec2> trail;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ONEWAY (one-way platforms) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — One-Way Platforms";
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

    const std::vector<game::OneWayPlatform2D> platforms = {
        {560.0f, 100.0f, 480.0f},  // A (low left)
        {400.0f, 520.0f, 860.0f},  // B (mid)
        {560.0f, 900.0f, 1180.0f}, // C (low right)
    };

    const float radius = 14.0f;
    std::vector<Ball> balls = {
        {math::vec2(250.0f, 380.0f), math::vec2(0.0f, 0.0f), false, {0.95f, 0.55f, 0.35f, 1.0f}, {}},
        {math::vec2(690.0f, 220.0f), math::vec2(0.0f, 0.0f), false, {0.45f, 0.8f, 1.0f, 1.0f}, {}},
        {math::vec2(1040.0f, 380.0f), math::vec2(0.0f, 0.0f), false, {0.55f, 0.9f, 0.6f, 1.0f}, {}},
        // Launched UP from below platform A — passes through it.
        {math::vec2(300.0f, 610.0f), math::vec2(0.0f, -700.0f), false, {1.0f, 0.85f, 0.35f, 1.0f}, {}},
    };

    // --- Run the deterministic fixed-step simulation up front. ---
    const float g = 900.0f;
    const float dt = 1.0f / 60.0f;
    const int steps = 80;
    for (int s = 0; s < steps; ++s) {
        for (Ball& b : balls) {
            if (b.resting) {
                continue;
            }
            b.vel.y += g * dt;
            const math::vec2 next = b.pos + b.vel * dt;
            const float prevBottom = b.pos.y + radius;
            const float curBottom = next.y + radius;
            const game::OneWayResult hit = game::resolveOneWayPlatforms(
                prevBottom, curBottom, next.x - radius, next.x + radius, platforms);
            if (hit.landed) {
                b.pos = math::vec2(next.x, hit.y - radius);
                b.vel = math::vec2(0.0f, 0.0f);
                b.resting = true;
            } else {
                b.pos = next;
            }
            if (s % 3 == 0) {
                b.trail.push_back(b.pos);
            }
        }
    }

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.6f, 0.66f, 0.78f, 1};
    const render::Color kPlat{0.34f, 0.38f, 0.46f, 1.0f};
    const render::Color kMark{0.45f, 0.85f, 1.0f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ONE-WAY PLATFORMS", kText, 0.55f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "solid from above: dropped balls land, the launched ball passes UP through "
                          "(game::resolveOneWayPlatforms)",
                          kDim, 0.3f);

            // Platforms: a bar + a row of little up-triangles marking the solid (top) side.
            for (const game::OneWayPlatform2D& p : platforms) {
                fillRect(*renderer, white, p.x0, p.y, p.x1 - p.x0, 8.0f, kPlat);
                for (float mx = p.x0 + 14.0f; mx < p.x1 - 6.0f; mx += 34.0f) {
                    const render::Point2 tri[3] = {
                        {mx, p.y - 10.0f}, {mx - 6.0f, p.y - 1.0f}, {mx + 6.0f, p.y - 1.0f}};
                    renderer->drawConvexPolygon(tri, 3, kMark);
                }
            }

            // Trails (faint) then balls.
            for (const Ball& b : balls) {
                render::Color faint = b.color;
                faint.a = 0.35f;
                for (const math::vec2& t : b.trail) {
                    fillCircle(*renderer, t, 3.0f, faint);
                }
            }
            for (const Ball& b : balls) {
                fillCircle(*renderer, b.pos, radius, b.color);
            }

            font.drawText(*renderer, 200.0f, 300.0f, "launched up", render::Color{1, 0.85f, 0.35f, 1},
                          0.3f);
            font.drawText(*renderer, 40.0f, 662.0f,
                          "up-triangles mark each platform's solid side; the yellow ball's trail crosses "
                          "platform A",
                          render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ONEWAY shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
