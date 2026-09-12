// Maz Engine — "SKELETON" (skeletal-animation demo)
// A tapered tube is bound to a chain of 8 bones (maz::anim::Skeleton). Each frame a travelling wave
// bends the joints, the skeleton produces skinning matrices, and the mesh vertices are skinned on
// the CPU (blended between the two nearest bones) and streamed through the dynamic-mesh path — so no
// new vertex format or shader is needed. The bones are drawn as a debug line chain over the
// deforming skin. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr int kJoints = 8;
constexpr float kSegLen = 0.9f;
constexpr float kTotalH = kJoints * kSegLen;

struct Weight {
    int j0, j1;
    float w0, w1;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SKELETON (skeletal-animation demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Skeletal Animation";
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

    // --- Skeleton: a straight chain of joints up +Y --------------------------------------------
    std::vector<anim::Joint> joints(kJoints);
    joints[0].parent = -1;
    joints[0].localBind = math::mat4(1.0f);
    for (int i = 1; i < kJoints; ++i) {
        joints[static_cast<size_t>(i)].parent = i - 1;
        joints[static_cast<size_t>(i)].localBind =
            glm::translate(math::mat4(1.0f), math::vec3(0, kSegLen, 0));
    }
    anim::Skeleton skel(joints);

    // --- Bind mesh: a tapered tube, each vertex weighted to its two nearest bones ---------------
    constexpr int kRings = 28, kSlices = 16;
    const float kBaseR = 0.85f;
    std::vector<render::MeshVertex> bind;
    std::vector<Weight> weights;
    std::vector<uint32_t> idx;
    for (int r = 0; r < kRings; ++r) {
        const float tt = static_cast<float>(r) / static_cast<float>(kRings - 1);
        const float y = tt * kTotalH;
        const float radius = kBaseR * (1.0f - 0.6f * tt);
        // Blend weights between the two bones bracketing this height.
        const float jf = y / kSegLen;
        int j0 = static_cast<int>(std::floor(jf));
        if (j0 < 0) j0 = 0;
        if (j0 > kJoints - 1) j0 = kJoints - 1;
        int j1 = j0 + 1 < kJoints ? j0 + 1 : j0;
        const float frac = jf - std::floor(jf);
        const float w1 = j1 != j0 ? frac : 0.0f;
        const Weight w{j0, j1, 1.0f - w1, w1};
        for (int s = 0; s < kSlices; ++s) {
            const float a = static_cast<float>(s) / static_cast<float>(kSlices) * 6.2831853f;
            const float cx = std::cos(a), sz = std::sin(a);
            render::MeshVertex v{};
            v.px = cx * radius;
            v.py = y;
            v.pz = sz * radius;
            v.nx = cx;
            v.ny = 0.0f;
            v.nz = sz;
            v.r = 0.5f + 0.4f * tt;
            v.g = 0.7f - 0.3f * tt;
            v.b = 0.9f - 0.4f * tt;
            v.u = static_cast<float>(s) / static_cast<float>(kSlices);
            v.v = tt;
            bind.push_back(v);
            weights.push_back(w);
        }
    }
    auto vindex = [](int r, int s) { return static_cast<uint32_t>(r * kSlices + (s % kSlices)); };
    for (int r = 0; r < kRings - 1; ++r) {
        for (int s = 0; s < kSlices; ++s) {
            const uint32_t a = vindex(r, s), b = vindex(r, s + 1);
            const uint32_t c = vindex(r + 1, s + 1), d = vindex(r + 1, s);
            idx.insert(idx.end(), {a, b, c, a, c, d});
        }
    }

    std::vector<render::MeshVertex> skinned = bind; // per-frame deformed copy
    render::MeshHandle mesh =
        renderer->createDynamicMesh(skinned.data(), static_cast<uint32_t>(skinned.size()),
                                    idx.data(), static_cast<uint32_t>(idx.size()));
    const uint8_t whitePx[4] = {235, 235, 245, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    render::SceneLighting lighting;
    lighting.ambient[0] = lighting.ambient[1] = lighting.ambient[2] = 0.4f;
    lighting.sunDir[0] = 0.5f;
    lighting.sunDir[1] = 0.7f;
    lighting.sunDir[2] = 0.6f;
    renderer->setLighting(lighting);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    std::vector<math::mat4> pose(kJoints), skin, globals;
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

        // Animate the pose: a travelling bend wave up the chain (rotate each joint about Z).
        for (int i = 0; i < kJoints; ++i) {
            const float angle = 0.32f * std::sin(t * 2.2f - static_cast<float>(i) * 0.7f);
            pose[static_cast<size_t>(i)] =
                skel.localBind(static_cast<size_t>(i)) *
                glm::rotate(math::mat4(1.0f), angle, math::vec3(0, 0, 1));
        }
        skel.computeSkinning(pose, skin);
        skel.computeGlobals(pose, globals);

        // CPU skinning: blend each bind vertex by its two bone skinning matrices.
        for (size_t vi = 0; vi < bind.size(); ++vi) {
            const Weight& w = weights[vi];
            const math::vec4 p(bind[vi].px, bind[vi].py, bind[vi].pz, 1.0f);
            const math::vec3 n(bind[vi].nx, bind[vi].ny, bind[vi].nz);
            const math::vec4 sp =
                skin[static_cast<size_t>(w.j0)] * p * w.w0 + skin[static_cast<size_t>(w.j1)] * p * w.w1;
            const math::vec3 sn = glm::normalize(
                math::mat3(skin[static_cast<size_t>(w.j0)]) * n * w.w0 +
                math::mat3(skin[static_cast<size_t>(w.j1)]) * n * w.w1);
            skinned[vi].px = sp.x;
            skinned[vi].py = sp.y;
            skinned[vi].pz = sp.z;
            skinned[vi].nx = sn.x;
            skinned[vi].ny = sn.y;
            skinned[vi].nz = sn.z;
        }
        renderer->updateMesh(mesh, skinned.data(), static_cast<uint32_t>(skinned.size()));

        const glm::vec3 eye(std::sin(t * 0.25f) * 12.0f, kTotalH * 0.5f + 1.0f,
                            std::cos(t * 0.25f) * 12.0f);
        const glm::mat4 proj = math::perspective(glm::radians(50.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view =
            glm::lookAt(eye, glm::vec3(0.0f, kTotalH * 0.45f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.10f, 0.12f, 0.17f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            renderer->drawMesh(mesh, glm::value_ptr(glm::mat4(1.0f)), white);

            // Draw the skeleton over the skin: a line from each joint to its parent.
            const float boneCol[4] = {1.0f, 0.85f, 0.3f, 1.0f};
            for (int i = 1; i < kJoints; ++i) {
                const math::vec4 a = globals[static_cast<size_t>(skel.parent(static_cast<size_t>(i)))] *
                                     math::vec4(0, 0, 0, 1);
                const math::vec4 b = globals[static_cast<size_t>(i)] * math::vec4(0, 0, 0, 1);
                const float pa[3] = {a.x, a.y, a.z};
                const float pb[3] = {b.x, b.y, b.z};
                renderer->drawLine(pa, pb, boneCol);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SKELETAL ANIMATION",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "%d-bone chain skinning %zu vertices (CPU)", kJoints,
                          bind.size());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.8f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SKELETON shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
