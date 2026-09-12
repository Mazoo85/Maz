// Maz Engine — "GLASS" (transparent-mesh demo)
// Opaque pillars behind a wall of overlapping colored glass panes. The panes are drawn with
// drawMeshTransparent: alpha-blended, depth-tested but not depth-writing, and sorted back-to-front
// so overlapping sheets composite correctly and you can see the pillars through the tint. Proves
// the renderer handles translucency, not just opaque geometry. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void addFace(std::vector<render::MeshVertex>& verts, std::vector<uint32_t>& idx, glm::vec3 a,
             glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n, glm::vec3 col) {
    const auto base = static_cast<uint32_t>(verts.size());
    const glm::vec3 corners[4] = {a, b, c, d};
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        const glm::vec3 p = corners[i];
        verts.push_back({p.x, p.y, p.z, n.x, n.y, n.z, col.r, col.g, col.b, uv[i][0], uv[i][1]});
    }
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// A unit cube centered at the origin, uniformly colored white (tint comes from the material).
render::MeshHandle makeCube(render::Renderer& r) {
    const glm::vec3 p[8] = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
    const glm::vec3 w(1.0f, 1.0f, 1.0f);
    std::vector<render::MeshVertex> v;
    std::vector<uint32_t> i;
    addFace(v, i, p[1], p[2], p[6], p[5], {1, 0, 0}, w);
    addFace(v, i, p[0], p[4], p[7], p[3], {-1, 0, 0}, w);
    addFace(v, i, p[3], p[7], p[6], p[2], {0, 1, 0}, w);
    addFace(v, i, p[0], p[1], p[5], p[4], {0, -1, 0}, w);
    addFace(v, i, p[4], p[5], p[6], p[7], {0, 0, 1}, w);
    addFace(v, i, p[1], p[0], p[3], p[2], {0, 0, -1}, w);
    return r.createMesh(v.data(), static_cast<uint32_t>(v.size()), i.data(),
                        static_cast<uint32_t>(i.size()));
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GLASS (transparent-mesh demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Transparency";
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

    render::MeshHandle cube = makeCube(*renderer);

    auto makeColorTex = [&](glm::vec3 c) {
        const uint8_t px[4] = {static_cast<uint8_t>(c.r * 255.0f), static_cast<uint8_t>(c.g * 255.0f),
                               static_cast<uint8_t>(c.b * 255.0f), 255};
        return renderer->createTexture(1, 1, px);
    };

    // Opaque backdrop: a row of tall colored pillars on a floor slab, so the glass has something to
    // occlude and tint.
    struct Pillar {
        glm::vec3 pos;
        render::TextureHandle tex;
    };
    std::vector<Pillar> pillars;
    const glm::vec3 pillarCols[5] = {{0.85f, 0.4f, 0.35f}, {0.45f, 0.75f, 0.45f}, {0.4f, 0.55f, 0.85f},
                                     {0.85f, 0.75f, 0.4f}, {0.7f, 0.45f, 0.8f}};
    for (int i = 0; i < 5; ++i) {
        pillars.push_back({glm::vec3((static_cast<float>(i) - 2.0f) * 3.0f, 2.5f, -4.0f),
                           makeColorTex(pillarCols[i])});
    }

    // Three overlapping translucent glass sheets at staggered depths and tints.
    struct Pane {
        glm::vec3 pos;
        glm::vec3 tint;
        float alpha;
    };
    Pane panes[3] = {
        {{-2.2f, 2.5f, 1.5f}, {0.35f, 0.8f, 1.0f}, 0.45f}, // cyan, front
        {{0.0f, 2.5f, 0.0f}, {1.0f, 0.45f, 0.55f}, 0.5f},  // rose, middle
        {{2.2f, 2.5f, -1.5f}, {0.5f, 1.0f, 0.55f}, 0.55f}, // green, back
    };
    render::TextureHandle paneTex[3];
    for (int i = 0; i < 3; ++i) {
        paneTex[i] = makeColorTex(panes[i].tint);
    }

    render::SceneLighting lighting;
    lighting.ambient[0] = lighting.ambient[1] = lighting.ambient[2] = 0.3f;
    lighting.sunColor[0] = lighting.sunColor[1] = lighting.sunColor[2] = 0.7f;
    renderer->setLighting(lighting);
    render::TextureHandle floorTex = makeColorTex(glm::vec3(0.55f, 0.57f, 0.6f));

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    float t = 0.0f;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            t += static_cast<float>(clock.fixedDelta());
        }

        // Camera orbits so the glass sweeps across the pillars from varying angles.
        const glm::vec3 eye(std::sin(t * 0.3f) * 11.0f, 4.0f, 9.0f + std::cos(t * 0.3f) * 3.0f);
        const glm::mat4 proj = math::perspective(glm::radians(55.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 2.2f, -2.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.12f, 0.14f, 0.20f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            // Floor slab.
            glm::mat4 floor = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.25f, -1.0f));
            floor = glm::scale(floor, glm::vec3(30.0f, 0.5f, 24.0f));
            renderer->drawMesh(cube, glm::value_ptr(floor), floorTex);

            // Opaque colored pillars.
            for (const Pillar& pl : pillars) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), pl.pos);
                m = glm::scale(m, glm::vec3(1.0f, 5.0f, 1.0f));
                renderer->drawMesh(cube, glm::value_ptr(m), pl.tex);
            }

            // Translucent glass panes — thin, tall, slowly swaying. drawMeshTransparent sorts and
            // blends them back-to-front over the opaque scene. The tint comes from a lit colored
            // albedo (not emissive), so the glass reads as colored, not glowing.
            for (int i = 0; i < 3; ++i) {
                const Pane& pane = panes[i];
                const float sway = 0.35f * std::sin(t * 0.8f + pane.pos.x);
                glm::mat4 m =
                    glm::translate(glm::mat4(1.0f), pane.pos + glm::vec3(0.0f, 0.0f, sway));
                m = glm::rotate(m, sway * 0.2f, glm::vec3(0, 1, 0));
                m = glm::scale(m, glm::vec3(3.4f, 4.6f, 0.12f));
                render::Renderer::Material mat;
                mat.albedo = paneTex[i];
                mat.specular = 0.5f;
                mat.roughness = 0.08f;
                renderer->drawMeshTransparent(cube, glm::value_ptr(m), mat, pane.alpha);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TRANSPARENCY",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "3 depth-sorted glass panes over opaque pillars",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GLASS shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
