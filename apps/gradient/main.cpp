// Maz Engine — "GRADIENT" (anim::Gradient, toward Godot's Gradient resource)
// Godot's Gradient is a colour ramp — sorted (offset, colour) stops sampled over [0,1] — behind
// CPUParticles' colour-over-lifetime, GradientTexture1D/2D, sky ramps, and health/heat tints. This draws a
// gallery of ramps as horizontal bars, each filled by sampling the gradient across its width:
//   * the SAME five-stop spectrum under all three interpolation modes (Constant = hard bands, Linear =
//     straight blend, Cubic = a smooth Catmull-Rom spline) so the modes sit side by side;
//   * a fire ramp (black→red→orange→yellow→white), a health ramp (red→amber→green), and an ocean ramp;
//   * a strip of the fire ramp BAKED to eight discrete swatches (Godot's GradientTexture1D), with the stop
//     positions ticked below the spectrum bar.
// The ramp maths is pure anim::Gradient; the app only samples + fills. Static gradients -> deterministic,
// golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color col) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, col);
}

// Draw a gradient as a bar by sampling it across `w` in `slices` vertical strips.
void drawRamp(render::Renderer& r, const anim::Gradient& g, float x, float y, float w, float h,
              int slices = 128) {
    const float sw = w / static_cast<float>(slices);
    for (int i = 0; i < slices; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(slices);
        fillRect(r, x + static_cast<float>(i) * sw, y, sw + 1.0f, h, g.sample(t));
    }
}

// Draw a baked N-swatch strip (discrete GradientTexture1D look).
void drawBaked(render::Renderer& r, const anim::Gradient& g, float x, float y, float w, float h,
               std::size_t n) {
    const auto ramp = g.bake(n);
    const float sw = w / static_cast<float>(n);
    for (std::size_t i = 0; i < n; ++i) {
        fillRect(r, x + static_cast<float>(i) * sw + 1.0f, y, sw - 2.0f, h, ramp[i]);
    }
}

anim::Gradient spectrum() {
    anim::Gradient g;
    g.addStop(0.0f, render::Color{0.9f, 0.1f, 0.15f, 1.0f});  // red
    g.addStop(0.25f, render::Color{0.95f, 0.8f, 0.15f, 1.0f}); // yellow
    g.addStop(0.5f, render::Color{0.2f, 0.8f, 0.3f, 1.0f});   // green
    g.addStop(0.75f, render::Color{0.2f, 0.6f, 0.95f, 1.0f}); // blue
    g.addStop(1.0f, render::Color{0.7f, 0.3f, 0.9f, 1.0f});   // violet
    return g;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GRADIENT (anim::Gradient) starting");

    anim::Gradient constant = spectrum();
    constant.interp = anim::GradientInterp::Constant;
    anim::Gradient linear = spectrum();
    linear.interp = anim::GradientInterp::Linear;
    anim::Gradient cubic = spectrum();
    cubic.interp = anim::GradientInterp::Cubic;

    anim::Gradient fire;
    fire.interp = anim::GradientInterp::Linear;
    fire.addStop(0.0f, render::Color{0.05f, 0.02f, 0.03f, 1.0f});
    fire.addStop(0.35f, render::Color{0.7f, 0.1f, 0.05f, 1.0f});
    fire.addStop(0.6f, render::Color{0.95f, 0.45f, 0.1f, 1.0f});
    fire.addStop(0.82f, render::Color{0.98f, 0.85f, 0.3f, 1.0f});
    fire.addStop(1.0f, render::Color{1.0f, 1.0f, 0.95f, 1.0f});

    anim::Gradient health;
    health.interp = anim::GradientInterp::Linear;
    health.addStop(0.0f, render::Color{0.85f, 0.15f, 0.15f, 1.0f});
    health.addStop(0.5f, render::Color{0.95f, 0.8f, 0.2f, 1.0f});
    health.addStop(1.0f, render::Color{0.2f, 0.8f, 0.3f, 1.0f});

    anim::Gradient ocean;
    ocean.interp = anim::GradientInterp::Cubic;
    ocean.addStop(0.0f, render::Color{0.02f, 0.05f, 0.15f, 1.0f});
    ocean.addStop(0.5f, render::Color{0.1f, 0.45f, 0.6f, 1.0f});
    ocean.addStop(1.0f, render::Color{0.6f, 0.9f, 0.85f, 1.0f});

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Gradient";
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

    const float barX = 60.0f, barW = 760.0f, barH = 44.0f;
    const render::Color label{0.82f, 0.86f, 0.94f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  GRADIENT",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "colour ramps - Godot's Gradient resource (particle colour-over-life, sky ramps, "
                          "health tints) (anim::Gradient)",
                          label, 0.4f);

            struct Row {
                const char* name;
                const anim::Gradient* g;
            };
            const Row rows[] = {
                {"spectrum - CONSTANT (hard bands)", &constant},
                {"spectrum - LINEAR (straight blend)", &linear},
                {"spectrum - CUBIC (Catmull-Rom spline)", &cubic},
                {"fire (black -> red -> orange -> white)", &fire},
                {"health (red -> amber -> green)", &health},
                {"ocean (cubic depth ramp)", &ocean},
            };

            const float startY = 100.0f, rowStep = 84.0f, barGap = 24.0f;
            float y = startY;
            for (const Row& row : rows) {
                font.drawText(*renderer, barX, y, row.name, label, 0.34f);
                drawRamp(*renderer, *row.g, barX, y + barGap, barW, barH);
                y += rowStep;
            }

            // Stop ticks under the spectrum (linear) bar: small notches at each stop offset.
            {
                const float ry = startY + barGap + barH; // bottom of the CONSTANT bar row
                for (std::size_t i = 0; i < linear.stopCount(); ++i) {
                    const float sx = barX + linear.stop(i).offset * barW;
                    fillRect(*renderer, sx - 1.0f, ry, 2.0f, 8.0f, render::Color{1, 1, 1, 0.8f});
                }
            }

            // Baked strip: the fire ramp as 8 discrete swatches (GradientTexture1D).
            font.drawText(*renderer, barX, y, "fire BAKED to 8 swatches (GradientTexture1D)", label, 0.34f);
            drawBaked(*renderer, fire, barX, y + barGap, barW, barH, 8);

            // Side note.
            const char* notes[] = {
                "* stops are (offset, colour) kept sorted;",
                "  sample(t) blends the two around t.",
                "",
                "* Constant = step, Linear = lerp,",
                "  Cubic = Catmull-Rom (smooth, may",
                "  slightly overshoot -> clamped to [0,1]).",
                "",
                "* bake(N) -> an N-colour ramp for a",
                "  GradientTexture / particle colour track.",
            };
            for (std::size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
                font.drawText(*renderer, 860.0f, 118.0f + static_cast<float>(i) * 24.0f, notes[i],
                              render::Color{0.72f, 0.76f, 0.85f, 1}, 0.3f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GRADIENT shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
