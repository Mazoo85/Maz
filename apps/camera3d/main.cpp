// Maz Engine — "CAMERA3D" (render::Camera3D, toward Godot's Camera3D projection API)
// Godot's Camera3D turns a 3D scene into screen pixels and back: unproject_position (world->screen),
// project_ray_origin/normal (screen->world ray), project_position, is_position_in_frustum. This demo drives
// ALL of them with the 2D renderer as the "display": a perspective Camera3D projects a 3D scene — a ground
// grid, RGB world axes, and a wireframe cube — down to 2D line/point draws via worldToScreen. Nearer edges
// read larger, farther ones smaller: real perspective, done by render::Camera3D, drawn as 2D. It also casts a
// ray from the viewport centre (screenToRay), intersects the ground plane, and marks the hit (unprojection);
// and it colours a scatter of world points green / red by isPointVisible (in-frustum vs behind/outside).
// Static camera + scene -> deterministic, golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>

using namespace maz;

namespace {

void dot(render::Renderer& r, math::vec2 p, float rad, render::Color col) {
    const int N = 14;
    render::Point2 poly[16];
    for (int i = 0; i < N; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(N) * 6.2831853f;
        poly[i] = {p.x + std::cos(a) * rad, p.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(poly, N, col);
}

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

// Draw a world-space segment by projecting both endpoints; skip it if either is behind the camera.
void worldLine(render::Renderer& r, const render::Camera3D& cam, math::vec3 a, math::vec3 b, float w,
               render::Color col) {
    const render::Projected pa = cam.worldToScreen(a);
    const render::Projected pb = cam.worldToScreen(b);
    if (!pa.inFront || !pb.inFront) {
        return;
    }
    thickLine(r, pa.screen, pb.screen, w, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CAMERA3D (render::Camera3D) starting");

    render::Camera3D cam;
    cam.viewportWidth = static_cast<float>(cfg.width);
    cam.viewportHeight = static_cast<float>(cfg.height);
    cam.lookAt(math::vec3(5.0f, 4.0f, 7.0f), math::vec3(0.0f, 0.5f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f));
    cam.perspective(glm::radians(55.0f), cam.viewportWidth / cam.viewportHeight, 0.1f, 18.0f);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Camera3D";
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

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.1f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam2d;
            cam2d.usePixelSpace = true;
            renderer->setCamera2D(cam2d);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CAMERA3D",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "a 3D scene projected to 2D by render::Camera3D - Godot's world<->screen "
                          "(unproject / ray / frustum)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            // Ground grid on the XZ plane (y=0), from -4..4, projected via the 3D camera.
            const render::Color gridCol{0.32f, 0.36f, 0.46f, 1.0f};
            for (int i = -4; i <= 4; ++i) {
                const float t = static_cast<float>(i);
                worldLine(*renderer, cam, math::vec3(t, 0, -4), math::vec3(t, 0, 4), 1.4f, gridCol);
                worldLine(*renderer, cam, math::vec3(-4, 0, t), math::vec3(4, 0, t), 1.4f, gridCol);
            }

            // RGB world axes from the origin.
            worldLine(*renderer, cam, math::vec3(0, 0, 0), math::vec3(2.5f, 0, 0), 3.0f,
                      render::Color{0.9f, 0.3f, 0.3f, 1});
            worldLine(*renderer, cam, math::vec3(0, 0, 0), math::vec3(0, 2.5f, 0), 3.0f,
                      render::Color{0.4f, 0.85f, 0.4f, 1});
            worldLine(*renderer, cam, math::vec3(0, 0, 0), math::vec3(0, 0, 2.5f), 3.0f,
                      render::Color{0.4f, 0.6f, 0.95f, 1});

            // A wireframe unit cube sitting on the ground (y 0..1.4, centred at origin).
            const float s = 0.7f, h = 1.4f;
            const math::vec3 c[8] = {{-s, 0, -s}, {s, 0, -s}, {s, 0, s}, {-s, 0, s},
                                     {-s, h, -s}, {s, h, -s}, {s, h, s}, {-s, h, s}};
            const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                      {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            const render::Color cubeCol{0.95f, 0.8f, 0.4f, 1.0f};
            for (const auto& e : edges) {
                worldLine(*renderer, cam, c[e[0]], c[e[1]], 2.4f, cubeCol);
            }
            // Cube corners as dots.
            for (const auto& corner : c) {
                const render::Projected p = cam.worldToScreen(corner);
                if (p.inFront) {
                    dot(*renderer, p.screen, 4.0f, render::Color{1.0f, 0.92f, 0.6f, 1.0f});
                }
            }

            // Scatter of world points coloured by frustum containment (green = visible, red = culled).
            // The last two probes sit beyond the (deliberately near) far plane, so they project on-screen but
            // fail the frustum test -> drawn red, proving isPointVisible culls them.
            const math::vec3 probes[] = {{3, 0.3f, 3},    {-3, 0.3f, -3}, {3.5f, 0.3f, -2}, {-2, 0.3f, 3.5f},
                                         {0, 3, 0},       {2, 0.3f, 2},   {0, 0.4f, -13},   {-3, 0.4f, -12}};
            for (const auto& w : probes) {
                const bool vis = cam.isPointVisible(w);
                const render::Projected p = cam.worldToScreen(w);
                if (p.inFront) {
                    dot(*renderer, p.screen, 6.0f,
                        vis ? render::Color{0.4f, 0.9f, 0.45f, 1} : render::Color{0.9f, 0.35f, 0.35f, 1});
                }
            }

            // Unproject: cast a ray from the viewport centre and mark where it meets the ground (y=0).
            const math::vec2 centre{cam.viewportWidth * 0.5f, cam.viewportHeight * 0.5f};
            const render::Ray3 ray = cam.screenToRay(centre);
            if (std::abs(ray.direction.y) > 1e-4f) {
                const float tHit = -ray.origin.y / ray.direction.y;
                if (tHit > 0.0f) {
                    const math::vec3 hit = ray.origin + ray.direction * tHit;
                    const render::Projected ph = cam.worldToScreen(hit);
                    if (ph.inFront) {
                        // Crosshair at screen centre + a ring at the ground hit.
                        thickLine(*renderer, math::vec2(centre.x - 12, centre.y),
                                  math::vec2(centre.x + 12, centre.y), 1.5f,
                                  render::Color{1, 1, 1, 0.6f});
                        thickLine(*renderer, math::vec2(centre.x, centre.y - 12),
                                  math::vec2(centre.x, centre.y + 12), 1.5f,
                                  render::Color{1, 1, 1, 0.6f});
                        dot(*renderer, ph.screen, 7.0f, render::Color{0.95f, 0.55f, 0.2f, 1});
                        worldLine(*renderer, cam, cam.eye(), hit, 1.0f,
                                  render::Color{0.95f, 0.55f, 0.2f, 0.5f});
                    }
                }
            }

            // Axis labels (projected).
            struct Lbl {
                math::vec3 at;
                const char* text;
                render::Color col;
            };
            const Lbl labels[] = {{{2.7f, 0, 0}, "X", {0.95f, 0.5f, 0.5f, 1}},
                                  {{0, 2.7f, 0}, "Y", {0.5f, 0.95f, 0.5f, 1}},
                                  {{0, 0, 2.7f}, "Z", {0.5f, 0.7f, 1.0f, 1}}};
            for (const auto& l : labels) {
                const render::Projected p = cam.worldToScreen(l.at);
                if (p.inFront) {
                    font.drawText(*renderer, p.screen.x, p.screen.y - 8.0f, l.text, l.col, 0.4f);
                }
            }

            // Side note.
            const char* notes[] = {
                "* worldToScreen projects each 3D point to a",
                "  pixel - the grid, axes and cube are 2D lines",
                "  between projected corners (perspective divide).",
                "",
                "* green / red dots = isPointVisible (in the",
                "  view frustum vs behind or off to the side).",
                "",
                "* the crosshair casts screenToRay from centre;",
                "  the orange ring is where it meets the ground",
                "  (unproject -> ray -> plane hit).",
            };
            for (std::size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
                font.drawText(*renderer, cam.viewportWidth - 400.0f, 96.0f + static_cast<float>(i) * 22.0f,
                              notes[i], render::Color{0.72f, 0.76f, 0.85f, 1}, 0.3f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CAMERA3D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
