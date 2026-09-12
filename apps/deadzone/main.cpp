// Maz Engine — "DEADZONE" (input::analogVector / applyDeadzone, toward Godot's Input.get_vector)
// A raw thumbstick drifts near centre and reaches ~√2 at the diagonals, so naive movement creeps at
// rest and runs faster diagonally. Godot's get_vector applies a RADIAL deadzone, rescales the leftover
// magnitude (deadzone edge -> 0, full tilt -> 1), and clamps to the unit circle. This demo shows both:
// LEFT, a grid of raw stick samples with an arrow to each conditioned vector — samples inside the
// deadzone collapse to centre, the corners get pulled onto the circle; RIGHT, the 1-D applyDeadzone
// response curve (flat through the deadzone, then linear to ±1). Fixed inputs -> deterministic golden.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
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
    const int n = 14;
    render::Point2 pts[14];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

void ring(render::Renderer& r, math::vec2 c, float radius, float w, render::Color col) {
    const int n = 56;
    math::vec2 prev(0.0f); // seeded: only read once i > 0, but MSVC cannot see that (C4701)
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

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("DEADZONE (input::analogVector) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Analog Deadzone";
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

    const float dz = 0.25f;

    // Left panel: stick field. Right panel: response curve.
    const math::vec2 stickC(360.0f, 400.0f);
    const float stickR = 235.0f;

    const float px0 = 760.0f, px1 = 1200.0f;   // curve plot box (x)
    const float py0 = 250.0f, py1 = 560.0f;     // curve plot box (y)

    const render::Color kAxis{0.30f, 0.34f, 0.42f, 1.0f};
    const render::Color kRim{0.55f, 0.62f, 0.75f, 1.0f};
    const render::Color kDz{1.0f, 0.45f, 0.4f, 1.0f};
    const render::Color kRaw{0.42f, 0.46f, 0.55f, 1.0f};
    const render::Color kArrow{0.35f, 0.4f, 0.5f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ANALOG DEADZONE", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "radial deadzone + rescale + unit clamp (input::analogVector, Godot "
                          "Input.get_vector) — no drift, no faster diagonals",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.32f);

            // ---- Left: stick field ----------------------------------------------------------------
            thickLine(*renderer, {stickC.x - stickR - 20.0f, stickC.y},
                      {stickC.x + stickR + 20.0f, stickC.y}, 1.0f, kAxis);
            thickLine(*renderer, {stickC.x, stickC.y - stickR - 20.0f},
                      {stickC.x, stickC.y + stickR + 20.0f}, 1.0f, kAxis);
            ring(*renderer, stickC, stickR, 2.0f, kRim);          // unit circle
            ring(*renderer, stickC, stickR * dz, 1.5f, kDz);      // deadzone circle

            // A grid of raw stick samples, each drawn with an arrow to its conditioned position.
            for (int gy = -6; gy <= 6; ++gy) {
                for (int gx = -6; gx <= 6; ++gx) {
                    const math::vec2 raw(static_cast<float>(gx) / 5.0f, static_cast<float>(gy) / 5.0f);
                    const math::vec2 proc = input::analogVector(raw, dz);
                    const math::vec2 rawS(stickC.x + raw.x * stickR, stickC.y + raw.y * stickR);
                    const math::vec2 procS(stickC.x + proc.x * stickR, stickC.y + proc.y * stickR);
                    thickLine(*renderer, rawS, procS, 1.0f, kArrow);
                    dot(*renderer, rawS, 2.0f, kRaw);
                    const float mag =
                        std::sqrt(proc.x * proc.x + proc.y * proc.y); // 0..1 -> colour
                    dot(*renderer, procS, 3.2f, rgba(0.4f + 0.6f * mag, 0.9f - 0.5f * mag, 0.5f, 1.0f));
                }
            }
            font.drawText(*renderer, stickC.x - 150.0f, stickC.y + stickR + 34.0f,
                          "stick field: raw dot -> conditioned dot", rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            // ---- Right: 1-D response curve --------------------------------------------------------
            const render::Point2 box[4] = {{px0, py0}, {px1, py0}, {px1, py1}, {px0, py1}};
            renderer->drawConvexPolygon(box, 4, rgba(0.12f, 0.13f, 0.17f, 1));
            const float midY = (py0 + py1) * 0.5f;
            thickLine(*renderer, {px0, midY}, {px1, midY}, 1.0f, kAxis);        // output 0 line
            const float midX = (px0 + px1) * 0.5f;
            thickLine(*renderer, {midX, py0}, {midX, py1}, 1.0f, kAxis);        // input 0 line
            // Deadzone band on the input axis.
            const float bandL = midX - dz * (px1 - px0) * 0.5f;
            const float bandR = midX + dz * (px1 - px0) * 0.5f;
            const render::Point2 band[4] = {{bandL, py0}, {bandR, py0}, {bandR, py1}, {bandL, py1}};
            renderer->drawConvexPolygon(band, 4, rgba(1.0f, 0.45f, 0.4f, 0.12f));

            math::vec2 prev(0.0f); // seeded: only read once i > 0, but MSVC cannot see that (C4701)
            const int samples = 128;
            for (int i = 0; i <= samples; ++i) {
                const float in = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(samples);
                const float out = input::applyDeadzone(in, dz);
                const math::vec2 p(px0 + (in * 0.5f + 0.5f) * (px1 - px0),
                                   midY - out * (py1 - py0) * 0.5f);
                if (i > 0) {
                    thickLine(*renderer, prev, p, 2.5f, rgba(0.55f, 0.85f, 1.0f, 1));
                }
                prev = p;
            }
            font.drawText(*renderer, px0, py1 + 24.0f,
                          "applyDeadzone response (flat through deadzone, linear to +/-1)",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("DEADZONE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
