// Maz Engine — "PROGRESS" (Range / ProgressBar, toward Godot's Range + ProgressBar)
// A panel of progress bars, each a ui::ProgressBar whose fill width is its ratio() and whose readout is
// its percent(). It shows the Range model driving several cases: plain fills at 25/60/100%, a health bar
// tinted red->green by its ratio, a custom-range (0..50) mana bar, and a stepped bar that snaps a set
// value onto its step grid. Fixed values -> deterministic, golden-stable. Run --headless / --frames N.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void quad(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

render::Color lerpColor(render::Color a, render::Color b, float t) {
    return render::Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                         a.a + (b.a - a.a) * t};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PROGRESS (Range / ProgressBar) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Range / ProgressBar";
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

    // ---- Build the bars from the Range model. --------------------------------------------------------
    struct Row {
        std::string name;
        ui::ProgressBar bar;
        render::Color fill;
        bool healthTint = false;
    };

    std::vector<Row> rows;
    auto make = [](double v, double mn = 0.0, double mx = 100.0, double step = 0.0) {
        ui::ProgressBar b;
        b.range.minValue = mn;
        b.range.maxValue = mx;
        b.range.step = step;
        b.setValue(v);
        return b;
    };

    rows.push_back({"Loading", make(25.0), render::Color{0.45f, 0.72f, 1.0f, 1.0f}, false});
    rows.push_back({"Download", make(60.0), render::Color{0.45f, 0.72f, 1.0f, 1.0f}, false});
    rows.push_back({"Install", make(100.0), render::Color{0.45f, 0.85f, 0.55f, 1.0f}, false});
    rows.push_back({"Health", make(35.0), render::Color{0, 0, 0, 1}, true});
    rows.push_back({"Mana (0-50)", make(30.0, 0.0, 50.0), render::Color{0.65f, 0.5f, 1.0f, 1.0f}, false});
    rows.push_back({"Stepped (25s)", make(60.0, 0.0, 100.0, 25.0),
                    render::Color{1.0f, 0.75f, 0.4f, 1.0f}, false}); // 60 snaps to 50 (nearest 25)

    const render::Color kRed{0.95f, 0.35f, 0.30f, 1.0f};
    const render::Color kGreen{0.45f, 0.90f, 0.5f, 1.0f};

    const float trackX = 340.0f;
    const float trackW = 620.0f;
    const float trackH = 36.0f;
    const float y0 = 176.0f;
    const float dy = 74.0f;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RANGE / PROGRESS BAR",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "each bar's fill = ProgressBar::fillFraction() (Range ratio), readout = "
                          "percent() — clamp / step / page all in ui::Range",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.32f);

            for (std::size_t i = 0; i < rows.size(); ++i) {
                const Row& row = rows[i];
                const float y = y0 + static_cast<float>(i) * dy;

                font.drawText(*renderer, 40.0f, y + 6.0f, row.name.c_str(),
                              render::Color{0.82f, 0.86f, 0.94f, 1}, 0.38f);

                // Track (background) + border.
                quad(*renderer, trackX - 2.0f, y - 2.0f, trackW + 4.0f, trackH + 4.0f,
                     render::Color{0.28f, 0.31f, 0.38f, 1.0f});
                quad(*renderer, trackX, y, trackW, trackH, render::Color{0.14f, 0.15f, 0.19f, 1.0f});

                // Fill = ratio * track width.
                const float frac = row.bar.fillFraction();
                render::Color fill = row.fill;
                if (row.healthTint) {
                    fill = lerpColor(kRed, kGreen, frac); // low health red, full health green
                }
                if (frac > 0.001f) {
                    quad(*renderer, trackX, y, trackW * frac, trackH, fill);
                }

                // Percent readout at the right.
                const std::string pct = std::to_string(row.bar.percent()) + "%";
                font.drawText(*renderer, trackX + trackW + 16.0f, y + 6.0f, pct.c_str(),
                              render::Color{0.9f, 0.93f, 1.0f, 1}, 0.38f);
            }

            font.drawText(*renderer, 40.0f, 664.0f,
                          "the stepped bar was set to 60 but snapped to 50 (step 25); health is tinted by "
                          "its own ratio",
                          render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PROGRESS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
