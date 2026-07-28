// Maz Engine — "_template" — the canonical starting point for a new (mobile-ready) Maz game. It is
// deliberately tiny: a single mover you drive with an on-screen virtual stick (touch) OR WASD/arrows
// (keyboard), rendered each frame as a square. Copy this directory, rename the target, and grow your game
// from here.
//
// What makes it mobile-shaped — the two things every phone/tablet build needs and desktop games usually skip:
//   1. The frame body is a `step` handed to platform::runMainLoop, so the OS can own the loop on iOS/Android/
//      web (a blocking while() there deadlocks). See WS3.
//   2. Input comes from input::VirtualControls (touch) unified with the keyboard, so the same build plays with
//      a thumb or a keyboard. See WS1/WS2.
//
// --headless / --frames N run the loop with no window (CI); ESC quits on desktop.

#include "maz/Engine.hpp"
#include "maz/input/VirtualControls.hpp"
#include "maz/io/VirtualFileSystem.hpp"
#include "maz/platform/DesktopBackend.hpp"
#include "maz/platform/PlatformBackend.hpp"
#include "maz/platform/WebLoop.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <vector>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("_template headless=%d frames=%d", cfg.headless, cfg.frames);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Template";
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

    // Platform backend: pick the host's backend from the registry (DesktopBackend here; an AndroidBackend /
    // IOSBackend slots into the same seam on device), attach the window, boot it, and mount res:// -> assets
    // and user:// -> the per-user save dir. Games then do all file I/O through schemes, so packaging into an
    // APK/app bundle is a one-line mount change — no game-code paths to touch.
    platform::PlatformRegistry registry = platform::defaultRegistry();
    std::unique_ptr<platform::PlatformBackend> backend =
        registry.create(platform::DesktopBackend::hostDesktopId());
    io::VirtualFileSystem vfs;
    auto* desktop = dynamic_cast<platform::DesktopBackend*>(backend.get());
    if (desktop) {
        desktop->attachWindow(window.sdl());
        desktop->init();
        desktop->mountStandard(vfs);
        MAZ_LOG_INFO("_template: %s backend up; res://->%s user://->%s", desktop->name(),
                     desktop->directory(platform::DirKind::Assets).c_str(),
                     desktop->directory(platform::DirKind::UserData).c_str());
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle whiteTex = renderer->createTexture(1, 1, white);

    // On-screen twin of a movement stick: a floating stick anchored to the lower-left, plus one action button
    // lower-right. Positioned in drawable pixels (touch/mouse coordinate space).
    uint32_t dw = 0, dh = 0;
    window.drawableSize(dw, dh);
    if (dw == 0) { dw = cfg.width; dh = cfg.height; }
    const float bw = static_cast<float>(dw);
    const float bh = static_cast<float>(dh);
    input::VirtualControls controls;
    controls.moveStick =
        input::VirtualStick(math::vec2(bw * 0.18f, bh * 0.75f), bh * 0.14f, /*floating=*/true);
    controls.moveStick.setActivationRadius(bw * 0.5f); // left half of the screen grabs the move stick
    controls.buttons.push_back(input::VirtualButton(math::vec2(bw * 0.85f, bh * 0.78f), bh * 0.09f));

    // The one piece of game state: a mover, in scene units, centered on screen.
    float posX = 0.0f, posY = 0.0f;
    const float speed = 40.0f; // units / second
    const float worldToPx = 12.0f;
    int flashes = 0;    // action-button presses (a trivial bit of gameplay to prove the button works)
    float flashT = 0.0f;
    int rendered = 0;

    auto frame = [&]() -> bool {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }
        // Drive the platform lifecycle from OS focus/minimize — on mobile this is where you'd pause the sim
        // and release the GPU surface when the app is backgrounded.
        if (desktop) desktop->syncLifecycle(window.isMinimized());
        controls.update(input);

        // Combine keyboard and virtual-stick movement into one intent vector.
        math::vec2 mv = controls.moveStick.state().value;
        if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT)) mv.x -= 1.0f;
        if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT)) mv.x += 1.0f;
        if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP)) mv.y += 1.0f;   // up = +Y
        if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN)) mv.y -= 1.0f;
        float mlen = std::sqrt(mv.x * mv.x + mv.y * mv.y);
        if (mlen > 1.0f) { mv.x /= mlen; mv.y /= mlen; } // never faster than full tilt

        // Action: the virtual button or SPACE triggers a brief flash.
        const bool act = controls.buttons[0].pressed() || input.keyPressed(SDL_SCANCODE_SPACE);
        if (act) { ++flashes; flashT = 0.4f; }

        const float dt = 1.0f / 60.0f;
        posX += mv.x * speed * dt;
        posY += mv.y * speed * dt; // screen-up (+Y) moves the mover up; we invert Y at draw time
        if (flashT > 0.0f) flashT -= dt;

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            auto rect = [&](float x, float y, float w, float h, render::Color c) {
                render::SpriteDesc s;
                s.x = x; s.y = y; s.width = w; s.height = h; s.color = c;
                renderer->drawSprite(whiteTex, s);
            };

            // The mover, screen-centered (+Y up on screen -> subtract from center).
            const float cx = bw * 0.5f + posX * worldToPx;
            const float cy = bh * 0.5f - posY * worldToPx;
            const float sz = 40.0f;
            const render::Color body = flashT > 0.0f ? render::Color{1.0f, 0.9f, 0.4f, 1.0f}
                                                     : render::Color{0.4f, 0.8f, 1.0f, 1.0f};
            rect(cx - sz * 0.5f, cy - sz * 0.5f, sz, sz, body);

            // Draw the virtual controls (ring + knob + button) so touch has an on-screen target.
            auto ring = [&](math::vec2 c, float r, render::Color col) {
                rect(c.x - r, c.y - r, r * 2.0f, r * 2.0f, col);
            };
            const auto ms = controls.moveStick.state();
            ring(ms.base, controls.moveStick.radius(), render::Color{1, 1, 1, 0.12f});
            ring(ms.knob, controls.moveStick.radius() * 0.4f, render::Color{1, 1, 1, 0.30f});
            const auto& btn = controls.buttons[0];
            ring(btn.center(), btn.radius(),
                 btn.held() ? render::Color{1.0f, 0.85f, 0.4f, 0.55f} : render::Color{1, 1, 1, 0.18f});

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            window.requestClose();
        }
        return !window.shouldClose();
    };

    // Callback-driven loop: blocking while() on desktop here, OS-driven callback on iOS/Android/web.
    using FrameFn = decltype(frame);
    platform::runMainLoop([](void* u) -> bool { return (*static_cast<FrameFn*>(u))(); }, &frame);

    MAZ_LOG_INFO("_template shutting down after %d frames (%d flashes, renderer %s)", rendered, flashes,
                 renderer->isActive() ? "active" : "inactive");
    if (desktop) desktop->shutdown();
    renderer->shutdown();
    window.shutdown();
    return 0;
}
