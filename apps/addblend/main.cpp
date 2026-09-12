// Maz Engine — "ADDBLEND" (additive/layered pose blending, toward Godot's AnimationNodeAdd2)
// A 2-bone arm plays one BASE pose (a fixed shoulder lift, elbow straight). An additive layer — an elbow
// bend stored relative to a straight-arm REFERENCE — is layered on top at five increasing weights. The
// shoulder never moves (its additive delta is zero, so the base is preserved) while the elbow folds
// progressively: exactly what additive blending buys you (layer a motion without disturbing the base).
// Each pose is run through the real anim::additiveBlend + Skeleton forward kinematics. Static →
// deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col, int segs = 32) {
    std::vector<render::Point2> pts;
    pts.reserve(static_cast<std::size_t>(segs) + 2);
    pts.push_back({c.x, c.y});
    for (int i = 0; i <= segs; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(segs) * 6.2831853f;
        pts.push_back({c.x + std::cos(a) * radius, c.y + std::sin(a) * radius});
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(pts.size()), col);
}

void drawBone(render::Renderer& r, math::vec2 a, math::vec2 b, float width, render::Color col) {
    render::PolylineStyle s;
    s.width = width;
    s.cap = render::CapMode::Round;
    const std::vector<math::vec2> tris = render::buildPolyline({a, b}, s);
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const render::Point2 tri[3] = {{tris[i].x, tris[i].y},
                                       {tris[i + 1].x, tris[i + 1].y},
                                       {tris[i + 2].x, tris[i + 2].y}};
        r.drawConvexPolygon(tri, 3, col);
    }
}

math::quat zrot(float degrees) {
    return glm::normalize(glm::angleAxis(glm::radians(degrees), math::vec3(0, 0, 1)));
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ADDBLEND (additive pose blending) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Additive Blend";
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
        font.load(*renderer, fontPath.c_str(), 32.0f);
    }

    // A 3-joint arm: shoulder -> elbow -> hand.
    std::vector<anim::Joint> joints = {{-1, math::mat4(1.0f)}, {0, math::mat4(1.0f)},
                                       {1, math::mat4(1.0f)}};
    anim::Skeleton skel(joints);
    const float upperLen = 96.0f;
    const float foreLen = 84.0f;

    auto makePose = [&](float shoulderDeg, float elbowDeg) {
        std::vector<anim::JointPose> p(3);
        p[0].translation = math::vec3(0, 0, 0);
        p[0].rotation = zrot(shoulderDeg);
        p[1].translation = math::vec3(upperLen, 0, 0);
        p[1].rotation = zrot(elbowDeg);
        p[2].translation = math::vec3(foreLen, 0, 0);
        return p;
    };

    // BASE: shoulder lifted to +40 deg, elbow straight. REFERENCE (additive's rest): straight arm, all
    // zero rotations. ADDITIVE: elbow bent to +100 deg, shoulder identical to reference (zero delta).
    const std::vector<anim::JointPose> base = makePose(40.0f, 0.0f);
    const std::vector<anim::JointPose> reference = makePose(0.0f, 0.0f);
    const std::vector<anim::JointPose> additive = makePose(0.0f, 100.0f);

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kText{0.90f, 0.93f, 1.0f, 1.0f};
    const render::Color kDim{0.68f, 0.74f, 0.86f, 1.0f};
    const render::Color kUpper{0.42f, 0.72f, 1.0f, 1.0f};
    const render::Color kFore{1.0f, 0.66f, 0.34f, 1.0f};
    const render::Color kJoint{0.95f, 0.95f, 1.0f, 1.0f};

    const float weights[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ADDITIVE BLEND (AnimationNodeAdd2)",
                          kText, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "one elbow-bend layer added onto a fixed base pose at rising weights", kDim,
                          0.34f);
            font.drawText(*renderer, 16.0f, 80.0f,
                          "shoulder stays put (zero delta = base preserved); only the elbow folds", kDim,
                          0.34f);

            for (int i = 0; i < 5; ++i) {
                const float w = weights[i];
                std::vector<anim::JointPose> pose;
                anim::additiveBlend(base, additive, reference, w, pose);

                std::vector<math::mat4> locals, globals;
                anim::posesToLocals(pose, locals);
                skel.computeGlobals(locals, globals);

                const float cx = 150.0f + static_cast<float>(i) * 244.0f;
                const float cy = 400.0f;
                auto screen = [&](const math::mat4& g) {
                    return math::vec2(cx + g[3].x, cy - g[3].y); // +Y up
                };
                const math::vec2 p0 = screen(globals[0]);
                const math::vec2 p1 = screen(globals[1]);
                const math::vec2 p2 = screen(globals[2]);

                drawBone(*renderer, p0, p1, 16.0f, kUpper);
                drawBone(*renderer, p1, p2, 13.0f, kFore);
                fillCircle(*renderer, p0, 11.0f, kJoint);
                fillCircle(*renderer, p1, 9.0f, kJoint);
                fillCircle(*renderer, p2, 7.0f, kJoint);

                char lbl[32];
                std::snprintf(lbl, sizeof(lbl), "weight %.2f", static_cast<double>(w));
                font.drawText(*renderer, cx - 46.0f, 560.0f, lbl, kText, 0.4f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ADDBLEND shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
