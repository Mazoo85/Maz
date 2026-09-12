// Maz Engine — "ACTIONS" (input action-mapping demo)
// Gameplay never touches a scancode: an input::ActionMap turns raw device state into named intents
// (MoveX / MoveY axes, Fire / Dash buttons), each bound to keyboard AND gamepad sources. An avatar is
// driven purely by those actions — velocity from the axes, a speed boost while Dash is held, a flash
// spawned when Fire fires. To keep the golden reproducible the demo feeds a deterministic SCRIPTED
// input (a pure function of the fixed-step clock) into the same map, OR'd with the real keyboard/pad —
// so it plays itself for the screenshot yet is fully playable (WASD/arrows move, Space fires, Shift
// dashes). A HUD shows each action's live state. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.85f ? (1.0f - d) / 0.15f : 1.0f);
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

// Deterministic scripted keyboard state as a pure function of sim time (drives the golden capture).
bool scriptedDown(input::Device d, int code, float t) {
    if (d != input::Device::Key) return false;
    const bool right = std::sin(t * 1.3f) > 0.0f;
    const bool downMove = std::sin(t * 0.9f + 1.0f) > 0.0f;
    const bool fire = std::fmod(t, 1.2f) < 0.15f;
    const bool dash = std::sin(t * 2.0f) > 0.4f;
    switch (code) {
    case SDL_SCANCODE_D: return right;
    case SDL_SCANCODE_A: return !right;
    case SDL_SCANCODE_S: return downMove;
    case SDL_SCANCODE_W: return !downMove;
    case SDL_SCANCODE_SPACE: return fire;
    case SDL_SCANCODE_LSHIFT: return dash;
    default: return false;
    }
}

struct Flash {
    float x, y, age;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ACTIONS (input action-mapping demo) starting");

    // --- Build the action map: each action bound to keyboard AND gamepad sources ------------------
    input::ActionMap map;
    map.bindAxisPair("MoveX", input::Device::Key, SDL_SCANCODE_A, input::Device::Key, SDL_SCANCODE_D);
    map.bindAxisPair("MoveX", input::Device::Key, SDL_SCANCODE_LEFT, input::Device::Key,
                     SDL_SCANCODE_RIGHT);
    map.bindAxisAnalog("MoveX", platform::pad::LeftX, 1.0f);
    map.bindAxisPair("MoveY", input::Device::Key, SDL_SCANCODE_W, input::Device::Key, SDL_SCANCODE_S);
    map.bindAxisPair("MoveY", input::Device::Key, SDL_SCANCODE_UP, input::Device::Key,
                     SDL_SCANCODE_DOWN);
    map.bindAxisAnalog("MoveY", platform::pad::LeftY, 1.0f);
    map.bindButton("Fire", input::Device::Key, SDL_SCANCODE_SPACE);
    map.bindButton("Fire", input::Device::PadButton, platform::pad::A);
    map.bindButton("Dash", input::Device::Key, SDL_SCANCODE_LSHIFT);
    map.bindButton("Dash", input::Device::PadButton, platform::pad::LeftShoulder);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Input Actions";
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
    render::TextureHandle disc = discTex(*renderer, 64);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    float t = 0.0f;
    float avX = static_cast<float>(cfg.width) * 0.5f;
    float avY = static_cast<float>(cfg.height) * 0.55f;
    std::vector<Flash> flashes;
    int fireCount = 0;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
        const float sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            t += dt;

            // Sampler: scripted input OR real device (so it self-plays yet stays playable).
            auto down = [&](input::Device d, int code) -> bool {
                bool real = false;
                switch (d) {
                case input::Device::Key: real = input.keyDown(code); break;
                case input::Device::MouseButton: real = input.mouseDown(code); break;
                case input::Device::PadButton: real = input.gamepadButtonDown(code); break;
                }
                return real || scriptedDown(d, code, t);
            };
            auto analog = [&](int ax) { return input.gamepadAxis(ax); };
            map.update(down, analog);

            const float speed = map.held("Dash") ? 560.0f : 300.0f;
            avX += map.axis("MoveX") * speed * dt;
            avY += map.axis("MoveY") * speed * dt;
            const float m = 120.0f;
            avX = avX < m ? m : (avX > sw - m ? sw - m : avX);
            avY = avY < m + 40.0f ? m + 40.0f : (avY > sh - m ? sh - m : avY);

            if (map.pressed("Fire")) {
                flashes.push_back({avX, avY, 0.0f});
                ++fireCount;
            }
            for (Flash& f : flashes) f.age += dt;
            while (!flashes.empty() && flashes.front().age > 0.6f)
                flashes.erase(flashes.begin());
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Fire flashes: expanding fading rings.
            for (const Flash& f : flashes) {
                const float k = f.age / 0.6f;
                const float size = 40.0f + k * 130.0f;
                render::SpriteDesc d;
                d.x = f.x - size * 0.5f;
                d.y = f.y - size * 0.5f;
                d.width = size;
                d.height = size;
                d.color = render::Color{1.0f, 0.8f, 0.3f, (1.0f - k) * 0.7f};
                renderer->drawSprite(disc, d);
            }

            // Avatar: blue disc, brighter/larger while dashing.
            const bool dashing = map.held("Dash");
            const float av = dashing ? 74.0f : 58.0f;
            render::SpriteDesc a;
            a.x = avX - av * 0.5f;
            a.y = avY - av * 0.5f;
            a.width = av;
            a.height = av;
            a.color = dashing ? render::Color{0.6f, 0.9f, 1.0f, 1.0f}
                              : render::Color{0.4f, 0.7f, 1.0f, 1.0f};
            renderer->drawSprite(disc, a);

            // HUD: title + each action's live state.
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  INPUT ACTION MAP",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "actions bound to keyboard + gamepad  -  scripted self-play (WASD/arrows to drive)",
                          render::Color{0.7f, 0.85f, 1.0f, 1}, 0.42f);

            auto axisBar = [&](float y, const char* name, float value) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%-7s % .2f", name, static_cast<double>(value));
                font.drawText(*renderer, 20.0f, y, buf, render::Color{0.8f, 0.85f, 0.9f, 1}, 0.44f);
                // Centered bar: track + fill from center.
                const float bx = 190.0f, bw2 = 180.0f, cxb = bx + bw2 * 0.5f;
                render::SpriteDesc track;
                track.x = bx;
                track.y = y + 4.0f;
                track.width = bw2;
                track.height = 14.0f;
                track.color = render::Color{0.16f, 0.18f, 0.24f, 1};
                renderer->drawSprite(white, track);
                render::SpriteDesc fill;
                fill.height = 14.0f;
                fill.y = y + 4.0f;
                fill.width = std::fabs(value) * (bw2 * 0.5f);
                fill.x = value >= 0.0f ? cxb : cxb - fill.width;
                fill.color = render::Color{0.4f, 0.8f, 1.0f, 1};
                renderer->drawSprite(white, fill);
            };
            axisBar(96.0f, "MoveX", map.axis("MoveX"));
            axisBar(124.0f, "MoveY", map.axis("MoveY"));

            auto btnRow = [&](float y, const char* name, bool heldNow) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%-7s %s", name, heldNow ? "HELD" : "-");
                font.drawText(*renderer, 20.0f, y, buf,
                              heldNow ? render::Color{1.0f, 0.85f, 0.4f, 1}
                                      : render::Color{0.55f, 0.6f, 0.68f, 1},
                              0.44f);
            };
            btnRow(154.0f, "Fire", map.held("Fire"));
            btnRow(182.0f, "Dash", map.held("Dash"));

            char cnt[64];
            std::snprintf(cnt, sizeof(cnt), "Fire count: %d", fireCount);
            font.drawText(*renderer, 20.0f, 210.0f, cnt, render::Color{0.6f, 0.7f, 0.82f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ACTIONS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
