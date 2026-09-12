// Maz Engine — "BLENDSPACE" (animation blend spaces, toward Godot's AnimationTree BlendSpace2D)
// Four corner poses of a stick-figure skeleton are placed at the corners of a 2-D parameter square.
// anim::BlendSpace2D turns any point in that square into barycentric weights over the (up to three)
// nearest poses, and anim::blendPosesWeighted mixes them into one pose that drives the shared
// Skeleton hierarchy. The demo renders a grid of figures, one per sampled (x,y) cell, so you can see
// the pose morph smoothly from corner to corner — exactly what a Godot BlendSpace2D does for, say,
// 8-way locomotion. Static + deterministic, so the render is golden-stable. Run --headless/--frames.

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
anim::Skeleton makeFigure() {
    struct Def {
        int parent;
        float x, y;
    };
    // 0 hip, 1 chest, 2 head, 3/4 left arm, 5/6 right arm, 7/8 left leg, 9/10 right leg.
    const Def defs[] = {{-1, 0, 0},  {0, 0, 32},   {1, 0, 26},  {1, -6, 0},   {3, -22, -2}, {1, 6, 0},
                        {5, 22, -2}, {0, -9, -2},  {7, -2, -30}, {0, 9, -2},   {9, 2, -30}};
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

// A pose = the bind translations (unchanged) plus a Z-rotation per joint (radians). Only the listed
// joints are rotated; the rest stay at their rest orientation.
std::vector<anim::JointPose> makePose(const anim::Skeleton& skel, const std::array<float, 11>& zrot) {
    std::vector<anim::JointPose> pose(skel.jointCount());
    for (size_t i = 0; i < skel.jointCount(); ++i) {
        const math::mat4& b = skel.localBind(i);
        pose[i].translation = math::vec3(b[3][0], b[3][1], b[3][2]);
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
    MAZ_LOG_INFO("BLENDSPACE (animation blend space) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Animation Blend Space";
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

    // Four corner poses (bottom-left, bottom-right, top-left, top-right of the blend square).
    const std::vector<std::vector<anim::JointPose>> corners = {
        makePose(skel, {0, 0, 0, -0.4f, -0.3f, 0.4f, 0.3f, 0, 0, 0, 0}),   // arms down (idle)
        makePose(skel, {0, 0, 0, -2.4f, 0.0f, 2.4f, 0.0f, 0, 0, 0, 0}),    // arms up (reach)
        makePose(skel, {0, 0, 0, -1.3f, -0.2f, 1.3f, 0.2f, -0.4f, 0, 0.4f, 0}), // arms out, legs spread
        makePose(skel, {0, 0, 0, -1.7f, -0.5f, 0.6f, 0.4f, 0.5f, -0.3f, -0.5f, 0.3f}), // asymmetric run
    };

    anim::BlendSpace2D bs;
    const int p00 = bs.addPoint(math::vec2(-1, -1), 0);
    const int p10 = bs.addPoint(math::vec2(1, -1), 1);
    const int p01 = bs.addPoint(math::vec2(-1, 1), 2);
    const int p11 = bs.addPoint(math::vec2(1, 1), 3);
    bs.addTriangle(p00, p10, p11);
    bs.addTriangle(p00, p11, p01);

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);
    const int cols = 5, rows = 3;
    const float marginX = 120.0f, marginTop = 140.0f, marginBot = 80.0f;
    const float cellW = (sw - 2.0f * marginX) / static_cast<float>(cols - 1);
    const float cellH = (sh - marginTop - marginBot) / static_cast<float>(rows - 1);
    const float scale = 1.2f;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.10f, 0.11f, 0.14f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            std::vector<math::mat4> locals, globals;
            for (int cy = 0; cy < rows; ++cy) {
                for (int cx = 0; cx < cols; ++cx) {
                    const float bx = -1.0f + 2.0f * static_cast<float>(cx) / static_cast<float>(cols - 1);
                    const float by = -1.0f + 2.0f * static_cast<float>(cy) / static_cast<float>(rows - 1);

                    // Blend the corner poses by the blend-space weights at (bx,by).
                    const auto ws = bs.weights(math::vec2(bx, by));
                    std::vector<const std::vector<anim::JointPose>*> poses;
                    std::vector<float> weights;
                    for (const auto& e : ws) {
                        poses.push_back(&corners[static_cast<size_t>(e.id)]);
                        weights.push_back(e.weight);
                    }
                    std::vector<anim::JointPose> blended;
                    anim::blendPosesWeighted(poses, weights, blended);

                    anim::posesToLocals(blended, locals);
                    skel.computeGlobals(locals, globals);

                    const float ox = marginX + static_cast<float>(cx) * cellW;
                    const float oy = marginTop + static_cast<float>(cy) * cellH; // hip baseline
                    auto screen = [&](size_t j) {
                        return math::vec2(ox + globals[j][3][0] * scale, oy - globals[j][3][1] * scale);
                    };

                    // Colour shifts across the grid so the morph is easy to follow.
                    const float hx = static_cast<float>(cx) / static_cast<float>(cols - 1);
                    const float hy = static_cast<float>(cy) / static_cast<float>(rows - 1);
                    const render::Color bone{0.45f + 0.5f * hx, 0.55f + 0.35f * hy,
                                             0.95f - 0.4f * hx, 1.0f};

                    // Draw each bone parent->child.
                    static const int parents[11] = {-1, 0, 1, 1, 3, 1, 5, 0, 7, 0, 9};
                    for (size_t j = 1; j < 11; ++j) {
                        drawBone(*renderer, screen(static_cast<size_t>(parents[j])), screen(j), 5.0f,
                                 bone);
                    }
                    // Head as a small filled octagon.
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ANIMATION BLEND SPACE (2D)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "one skeleton, four corner poses blended barycentrically across the grid",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BLENDSPACE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
