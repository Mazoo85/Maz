// Maz Engine — "SOFTSHADOW" (soft/penumbra 2D shadows via area-light sampling, toward Godot Light2D)
// The SAME scene — one occluder box and one light — drawn twice for comparison. On the LEFT the light
// is a single point (game::Visibility2D): a razor-sharp shadow edge. On the RIGHT the light is an
// AREA light: game::diskSamples spreads samples across a small disc, and one faint visibility fan is
// composited additively per sample. Where a point can reach every sample it is fully lit; where it can
// reach none it is in umbra; the boundary — reached by only some samples — is a soft PENUMBRA that
// widens with distance from the caster. The scene is static, so the render is golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

struct Box {
    float x, y, w, h;
};

void addBoxEdges(std::vector<game::Segment2>& out, const Box& b) {
    const math::vec2 p0{b.x, b.y}, p1{b.x + b.w, b.y};
    const math::vec2 p2{b.x + b.w, b.y + b.h}, p3{b.x, b.y + b.h};
    out.push_back(game::Segment2{p0, p1});
    out.push_back(game::Segment2{p1, p2});
    out.push_back(game::Segment2{p2, p3});
    out.push_back(game::Segment2{p3, p0});
}

// Draw a light as `samples` disc-sampled visibility fans, composited additively. size=0/samples=1 is a
// hard point light; a larger disc + more samples gives soft penumbrae. peakAlpha is per-fan so that the
// fans sum toward full brightness in regions that see the whole disc.
void drawSampledLight(render::Renderer& r, math::vec2 center, float discRadius, int samples,
                      render::Color color, float glowRadius, const std::vector<game::Segment2>& occ,
                      math::vec2 bmin, math::vec2 bmax) {
    const std::vector<math::vec2> pts = game::diskSamples(center, discRadius, samples);
    const float peak = color.a * (samples > 1 ? 1.3f / static_cast<float>(samples) : 1.0f);
    for (const math::vec2& sp : pts) {
        const auto poly = game::Visibility2D::compute(sp, occ, bmin, bmax);
        if (poly.size() < 3) {
            continue;
        }
        std::vector<render::PolyVertex> fan;
        fan.reserve(poly.size() + 2);
        fan.push_back(render::PolyVertex{sp.x, sp.y, render::Color{color.r, color.g, color.b, peak}});
        auto rim = [&](const math::vec2& p) {
            const float dx = p.x - sp.x, dy = p.y - sp.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            float f = 1.0f - d / glowRadius;
            if (f < 0.0f) f = 0.0f;
            f = f * f;
            return render::PolyVertex{p.x, p.y,
                                      render::Color{color.r, color.g, color.b, peak * f}};
        };
        for (const math::vec2& p : poly) {
            fan.push_back(rim(p));
        }
        fan.push_back(rim(poly.front()));
        r.drawPolygonFan(fan.data(), static_cast<uint32_t>(fan.size()), render::BlendMode::Additive);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SOFTSHADOW (area-light penumbra) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Soft 2D Shadows";
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

    const float W = static_cast<float>(cfg.width);
    const float H = static_cast<float>(cfg.height);
    const float top = 96.0f;
    const float bot = H - 20.0f;
    const float gap = 16.0f;
    const float panelW = (W - 3.0f * gap) * 0.5f;

    // Panel-local scene: a light upper-left, an occluder box in the middle, shadow cast to lower-right.
    const Box boxLocal{panelW * 0.46f, (top + bot) * 0.5f - 30.0f, 74.0f, 74.0f};
    const math::vec2 lightLocal{panelW * 0.28f, top + 80.0f};
    const render::Color warm{1.0f, 0.86f, 0.55f, 0.9f};
    const float glow = W * 0.62f;

    auto drawPanel = [&](float xoff, bool soft, const char* label) {
        const math::vec2 bmin{xoff + 6.0f, top};
        const math::vec2 bmax{xoff + panelW - 6.0f, bot};
        const Box box{xoff + boxLocal.x, boxLocal.y, boxLocal.w, boxLocal.h};
        std::vector<game::Segment2> occ;
        addBoxEdges(occ, box);
        const math::vec2 lc{xoff + lightLocal.x, lightLocal.y};

        // Panel backdrop (dark room).
        const render::Point2 bg[4] = {{bmin.x, bmin.y}, {bmax.x, bmin.y}, {bmax.x, bmax.y}, {bmin.x, bmax.y}};
        renderer->drawConvexPolygon(bg, 4, render::Color{0.04f, 0.04f, 0.06f, 1.0f});

        drawSampledLight(*renderer, lc, soft ? 30.0f : 0.0f, soft ? 24 : 1, warm, glow, occ, bmin, bmax);

        // Solid box on top.
        const render::Point2 quad[4] = {
            {box.x, box.y}, {box.x + box.w, box.y}, {box.x + box.w, box.y + box.h}, {box.x, box.y + box.h}};
        renderer->drawConvexPolygon(quad, 4, render::Color{0.12f, 0.13f, 0.16f, 1.0f});

        // Light marker: a small disc for the area light, a dot for the point light.
        if (soft) {
            const int n = 18;
            std::vector<render::Point2> disc(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) {
                const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
                disc[static_cast<size_t>(i)] = {lc.x + std::cos(a) * 30.0f, lc.y + std::sin(a) * 30.0f};
            }
            renderer->drawConvexPolygon(disc.data(), static_cast<uint32_t>(n),
                                        render::Color{1.0f, 1.0f, 1.0f, 0.5f});
        }
        const render::Point2 dot[4] = {
            {lc.x - 4, lc.y - 4}, {lc.x + 4, lc.y - 4}, {lc.x + 4, lc.y + 4}, {lc.x - 4, lc.y + 4}};
        renderer->drawConvexPolygon(dot, 4, render::Color{1.0f, 1.0f, 1.0f, 1.0f});

        font.drawText(*renderer, xoff + 12.0f, top + 8.0f, label, render::Color{1, 1, 1, 1}, 0.44f);
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.02f, 0.02f, 0.03f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            drawPanel(gap, false, "HARD  -  point light (crisp shadow edge)");
            drawPanel(gap * 2.0f + panelW, true, "SOFT  -  area light, 24 samples (penumbra)");

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SOFT 2D SHADOWS (AREA LIGHT)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 52.0f,
                          "same box + light; the area light's shadow edge feathers into a penumbra",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SOFTSHADOW shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
