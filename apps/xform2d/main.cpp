// Maz Engine — "XFORM2D" (math::Transform2D, toward Godot's Transform2D)
// Transform2D is the 2x3 affine matrix behind every Node2D: it places, rotates, scales and skews things in
// 2D and converts between local and world space. This demo draws one asymmetric arrow shape under a gallery
// of transforms — identity, rotate, non-uniform scale, a composed rotate+scale, a skew, and an X-mirror —
// each over a faint copy of the untransformed arrow (so the effect is visible) with the transform's basis
// vectors drawn as a red/green gizmo (the columns of the matrix). Every outline vertex is mapped by
// Transform2D::xform and every gizmo axis by basisXform. Static transforms -> deterministic, golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>

using namespace maz;

namespace {

void thickLine(render::Renderer& r, math::vec2 a, math::vec2 b, float w, render::Color col) {
    const math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-3f) {
        return;
    }
    const math::vec2 n{-d.y / len * w * 0.5f, d.x / len * w * 0.5f};
    const render::Point2 quad[4] = {
        {a.x + n.x, a.y + n.y}, {b.x + n.x, b.y + n.y}, {b.x - n.x, b.y - n.y}, {a.x - n.x, a.y - n.y}};
    r.drawConvexPolygon(quad, 4, col);
}

// The reference arrow outline, in local pixel space (asymmetric so flips/rotations read clearly).
const math::vec2 kArrow[] = {{-38, -16}, {16, -16}, {16, -36}, {56, 0},  {16, 36}, {16, 16},
                             {-10, 16},  {-10, 34}, {-26, 34}, {-26, 16}, {-38, 16}};
const std::size_t kArrowN = sizeof(kArrow) / sizeof(kArrow[0]);

void drawShape(render::Renderer& r, const math::Transform2D& t, float w, render::Color col) {
    for (std::size_t i = 0; i < kArrowN; ++i) {
        const math::vec2 a = t.xform(kArrow[i]);
        const math::vec2 b = t.xform(kArrow[(i + 1) % kArrowN]);
        thickLine(r, a, b, w, col);
    }
}

void drawGizmo(render::Renderer& r, const math::Transform2D& t) {
    const math::vec2 o = t.origin;
    const float axis = 46.0f;
    thickLine(r, o, o + t.basisXform(math::vec2(axis, 0)), 3.0f, render::Color{0.95f, 0.4f, 0.4f, 1}); // X
    thickLine(r, o, o + t.basisXform(math::vec2(0, axis)), 3.0f, render::Color{0.5f, 0.9f, 0.5f, 1});   // Y
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("XFORM2D (math::Transform2D) starting");

    using math::Transform2D;
    using math::vec2;
    const float pi = 3.14159265358979323846f;

    struct Cell {
        const char* label;
        Transform2D local;
        vec2 center;
    };
    const Cell cells[] = {
        {"identity", Transform2D::identity(), vec2(230, 250)},
        {"rotate 35deg", Transform2D::rotation(35.0f * pi / 180.0f), vec2(640, 250)},
        {"scale (1.6, 0.7)", Transform2D::scaling(vec2(1.6f, 0.7f)), vec2(1050, 250)},
        {"rotate -30 + scale 1.3", Transform2D::compose(-30.0f * pi / 180.0f, vec2(1.3f, 1.3f), vec2(0, 0)),
         vec2(230, 540)},
        {"skew 0.5", Transform2D::compose(0.0f, vec2(1, 1), vec2(0, 0), 0.5f), vec2(640, 540)},
        {"mirror X", Transform2D::scaling(vec2(-1, 1)), vec2(1050, 540)},
    };

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Transform2D";
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

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.07f, 0.1f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TRANSFORM2D",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "the 2x3 affine matrix behind every Node2D - xform / basisXform / compose "
                          "(math::Transform2D)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            // Faint reference grid.
            const render::Color grid{0.13f, 0.14f, 0.18f, 1.0f};
            for (float gxp = 60.0f; gxp <= 1220.0f; gxp += 40.0f) {
                thickLine(*renderer, vec2(gxp, 96), vec2(gxp, 690), 1.0f, grid);
            }
            for (float gyp = 96.0f; gyp <= 690.0f; gyp += 40.0f) {
                thickLine(*renderer, vec2(60, gyp), vec2(1220, gyp), 1.0f, grid);
            }

            for (const Cell& c : cells) {
                const Transform2D world = Transform2D::translation(c.center) * c.local;
                // Ghost of the untransformed arrow at the cell centre.
                drawShape(*renderer, Transform2D::translation(c.center), 1.5f,
                          render::Color{0.3f, 0.32f, 0.4f, 1.0f});
                // The transformed arrow.
                drawShape(*renderer, world, 2.6f, render::Color{0.95f, 0.8f, 0.4f, 1.0f});
                // The transform's basis gizmo (its matrix columns).
                drawGizmo(*renderer, world);
                // Label.
                font.drawText(*renderer, c.center.x - 90.0f, c.center.y + 96.0f, c.label,
                              render::Color{0.8f, 0.85f, 0.95f, 1}, 0.34f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("XFORM2D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
