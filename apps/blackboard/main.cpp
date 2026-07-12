// Maz Engine — "BLACKBOARD" (behavior-tree blackboard + parallel/decorator nodes, toward Godot BT AI)
// A sentry's behavior tree drawn as a node graph, TWICE — once with the blackboard flag "visible" =
// false (PATROL) and once = true (ENGAGE). Each node box is colored by the status the REAL tree returns
// for it on a single tick (green Success, red Failure, amber Running, gray = not evaluated). So you can
// see the reactive selector switch branches purely from blackboard data: with no intruder it falls
// through to a PARALLEL patrol+scan (with a REPEAT decorator looping the route); the instant "visible"
// flips, the SEQUENCE engage branch pre-empts and the patrol branch is never even ticked. Static — a
// fixed single tick per tree — so the render is deterministic and golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>

using namespace maz;
namespace bt = maz::game::bt;

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

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

render::Color statusColor(int s) {
    switch (s) {
    case 0: return render::Color{0.30f, 0.78f, 0.42f, 1.0f}; // Success
    case 1: return render::Color{0.85f, 0.33f, 0.33f, 1.0f}; // Failure
    case 2: return render::Color{0.95f, 0.75f, 0.32f, 1.0f}; // Running
    default: return render::Color{0.28f, 0.30f, 0.36f, 1.0f}; // not evaluated
    }
}

struct BoxDef {
    float x, y;
    const char* label;
};

// Layout of the eight nodes within a diagram (local coordinates). Index == status-array index.
const BoxDef kBoxes[8] = {
    {215.0f, 30.0f, "SELECTOR"},  // 0 root
    {70.0f, 120.0f, "SEQUENCE"},  // 1 engage
    {10.0f, 210.0f, "visible?"},  // 2 condition
    {180.0f, 210.0f, "chase"},    // 3 action
    {360.0f, 120.0f, "PARALLEL"}, // 4 patrol+scan
    {300.0f, 210.0f, "REPEAT"},   // 5 repeater
    {300.0f, 300.0f, "goto wp"},  // 6 action
    {470.0f, 210.0f, "scan"},     // 7 action
};
// Parent -> child edges (index pairs).
const int kEdges[7][2] = {{0, 1}, {0, 4}, {1, 2}, {1, 3}, {4, 5}, {4, 7}, {5, 6}};
constexpr float kBoxW = 150.0f, kBoxH = 40.0f;

void drawTreeDiagram(render::Renderer& r, ui::Font& font, float ox, float oy, bool visible,
                     const char* title) {
    // Build the sentry tree with a Tap on every node so we read the real per-node status.
    int st[8];
    for (int& s : st) {
        s = -1;
    }
    bt::Blackboard bb;
    bb.set<bool>("visible", visible);
    auto running = [] { return bt::Status::Running; };

    bt::NodePtr root = bt::tap(
        &st[0],
        bt::selector(
            bt::tap(&st[1], bt::sequence(
                                bt::tap(&st[2], bt::condition([&] {
                                            return bb.getOr<bool>("visible", false);
                                        })),
                                bt::tap(&st[3], bt::action(running)))),
            bt::tap(&st[4], bt::parallel(bt::ParallelPolicy::RequireAll,
                                         bt::tap(&st[5], bt::repeater(0, bt::tap(&st[6],
                                                                                bt::action(running)))),
                                         bt::tap(&st[7], bt::action(running))))));
    bt::BehaviorTree tree(std::move(root));
    tree.tick();

    font.drawText(r, ox + 8.0f, oy - 26.0f, title, render::Color{0.9f, 0.93f, 1.0f, 1}, 0.42f);

    // Connectors first (under the boxes).
    for (const auto& e : kEdges) {
        const math::vec2 a(ox + kBoxes[e[0]].x + kBoxW * 0.5f, oy + kBoxes[e[0]].y + kBoxH);
        const math::vec2 b(ox + kBoxes[e[1]].x + kBoxW * 0.5f, oy + kBoxes[e[1]].y);
        thickLine(r, a, b, 2.0f, render::Color{0.4f, 0.43f, 0.5f, 1});
    }
    // Node boxes, tinted by their status.
    for (int i = 0; i < 8; ++i) {
        const float x = ox + kBoxes[i].x, y = oy + kBoxes[i].y;
        fillRect(r, x - 2.0f, y - 2.0f, kBoxW + 4.0f, kBoxH + 4.0f,
                 render::Color{0.08f, 0.09f, 0.12f, 1}); // outline
        fillRect(r, x, y, kBoxW, kBoxH, statusColor(st[i]));
        font.drawText(r, x + 12.0f, y + 11.0f, kBoxes[i].label, render::Color{0.05f, 0.06f, 0.09f, 1},
                      0.4f);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BLACKBOARD (behavior-tree AI) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Behavior-Tree Blackboard";
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

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            drawTreeDiagram(*renderer, font, 30.0f, 130.0f, false,
                            "PATROL  -  blackboard visible = false");
            drawTreeDiagram(*renderer, font, 650.0f, 130.0f, true,
                            "ENGAGE  -  blackboard visible = true");

            // Legend.
            const char* names[4] = {"Success", "Failure", "Running", "not evaluated"};
            const int codes[4] = {0, 1, 2, -1};
            for (int i = 0; i < 4; ++i) {
                const float x = 40.0f + static_cast<float>(i) * 200.0f;
                fillRect(*renderer, x, 560.0f, 22.0f, 22.0f, statusColor(codes[i]));
                font.drawText(*renderer, x + 30.0f, 563.0f, names[i],
                              render::Color{0.85f, 0.88f, 0.95f, 1}, 0.38f);
            }

            font.drawText(*renderer, 16.0f, 12.0f,
                          "MAZ ENGINE  -  BEHAVIOR-TREE BLACKBOARD + PARALLEL / DECORATORS",
                          render::Color{1, 1, 1, 1}, 0.62f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "one flag on the blackboard flips the sentry between patrol and engage; the "
                          "unused branch is never ticked",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BLACKBOARD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
