// Maz Engine — "NORMALS" (render::computeNormals, toward Godot's SurfaceTool.generate_normals)
// Given only positions + indices, computeNormals derives a smooth per-vertex normal by area-weighted
// averaging of the faces around each vertex — the attribute lighting needs but raw/imported geometry
// often lacks. This demo builds a procedural wavy heightfield grid (positions + triangle indices only),
// runs computeNormals, then draws — in a fixed 3D-to-2D projection so it stays byte-deterministic — the
// wireframe surface plus a short "hair" at every vertex pointing along its computed normal, coloured by
// direction (RGB = normal*0.5+0.5). The hairs fan smoothly with the surface curvature: they lean outward
// over the crests and tuck inward through the troughs, which is exactly what a correct smooth-normal
// field looks like. Static geometry -> deterministic, golden-stable. Run --headless / --frames N for CI.

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

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("NORMALS (render::computeNormals) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Smooth Normal Generation";
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

    // ---- Build a procedural wavy-grid heightfield (positions + indices only) ------------------------
    const int N = 22; // vertices per side
    auto vindex = [&](int ix, int iz) { return static_cast<std::uint32_t>(iz * N + ix); };

    std::vector<math::vec3> positions;
    positions.reserve(static_cast<std::size_t>(N * N));
    for (int iz = 0; iz < N; ++iz) {
        for (int ix = 0; ix < N; ++ix) {
            const float x = (static_cast<float>(ix) / static_cast<float>(N - 1) - 0.5f) * 2.0f;
            const float z = (static_cast<float>(iz) / static_cast<float>(N - 1) - 0.5f) * 2.0f;
            const float h = 0.32f * std::sin(x * 3.1f) * std::cos(z * 3.1f);
            positions.push_back(math::vec3(x, h, z));
        }
    }
    std::vector<std::uint32_t> indices;
    for (int iz = 0; iz < N - 1; ++iz) {
        for (int ix = 0; ix < N - 1; ++ix) {
            const std::uint32_t a = vindex(ix, iz);
            const std::uint32_t b = vindex(ix + 1, iz);
            const std::uint32_t c = vindex(ix + 1, iz + 1);
            const std::uint32_t d = vindex(ix, iz + 1);
            indices.insert(indices.end(), {a, c, b, a, d, c});
        }
    }

    // The star of the show: derive smooth vertex normals from raw geometry.
    const std::vector<math::vec3> normals = render::computeNormals(positions, indices);

    // ---- Fixed 3D->2D projection (deterministic; no 3D pipeline needed) -----------------------------
    const float yaw = 0.62f, pitch = 0.95f;
    const float cy_ = std::cos(yaw), sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float scale = 250.0f;
    const math::vec2 origin(640.0f, 400.0f);
    auto project = [&](const math::vec3& p) -> math::vec2 {
        // Rotate about Y (yaw) then X (pitch).
        const float x1 = p.x * cy_ + p.z * sy;
        const float z1 = -p.x * sy + p.z * cy_;
        const float y2 = p.y * cp - z1 * sp;
        return math::vec2(origin.x + x1 * scale, origin.y - y2 * scale);
    };

    const float hairLen = 0.16f;

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SMOOTH NORMAL GENERATION",
                          rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "render::computeNormals (Godot SurfaceTool.generate_normals): area-weighted "
                          "per-vertex normals from positions + indices alone",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // Wireframe: connect each vertex to its +X and +Z neighbours, shaded by height.
            for (int iz = 0; iz < N; ++iz) {
                for (int ix = 0; ix < N; ++ix) {
                    const math::vec3& p = positions[static_cast<std::size_t>(vindex(ix, iz))];
                    const float shade = 0.28f + 0.30f * (p.y / 0.32f * 0.5f + 0.5f);
                    const render::Color wire = rgba(shade * 0.7f, shade * 0.8f, shade, 0.9f);
                    const math::vec2 sp0 = project(p);
                    if (ix + 1 < N) {
                        thickLine(*renderer, sp0,
                                  project(positions[static_cast<std::size_t>(vindex(ix + 1, iz))]), 1.2f,
                                  wire);
                    }
                    if (iz + 1 < N) {
                        thickLine(*renderer, sp0,
                                  project(positions[static_cast<std::size_t>(vindex(ix, iz + 1))]), 1.2f,
                                  wire);
                    }
                }
            }

            // Normal hairs: a short segment along each vertex's computed normal, coloured by direction.
            for (std::size_t i = 0; i < positions.size(); ++i) {
                const math::vec3& p = positions[i];
                const math::vec3& n = normals[i];
                const math::vec3 tip(p.x + n.x * hairLen, p.y + n.y * hairLen, p.z + n.z * hairLen);
                const render::Color col =
                    rgba(n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, n.z * 0.5f + 0.5f, 1.0f);
                thickLine(*renderer, project(p), project(tip), 1.8f, col);
            }

            font.drawText(*renderer, 16.0f, 684.0f,
                          "each hair points along the generated normal (colour = direction); they fan "
                          "smoothly with the surface curvature",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("NORMALS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
