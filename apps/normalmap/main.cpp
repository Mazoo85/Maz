// Maz Engine — "NORMALMAP" (normal-mapped 2D lighting, toward Godot Light2D normal maps)
// A flat surface painted one colour is given a NORMAL MAP — a field of dome bumps — and lit by three
// coloured point lights. Each cell of the surface is shaded on the CPU by how squarely its bump normal
// faces each light (game::shadeSurface), so the bumps read as three-dimensional: the side of each dome
// facing a light is bright, the far side falls into shadow, and the coloured pools overlap and mix. This
// is the per-texel normal response Godot's Light2D gets from a sprite normal map; earlier Maz lights
// (M89/M101) were flat pools with occluder shadows but no surface relief. Static lights -> deterministic.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    const int seg = 20;
    render::Point2 p[22];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

// The procedural normal map: a grid of dome bumps. Returns the unit surface normal at surface-local
// coords (u, v) in [0,1], with `bx`x`by` bumps across the field.
math::vec3 bumpNormal(float u, float v, int bx, int by) {
    const float fu = u * static_cast<float>(bx);
    const float fv = v * static_cast<float>(by);
    const float lx = (fu - std::floor(fu) - 0.5f) * 2.0f; // [-1,1] within the bump tile
    const float ly = (fv - std::floor(fv) - 0.5f) * 2.0f;
    const float r2 = lx * lx + ly * ly;
    if (r2 >= 1.0f) {
        return math::vec3(0.0f, 0.0f, 1.0f); // flat gaps between bumps
    }
    return glm::normalize(math::vec3(lx, ly, std::sqrt(1.0f - r2))); // hemisphere dome
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("NORMALMAP (normal-mapped 2D lighting) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Normal-Mapped 2D Lighting";
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

    // Surface region + shading setup.
    const float x0 = 20.0f, y0 = 92.0f, x1 = 1260.0f, y1 = 636.0f;
    const int cols = 96, rows = 44;
    const int bumpsX = 12, bumpsY = 6;
    const math::vec3 albedo(0.80f, 0.78f, 0.74f); // light stone; the colour comes from the lights
    const math::vec3 ambient(0.10f, 0.11f, 0.15f);

    std::vector<game::PointLight2D> lights;
    { // warm key light, upper-left
        game::PointLight2D L;
        L.pos = math::vec2(360.0f, 250.0f);
        L.height = 60.0f;
        L.color = math::vec3(1.0f, 0.82f, 0.5f);
        L.energy = 1.15f;
        L.range = 520.0f;
        lights.push_back(L);
    }
    { // cool fill light, upper-right
        game::PointLight2D L;
        L.pos = math::vec2(940.0f, 250.0f);
        L.height = 60.0f;
        L.color = math::vec3(0.45f, 0.68f, 1.0f);
        L.energy = 1.15f;
        L.range = 520.0f;
        lights.push_back(L);
    }
    { // magenta accent, lower-centre
        game::PointLight2D L;
        L.pos = math::vec2(650.0f, 560.0f);
        L.height = 48.0f;
        L.color = math::vec3(1.0f, 0.45f, 0.8f);
        L.energy = 1.0f;
        L.range = 430.0f;
        lights.push_back(L);
    }

    const float cw = (x1 - x0) / static_cast<float>(cols);
    const float ch = (y1 - y0) / static_cast<float>(rows);

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.05f, 0.05f, 0.07f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Shade every cell of the surface by its bump normal under all three lights.
            for (int j = 0; j < rows; ++j) {
                for (int i = 0; i < cols; ++i) {
                    const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(cols);
                    const float v = (static_cast<float>(j) + 0.5f) / static_cast<float>(rows);
                    const math::vec2 p(x0 + (static_cast<float>(i) + 0.5f) * cw,
                                       y0 + (static_cast<float>(j) + 0.5f) * ch);
                    const math::vec3 n = bumpNormal(u, v, bumpsX, bumpsY);
                    const math::vec3 c = game::shadeSurface(p, n, albedo, lights, ambient);
                    fillRect(*renderer, x0 + static_cast<float>(i) * cw,
                             y0 + static_cast<float>(j) * ch, cw + 0.5f, ch + 0.5f,
                             render::Color{c.x, c.y, c.z, 1.0f});
                }
            }

            // Mark each light's position with a dot in its own colour.
            for (const game::PointLight2D& L : lights) {
                fillCircle(*renderer, L.pos, 7.0f, render::Color{L.color.x, L.color.y, L.color.z, 1.0f});
                fillCircle(*renderer, L.pos, 3.0f, render::Color{1, 1, 1, 1});
            }

            font.drawText(*renderer, 16.0f, 12.0f,
                          "MAZ ENGINE  -  NORMAL-MAPPED 2D LIGHTING",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "one flat-coloured surface + a bump normal map, lit by three coloured point "
                          "lights - each dome is lit on the side facing a light",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("NORMALMAP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
