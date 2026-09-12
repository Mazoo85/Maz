// Maz Engine — "BLENDTREE" (animation blend TREE, toward Godot's AnimationNodeBlendTree)
// One evaluable node graph drives a stick-figure skeleton. The tree nests three kinds of node:
//   * a 1-D BLEND SPACE over idle/walk/run, selected by a "gait" parameter (the lower body),
//   * an ADDITIVE layer (Add2) that lays a right-arm "wave" delta on top by a "wave" parameter,
//   * a BLEND2 that cross-fades that whole locomotion result into a "jump" pose by an "air" parameter.
// The demo renders a grid: columns sweep gait (idle -> walk -> run), rows sweep air (grounded -> jump),
// with the wave additive held on — so you can read the tree responding on both axes at once. This is
// exactly the shape Godot's AnimationTree uses to drive a character. Static + deterministic -> the
// render is golden-stable. Run --headless / --frames for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

// Build a simple 2-D humanoid: parents-before-children, bone offsets in a +Y-up local space.
// 0 hip, 1 chest, 2 head, 3/4 left arm, 5/6 right arm, 7/8 left leg, 9/10 right leg.
anim::Skeleton makeFigure() {
    struct Def {
        int parent;
        float x, y;
    };
    const Def defs[] = {{-1, 0, 0},  {0, 0, 32},  {1, 0, 26},   {1, -6, 0},  {3, -22, -2}, {1, 6, 0},
                        {5, 22, -2}, {0, -9, -2}, {7, -2, -30}, {0, 9, -2},  {9, 2, -30}};
    std::vector<anim::Joint> joints;
    joints.reserve(11);
    for (const Def& d : defs) {
        anim::Joint j;
        j.parent = d.parent;
        j.localBind = glm::translate(math::mat4(1.0f), math::vec3(d.x, d.y, 0.0f));
        joints.push_back(j);
    }
    return anim::Skeleton(std::move(joints));
}

// A full pose = the bind translations plus a Z-rotation per joint (radians).
anim::Pose makePose(const anim::Skeleton& skel, const std::array<float, 11>& zrot) {
    anim::Pose pose(skel.jointCount());
    for (size_t i = 0; i < skel.jointCount(); ++i) {
        const math::mat4& b = skel.localBind(i);
        pose[i].translation = math::vec3(b[3][0], b[3][1], b[3][2]);
        pose[i].scale = math::vec3(1.0f);
        pose[i].rotation = glm::angleAxis(zrot[i], math::vec3(0, 0, 1));
    }
    return pose;
}

// An ADDITIVE delta = zero translation, unit scale, a Z-rotation only on the listed joints. Add2
// layers this on a base pose (base.rotation * delta), so translations stay put and the arm just lifts.
anim::Pose makeDelta(const anim::Skeleton& skel, const std::array<float, 11>& zrot) {
    anim::Pose pose(skel.jointCount());
    for (size_t i = 0; i < skel.jointCount(); ++i) {
        pose[i].translation = math::vec3(0.0f);
        pose[i].scale = math::vec3(1.0f);
        pose[i].rotation = glm::angleAxis(zrot[i], math::vec3(0, 0, 1));
    }
    return pose;
}

// A thin quad between two screen points, for drawing a bone.
void drawBone(render::Renderer& r, math::vec2 a, math::vec2 b, float thick, render::Color col) {
    math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y * thick * 0.5f, d.x * thick * 0.5f);
    const render::Point2 quad[4] = {{a.x + n.x, a.y + n.y},
                                    {b.x + n.x, b.y + n.y},
                                    {b.x - n.x, b.y - n.y},
                                    {a.x - n.x, a.y - n.y}};
    r.drawConvexPolygon(quad, 4, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BLENDTREE (animation blend tree) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Animation Blend Tree";
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

    const anim::Skeleton skel = makeFigure();

    // The four whole-body poses + one additive delta the tree references (by index into `inputs`).
    const anim::Pose idle = makePose(skel, {0, 0, 0, -0.3f, -0.2f, 0.3f, 0.2f, 0.05f, 0, -0.05f, 0});
    const anim::Pose walk = makePose(skel, {0, 0, 0, 0.5f, -0.2f, -0.5f, 0.2f, 0.5f, -0.3f, -0.5f, 0.3f});
    const anim::Pose run =
        makePose(skel, {0.2f, 0.1f, 0, 1.0f, -0.4f, -1.0f, 0.4f, 0.9f, -0.5f, -0.9f, 0.5f});
    const anim::Pose jump =
        makePose(skel, {0, 0, 0, -2.5f, -0.3f, 2.5f, 0.3f, 1.0f, -1.2f, -1.0f, -1.2f});
    const anim::Pose wave = makeDelta(skel, {0, 0, 0, 0, 0, 1.6f, 0.8f, 0, 0, 0, 0}); // right arm up
    const std::vector<const anim::Pose*> inputs{&idle, &walk, &run, &jump, &wave};

    // Build the tree once:  root = Blend2( Add2( BlendSpace1(gait), wave, "wave" ), jump, "air" ).
    anim::BlendTree tree;
    const int gait = tree.addBlendSpace1("gait");
    tree.addBlendPoint(gait, 0.0f, tree.addInput(0)); // idle
    tree.addBlendPoint(gait, 1.0f, tree.addInput(1)); // walk
    tree.addBlendPoint(gait, 2.0f, tree.addInput(2)); // run
    const int waveIn = tree.addInput(4);
    const int layered = tree.addAdd2(gait, waveIn, "wave");
    const int jumpIn = tree.addInput(3);
    const int root = tree.addBlend2(layered, jumpIn, "air");
    tree.setRoot(root);
    tree.setParam("wave", 0.7f); // right-arm wave held on across the whole grid

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);
    const int cols = 5, rows = 3;
    const float marginX = 130.0f, marginTop = 150.0f, marginBot = 90.0f;
    const float cellW = (sw - 2.0f * marginX) / static_cast<float>(cols - 1);
    const float cellH = (sh - marginTop - marginBot) / static_cast<float>(rows - 1);
    const float scale = 1.15f;

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

            std::vector<math::mat4> locals, globals;
            anim::Pose out;
            for (int cy = 0; cy < rows; ++cy) {
                for (int cx = 0; cx < cols; ++cx) {
                    const float g = 2.0f * static_cast<float>(cx) / static_cast<float>(cols - 1); // 0..2
                    const float air = static_cast<float>(cy) / static_cast<float>(rows - 1);       // 0..1
                    tree.setParam("gait", g);
                    tree.setParam("air", air);
                    tree.evaluate(inputs, out);

                    anim::posesToLocals(out, locals);
                    skel.computeGlobals(locals, globals);

                    const float ox = marginX + static_cast<float>(cx) * cellW;
                    const float oy = marginTop + static_cast<float>(cy) * cellH; // hip baseline
                    auto screen = [&](size_t j) {
                        return math::vec2(ox + globals[j][3][0] * scale, oy - globals[j][3][1] * scale);
                    };

                    // Grounded figures shade cool->warm along gait; airborne rows tint toward violet.
                    const float hx = static_cast<float>(cx) / static_cast<float>(cols - 1);
                    const render::Color bone{0.45f + 0.5f * hx, 0.62f - 0.25f * air,
                                             0.95f - 0.35f * hx + 0.2f * air, 1.0f};

                    static const int parents[11] = {-1, 0, 1, 1, 3, 1, 5, 0, 7, 0, 9};
                    for (size_t j = 1; j < 11; ++j) {
                        drawBone(*renderer, screen(static_cast<size_t>(parents[j])), screen(j), 5.0f,
                                 bone);
                    }
                    const math::vec2 hpos = screen(2);
                    std::array<render::Point2, 8> head;
                    for (int k = 0; k < 8; ++k) {
                        const float ang = 6.2831853f * static_cast<float>(k) / 8.0f;
                        head[static_cast<size_t>(k)] =
                            render::Point2{hpos.x + std::cos(ang) * 8.0f, hpos.y + std::sin(ang) * 8.0f};
                    }
                    renderer->drawConvexPolygon(head.data(), 8, bone);
                }
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ANIMATION BLEND TREE",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "one node graph: gait blend-space -> +wave additive -> cross-fade to jump by air",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);
            font.drawText(*renderer, 16.0f, 74.0f,
                          "columns sweep gait (idle -> walk -> run)   rows sweep air (grounded -> jump)",
                          render::Color{0.62f, 0.68f, 0.8f, 1}, 0.4f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BLENDTREE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
