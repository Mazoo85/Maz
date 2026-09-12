// Maz Engine — "ANIMCLIP" (keyframe animation + blending demo)
// Two authored, looping clips drive an 8-bone skinned tube: "wave" (a travelling ripple of
// keyframed joint rotations) and "coil" (curl in and out). Each frame both clips are sampled at the
// current time (maz::anim::AnimClip, slerped rotations) and cross-faded by an oscillating weight
// (blendPoses); the blended pose skins the mesh via the Skeleton + dynamic-mesh path. Composes M68
// keyframe playback with M67 skinning. Run --headless / --frames N for CI.

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

anim::AnimClip makeWaveClip() {
    anim::AnimClip c;
    c.duration = 2.0f;
    c.loop = true;
    c.tracks.resize(kJoints);
    const float times[5] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
    for (int i = 0; i < kJoints; ++i) {
        for (float tk : times) {
            // Phase over a full 2*pi across the loop => value at t=0 and t=duration match (seamless).
            const float ang = 0.35f * std::sin(tk * 3.14159265f - static_cast<float>(i) * 0.7f);
            c.tracks[static_cast<size_t>(i)].rotation.push_back(
                {tk, glm::angleAxis(ang, math::vec3(0, 0, 1))});
        }
    }
    return c;
}

anim::AnimClip makeCoilClip() {
    anim::AnimClip c;
    c.duration = 2.0f;
    c.loop = true;
    c.tracks.resize(kJoints);
    for (int i = 0; i < kJoints; ++i) {
        c.tracks[static_cast<size_t>(i)].rotation = {
            {0.0f, glm::angleAxis(0.0f, math::vec3(0, 0, 1))},
            {1.0f, glm::angleAxis(0.42f, math::vec3(0, 0, 1))}, // curl mid-loop
            {2.0f, glm::angleAxis(0.0f, math::vec3(0, 0, 1))}};
    }
    return c;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ANIMCLIP (keyframe + blend demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Animation Clips";
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

    // Skeleton: a chain of joints up +Y (matches the skeleton demo).
    std::vector<anim::Joint> joints(kJoints);
    joints[0].parent = -1;
    joints[0].localBind = math::mat4(1.0f);
    std::vector<anim::JointPose> rest(kJoints);
    for (int i = 1; i < kJoints; ++i) {
        joints[static_cast<size_t>(i)].parent = i - 1;
        joints[static_cast<size_t>(i)].localBind =
            glm::translate(math::mat4(1.0f), math::vec3(0, kSegLen, 0));
        rest[static_cast<size_t>(i)].translation = math::vec3(0, kSegLen, 0);
    }
    anim::Skeleton skel(joints);

    // Bind mesh: tapered tube weighted to the two nearest bones (same as the skeleton demo).
    constexpr int kRings = 28, kSlices = 16;
    const float kBaseR = 0.85f;
    std::vector<render::MeshVertex> bind;
    std::vector<Weight> weights;
    std::vector<uint32_t> idx;
    for (int r = 0; r < kRings; ++r) {
        const float tt = static_cast<float>(r) / static_cast<float>(kRings - 1);
        const float y = tt * kTotalH;
        const float radius = kBaseR * (1.0f - 0.6f * tt);
        const float jf = y / kSegLen;
        int j0 = static_cast<int>(std::floor(jf));
        if (j0 < 0) j0 = 0;
        if (j0 > kJoints - 1) j0 = kJoints - 1;
        const int j1 = j0 + 1 < kJoints ? j0 + 1 : j0;
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
            v.r = 0.55f + 0.35f * tt;
            v.g = 0.55f - 0.2f * tt;
            v.b = 0.9f - 0.35f * tt;
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

    std::vector<render::MeshVertex> skinned = bind;
    render::MeshHandle mesh =
        renderer->createDynamicMesh(skinned.data(), static_cast<uint32_t>(skinned.size()),
                                    idx.data(), static_cast<uint32_t>(idx.size()));
    const uint8_t whitePx[4] = {235, 235, 245, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    const anim::AnimClip waveClip = makeWaveClip();
    const anim::AnimClip coilClip = makeCoilClip();

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

    std::vector<anim::JointPose> poseA, poseB, poseMix;
    std::vector<math::mat4> locals, skin, globals;
    float t = 0.0f;
    float blendW = 0.0f;
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

        // Sample both clips at the current time, then cross-fade by an oscillating weight.
        blendW = 0.5f + 0.5f * std::sin(t * 0.5f);
        waveClip.sample(t, rest, poseA);
        coilClip.sample(t, rest, poseB);
        anim::blendPoses(poseA, poseB, blendW, poseMix);
        anim::posesToLocals(poseMix, locals);
        skel.computeSkinning(locals, skin);
        skel.computeGlobals(locals, globals);

        for (size_t vi = 0; vi < bind.size(); ++vi) {
            const Weight& w = weights[vi];
            const math::vec4 p(bind[vi].px, bind[vi].py, bind[vi].pz, 1.0f);
            const math::vec3 n(bind[vi].nx, bind[vi].ny, bind[vi].nz);
            const math::vec4 sp = skin[static_cast<size_t>(w.j0)] * p * w.w0 +
                                  skin[static_cast<size_t>(w.j1)] * p * w.w1;
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

        renderer->setClearColor(render::Color{0.11f, 0.10f, 0.16f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            renderer->drawMesh(mesh, glm::value_ptr(glm::mat4(1.0f)), white);

            const float boneCol[4] = {1.0f, 0.8f, 0.35f, 1.0f};
            for (int i = 1; i < kJoints; ++i) {
                const math::vec4 a =
                    globals[static_cast<size_t>(skel.parent(static_cast<size_t>(i)))] *
                    math::vec4(0, 0, 0, 1);
                const math::vec4 b = globals[static_cast<size_t>(i)] * math::vec4(0, 0, 0, 1);
                const float pa[3] = {a.x, a.y, a.z};
                const float pb[3] = {b.x, b.y, b.z};
                renderer->drawLine(pa, pb, boneCol);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ANIMATION CLIPS + BLEND",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "blend  wave <-> coil : %.2f   (keyframed, slerped)",
                          static_cast<double>(blendW));
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.85f, 0.9f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ANIMCLIP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
