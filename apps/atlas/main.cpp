// Maz Engine — "ATLAS" (render::AtlasPacker skyline rectangle bin-packing, toward Godot's atlas importer)
// The layout step that turns a pile of separate sprites/glyphs into one packed texture atlas. A fixed set
// of ~70 rectangles (deterministic RNG seed) is packed into a single bin with the Skyline Bottom-Left
// heuristic; each placed rect is drawn filled with a per-index colour inside the bin outline, and the
// header reports how many packed and the resulting occupancy. Fixed inputs -> deterministic layout, so it
// is golden-stable. Run --headless / --frames N for CI.

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

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

// Simple HSV->RGB for distinct per-rect colours (h in [0,1)).
render::Color hsv(float h, float s, float v, float a) {
    const float i = std::floor(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    switch (static_cast<int>(i) % 6) {
    case 0: return rgba(v, t, p, a);
    case 1: return rgba(q, v, p, a);
    case 2: return rgba(p, v, t, a);
    case 3: return rgba(p, q, v, a);
    case 4: return rgba(t, p, v, a);
    default: return rgba(v, p, q, a);
    }
}

void box(render::Renderer& r, float x0, float y0, float x1, float y1, render::Color c) {
    const render::Point2 q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    r.drawConvexPolygon(q, 4, c);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ATLAS (render::AtlasPacker) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — AtlasPacker";
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

    // Generate a fixed set of rectangles and pack them (deterministic seed -> golden-stable).
    const int binW = 600;
    const int binH = 560;
    core::Random rng(0xA71A5ULL);
    std::vector<render::PackSize> sizes;
    sizes.reserve(80);
    for (int i = 0; i < 80; ++i) {
        sizes.push_back(render::PackSize{rng.range(24, 120), rng.range(20, 110)});
    }
    render::AtlasPacker packer(binW, binH);
    const std::vector<render::Placement> placed = packer.pack(sizes);
    int packedCount = 0;
    for (const render::Placement& p : placed) {
        if (p.placed) {
            ++packedCount;
        }
    }
    const int total = static_cast<int>(sizes.size());
    const float occ = packer.occupancy();

    char statStr[96];
    std::snprintf(statStr, sizeof(statStr), "packed %d / %d rects   occupancy %.1f%%", packedCount, total,
                  static_cast<double>(occ) * 100.0);

    // Bin origin on screen (1:1 pixel scale).
    const float ox = 40.0f;
    const float oy = 120.0f;

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ATLASPACKER", rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "skyline rectangle bin-packing for texture atlases "
                          "(render::AtlasPacker, Godot atlas importer)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);
            font.drawText(*renderer, 16.0f, 84.0f, statStr, rgba(0.85f, 0.9f, 0.6f, 1), 0.35f);

            // Bin background + outline.
            box(*renderer, ox - 3, oy - 3, ox + static_cast<float>(binW) + 3,
                oy + static_cast<float>(binH) + 3, rgba(0.5f, 0.55f, 0.65f, 1));
            box(*renderer, ox, oy, ox + static_cast<float>(binW), oy + static_cast<float>(binH),
                rgba(0.11f, 0.12f, 0.16f, 1));

            // Each placed rect: filled colour with a 1px darker inset border effect.
            for (std::size_t i = 0; i < placed.size(); ++i) {
                const render::Placement& p = placed[i];
                if (!p.placed) {
                    continue;
                }
                const float x0 = ox + static_cast<float>(p.x);
                const float y0 = oy + static_cast<float>(p.y);
                const float x1 = x0 + static_cast<float>(p.w);
                const float y1 = y0 + static_cast<float>(p.h);
                const float h = static_cast<float>((i * 7) % 60) / 60.0f;
                box(*renderer, x0, y0, x1, y1, rgba(0.05f, 0.05f, 0.07f, 1));        // border
                box(*renderer, x0 + 1.5f, y0 + 1.5f, x1 - 1.5f, y1 - 1.5f, hsv(h, 0.55f, 0.9f, 1)); // fill
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ATLAS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
