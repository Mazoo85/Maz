// Maz Engine — "ORTHO3D" (3D orthographic camera, toward Godot's Camera3D Orthogonal projection)
// A parallel-projection camera: no perspective divide, so equal-size objects stay equal size at every
// depth and parallel edges never converge — the look of isometric strategy games, CAD, and 2.5D. The scene
// is a 7x7 field of lit cube columns of varying height (a central mound), viewed from a fixed isometric
// angle through math::orthographicSize. Static camera + static scene -> deterministic, golden-stable.
// The `cube` demo is the perspective counterpart; here every column reads the same width regardless of how
// far back it sits. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void addFace(std::vector<render::MeshVertex>& verts, std::vector<uint32_t>& idx, glm::vec3 a, glm::vec3 b,
             glm::vec3 c, glm::vec3 d, glm::vec3 n, glm::vec3 col) {
    const auto base = static_cast<uint32_t>(verts.size());
    const glm::vec3 corners[4] = {a, b, c, d};
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        const glm::vec3 p = corners[i];
        verts.push_back({p.x, p.y, p.z, n.x, n.y, n.z, col.r, col.g, col.b, uv[i][0], uv[i][1]});
    }
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// A box from (min) to (max) with per-box colour, 6 outward faces.
void addBox(std::vector<render::MeshVertex>& v, std::vector<uint32_t>& i, glm::vec3 lo, glm::vec3 hi,
            glm::vec3 col) {
    const glm::vec3 c000(lo.x, lo.y, lo.z), c100(hi.x, lo.y, lo.z), c110(hi.x, hi.y, lo.z),
        c010(lo.x, hi.y, lo.z), c001(lo.x, lo.y, hi.z), c101(hi.x, lo.y, hi.z), c111(hi.x, hi.y, hi.z),
        c011(lo.x, hi.y, hi.z);
    addFace(v, i, c001, c101, c111, c011, {0, 0, 1}, col);   // +z
    addFace(v, i, c100, c000, c010, c110, {0, 0, -1}, col);  // -z
    addFace(v, i, c100, c101, c001, c000, {0, -1, 0}, col);  // -y
    addFace(v, i, c010, c011, c111, c110, {0, 1, 0}, col);   // +y (top)
    addFace(v, i, c000, c001, c011, c010, {-1, 0, 0}, col);  // -x
    addFace(v, i, c101, c100, c110, c111, {1, 0, 0}, col);   // +x
}

glm::vec3 lerp3(glm::vec3 a, glm::vec3 b, float t) { return a + (b - a) * t; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ORTHO3D (orthographic camera) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Orthographic Camera";
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

    // A 1x1 white albedo so the mesh shows its per-vertex colours untinted.
    const uint8_t whitePx[4] = {255, 255, 255, 255};
    const render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    // Build the field of cube columns as ONE mesh (colour by height).
    std::vector<render::MeshVertex> verts;
    std::vector<uint32_t> idx;
    const int N = 7;
    const float spacing = 1.02f;
    const glm::vec3 lowColor(0.30f, 0.55f, 0.85f);
    const glm::vec3 highColor(1.0f, 0.85f, 0.45f);
    for (int gz = 0; gz < N; ++gz) {
        for (int gx = 0; gx < N; ++gx) {
            const float fx = static_cast<float>(gx - N / 2);
            const float fz = static_cast<float>(gz - N / 2);
            const float r2 = fx * fx + fz * fz;
            const float height = 0.5f + 2.4f * std::exp(-r2 / 7.0f); // central mound
            const float cx = fx * spacing;
            const float cz = fz * spacing;
            const float t = (height - 0.5f) / 2.4f;
            addBox(verts, idx, glm::vec3(cx - 0.46f, 0.0f, cz - 0.46f),
                   glm::vec3(cx + 0.46f, height, cz + 0.46f), lerp3(lowColor, highColor, t));
        }
    }
    const render::MeshHandle field = renderer->createMesh(
        verts.data(), static_cast<uint32_t>(verts.size()), idx.data(), static_cast<uint32_t>(idx.size()));

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

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect = bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        // Isometric-angle camera with an ORTHOGRAPHIC projection.
        const glm::vec3 eye(9.0f, 8.0f, 9.0f);
        const glm::mat4 proj = math::orthographicSize(11.0f, aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;
        const glm::mat4 model(1.0f);

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));
            render::Renderer::Material mat;
            mat.albedo = white;
            mat.roughness = 0.7f;
            mat.specular = 0.4f;
            renderer->drawMeshMaterial(field, glm::value_ptr(model), mat);

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ORTHOGRAPHIC CAMERA",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "parallel projection (math::orthographicSize): every column is the same width "
                          "regardless of depth — the isometric look, no vanishing point",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ORTHO3D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
