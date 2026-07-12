// Maz Engine — "LIGHTS2D" (2D lights + shadows, toward Godot's Light2D + LightOccluder2D)
// A dark room with a few solid occluder boxes and three colored point lights. For each light we
// compute its visibility polygon (game::Visibility2D) against the box edges + room bounds, then
// render it as a gradient triangle fan (drawPolygonFan): bright at the light, faded to nothing at
// the rim, with the notches behind the boxes carved out — those notches ARE the shadows. The lights
// alpha-composite over the dark room, and the boxes are drawn solid on top so they read as objects
// blocking the light. The scene is static, so the render is golden-stable. Run --headless / --frames.

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

struct Box {
    float x, y, w, h;
};

// The four edges of a box, as occluder segments.
void addBoxEdges(std::vector<game::Segment2>& out, const Box& b) {
    const math::vec2 p0{b.x, b.y}, p1{b.x + b.w, b.y};
    const math::vec2 p2{b.x + b.w, b.y + b.h}, p3{b.x, b.y + b.h};
    out.push_back(game::Segment2{p0, p1});
    out.push_back(game::Segment2{p1, p2});
    out.push_back(game::Segment2{p2, p3});
    out.push_back(game::Segment2{p3, p0});
}

struct Light {
    float x, y;
    render::Color color;
    float radius;
};

// Render one light as a gradient fan over its visibility polygon.
void drawLight(render::Renderer& r, const Light& li, const std::vector<game::Segment2>& occ,
               math::vec2 bmin, math::vec2 bmax) {
    const auto poly = game::Visibility2D::compute(math::vec2{li.x, li.y}, occ, bmin, bmax);
    if (poly.size() < 3) {
        return;
    }
    std::vector<render::PolyVertex> fan;
    fan.reserve(poly.size() + 2);
    // Center: full-strength light color.
    fan.push_back(render::PolyVertex{li.x, li.y, li.color});
    // Rim: light color faded to zero by distance (finite radius glow).
    auto rim = [&](const math::vec2& p) {
        const float dx = p.x - li.x, dy = p.y - li.y;
        const float d = std::sqrt(dx * dx + dy * dy);
        float f = 1.0f - d / li.radius;
        if (f < 0.0f) f = 0.0f;
        f = f * f; // ease so the falloff looks soft
        return render::PolyVertex{p.x, p.y,
                                  render::Color{li.color.r, li.color.g, li.color.b, li.color.a * f}};
    };
    for (const math::vec2& p : poly) {
        fan.push_back(rim(p));
    }
    fan.push_back(rim(poly.front())); // close the loop back to the first rim vertex
    // Additive: where two lights overlap the light ADDS up (brightens) instead of averaging —
    // physically how light accumulates, and how Godot's Light2D composites.
    r.drawPolygonFan(fan.data(), static_cast<uint32_t>(fan.size()), render::BlendMode::Additive);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LIGHTS2D (2D lights + shadows) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2D Lights + Shadows";
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
    const float sh = static_cast<float>(cfg.height);
    const math::vec2 bmin{0.0f, 0.0f}, bmax{sw, sh};

    // Static occluder boxes.
    const std::vector<Box> boxes = {
        {sw * 0.30f, sh * 0.28f, 90.0f, 90.0f},
        {sw * 0.58f, sh * 0.20f, 70.0f, 160.0f},
        {sw * 0.44f, sh * 0.62f, 150.0f, 60.0f},
        {sw * 0.16f, sh * 0.66f, 80.0f, 80.0f},
    };
    std::vector<game::Segment2> occ;
    for (const Box& b : boxes) {
        addBoxEdges(occ, b);
    }

    // Static colored lights. Alphas are moderate because the pools composite ADDITIVELY — where they
    // overlap the colors sum toward white, so keeping each below 1 leaves the blend colorful.
    const std::vector<Light> lights = {
        {sw * 0.20f, sh * 0.22f, render::Color{1.0f, 0.82f, 0.5f, 0.85f}, sw * 0.55f}, // warm
        {sw * 0.80f, sh * 0.70f, render::Color{0.4f, 0.72f, 1.0f, 0.8f}, sw * 0.55f},  // cool
        {sw * 0.52f, sh * 0.40f, render::Color{1.0f, 0.35f, 0.7f, 0.7f}, sw * 0.42f},  // magenta
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.03f, 0.03f, 0.05f, 1.0f}); // near-black room
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Light pools first (they alpha-blend over the dark room).
            for (const Light& li : lights) {
                drawLight(*renderer, li, occ, bmin, bmax);
            }

            // Solid boxes on top, so they read as objects blocking the light.
            for (const Box& b : boxes) {
                const render::Point2 quad[4] = {
                    {b.x, b.y}, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.h}, {b.x, b.y + b.h}};
                renderer->drawConvexPolygon(quad, 4, render::Color{0.10f, 0.11f, 0.14f, 1.0f});
            }

            // Small markers at each light source.
            for (const Light& li : lights) {
                const render::Point2 dot[4] = {{li.x - 4, li.y - 4},
                                               {li.x + 4, li.y - 4},
                                               {li.x + 4, li.y + 4},
                                               {li.x - 4, li.y + 4}};
                renderer->drawConvexPolygon(dot, 4, render::Color{1.0f, 1.0f, 1.0f, 1.0f});
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D LIGHTS + SHADOWS",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "additive light pools (overlaps brighten); boxes cast real shadows",
                          render::Color{0.75f, 0.82f, 0.95f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LIGHTS2D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
