// Maz Engine — "PREFAB" (prefabs / instancing, toward Godot's PackedScene)
// One turret PREFAB — a chassis with a nested turret and barrel, each carrying exported properties — is
// authored once, then INSTANTIATED six times. The leftmost is the untouched template; the rest each apply
// per-node OVERRIDES (body colour, turret colour, barrel length, body size) so every copy differs while
// sharing one definition. Each robot is drawn from its resolved node tree, composing child positions onto
// their parent (chassis -> turret -> barrel). Instancing is pure data + deterministic, so the render is
// golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    if (rad < 1.0f) {
        rad = 1.0f;
    }
    const int seg = 22;
    render::Point2 p[24];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

render::Color toColor(math::vec4 c) { return render::Color{c.x, c.y, c.z, c.w}; }

// Author the base turret prefab once.
scene::Prefab makeTurretPrefab() {
    scene::Prefab pf;
    pf.root.name = "chassis";
    scene::setProp(pf.root.props, "pos", scene::PropValue::makeVec2(math::vec2(0.0f, 0.0f)));
    scene::setProp(pf.root.props, "w", scene::PropValue::makeFloat(88.0f));
    scene::setProp(pf.root.props, "h", scene::PropValue::makeFloat(52.0f));
    scene::setProp(pf.root.props, "color", scene::PropValue::makeColor(math::vec4(0.42f, 0.46f, 0.54f, 1.0f)));

    scene::PrefabNode turret;
    turret.name = "turret";
    scene::setProp(turret.props, "pos", scene::PropValue::makeVec2(math::vec2(0.0f, -40.0f)));
    scene::setProp(turret.props, "r", scene::PropValue::makeFloat(24.0f));
    scene::setProp(turret.props, "color", scene::PropValue::makeColor(math::vec4(0.6f, 0.64f, 0.72f, 1.0f)));

    scene::PrefabNode barrel;
    barrel.name = "barrel";
    scene::setProp(barrel.props, "pos", scene::PropValue::makeVec2(math::vec2(0.0f, -16.0f)));
    scene::setProp(barrel.props, "len", scene::PropValue::makeFloat(46.0f));
    scene::setProp(barrel.props, "w", scene::PropValue::makeFloat(12.0f));
    scene::setProp(barrel.props, "color", scene::PropValue::makeColor(math::vec4(0.25f, 0.27f, 0.32f, 1.0f)));

    turret.children.push_back(barrel);
    pf.root.children.push_back(turret);
    return pf;
}

// Draw a resolved instance tree, composing child offsets onto the chassis anchor.
void drawRobot(render::Renderer& r, render::TextureHandle white, const scene::PrefabNode& inst) {
    const math::vec2 base = scene::getVec2(inst.props, "pos");
    const float w = scene::getFloat(inst.props, "w", 88.0f);
    const float h = scene::getFloat(inst.props, "h", 52.0f);
    fillRect(r, white, base.x - w * 0.5f, base.y - h * 0.5f, w, h, toColor(scene::getColor(inst.props, "color")));
    // Tracks.
    fillRect(r, white, base.x - w * 0.5f, base.y + h * 0.5f - 6.0f, w, 12.0f,
             render::Color{0.18f, 0.19f, 0.22f, 1.0f});

    for (const scene::PrefabNode& turret : inst.children) {
        if (turret.name != "turret") {
            continue;
        }
        const math::vec2 tc = base + scene::getVec2(turret.props, "pos");
        for (const scene::PrefabNode& barrel : turret.children) {
            if (barrel.name != "barrel") {
                continue;
            }
            const math::vec2 bb = tc + scene::getVec2(barrel.props, "pos");
            const float len = scene::getFloat(barrel.props, "len", 46.0f);
            const float bw = scene::getFloat(barrel.props, "w", 12.0f);
            fillRect(r, white, bb.x - bw * 0.5f, bb.y - len, bw, len, toColor(scene::getColor(barrel.props, "color")));
        }
        const float rr = scene::getFloat(turret.props, "r", 24.0f);
        fillCircle(r, tc, rr, toColor(scene::getColor(turret.props, "color")));
        fillCircle(r, tc, rr * 0.45f, render::Color{0.15f, 0.16f, 0.2f, 1.0f});
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PREFAB (instancing) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Prefabs / Instancing";
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
    render::TextureHandle white = whiteTex(*renderer);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    const scene::Prefab prefab = makeTurretPrefab();

    // Six columns: the template (no overrides) then five customized instances.
    struct Spec {
        float x;
        const char* tag;
        math::vec4 body;
        math::vec4 turret;
        float barrelLen;
        float bodyW;
    };
    const float cy = 392.0f;
    const Spec specs[6] = {
        {150.0f, "template", math::vec4(0.42f, 0.46f, 0.54f, 1), math::vec4(0.6f, 0.64f, 0.72f, 1), 46.0f, 88.0f},
        {358.0f, "red / long", math::vec4(0.80f, 0.32f, 0.30f, 1), math::vec4(0.95f, 0.55f, 0.5f, 1), 70.0f, 88.0f},
        {566.0f, "green / stub", math::vec4(0.32f, 0.62f, 0.38f, 1), math::vec4(0.55f, 0.85f, 0.6f, 1), 26.0f, 88.0f},
        {774.0f, "gold / wide", math::vec4(0.85f, 0.68f, 0.28f, 1), math::vec4(0.98f, 0.85f, 0.5f, 1), 46.0f, 120.0f},
        {982.0f, "violet / narrow", math::vec4(0.55f, 0.4f, 0.78f, 1), math::vec4(0.75f, 0.62f, 0.95f, 1), 40.0f, 62.0f},
        {1160.0f, "cyan / tall", math::vec4(0.3f, 0.66f, 0.72f, 1), math::vec4(0.55f, 0.86f, 0.9f, 1), 58.0f, 80.0f},
    };

    // Resolve all instances once (deterministic).
    scene::PrefabNode instances[6];
    for (int i = 0; i < 6; ++i) {
        const Spec& s = specs[static_cast<std::size_t>(i)];
        scene::OverrideMap ov;
        ov.push_back({"", {{"pos", scene::PropValue::makeVec2(math::vec2(s.x, cy))}}});
        if (i != 0) { // the template column keeps its defaults (except position)
            scene::setProp(ov.back().second, "color", scene::PropValue::makeColor(s.body));
            scene::setProp(ov.back().second, "w", scene::PropValue::makeFloat(s.bodyW));
            ov.push_back({"turret", {{"color", scene::PropValue::makeColor(s.turret)}}});
            ov.push_back({"turret/barrel", {{"len", scene::PropValue::makeFloat(s.barrelLen)}}});
        }
        instances[i] = scene::instantiate(prefab, ov);
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PREFABS / INSTANCING",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "one turret prefab (chassis > turret > barrel) instanced six times with "
                          "per-node overrides (scene::instantiate)",
                          render::Color{0.78f, 0.83f, 0.93f, 1}, 0.32f);

            // Highlight the template column.
            fillRect(*renderer, white, 60.0f, 250.0f, 180.0f, 240.0f, render::Color{0.14f, 0.15f, 0.19f, 1.0f});

            for (int i = 0; i < 6; ++i) {
                const Spec& s = specs[static_cast<std::size_t>(i)];
                drawRobot(*renderer, white, instances[static_cast<std::size_t>(i)]);
                font.drawText(*renderer, s.x - 66.0f, cy + 70.0f, s.tag,
                              render::Color{0.82f, 0.86f, 0.93f, 1}, 0.3f);
            }

            font.drawText(*renderer, 60.0f, 220.0f, "TEMPLATE", render::Color{0.7f, 0.74f, 0.82f, 1}, 0.3f);
            font.drawText(*renderer, 340.0f, 220.0f, "INSTANCES (same prefab, different overrides)",
                          render::Color{0.7f, 0.74f, 0.82f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PREFAB shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
