// Maz Engine — "_template" — the canonical starting point for a new (mobile-ready) Maz game. It is
// deliberately tiny: a single mover you drive with an on-screen virtual stick (touch) OR WASD/arrows
// (keyboard), rendered each frame as a square. Copy this directory, rename the target, and grow your game
// from here.
//
// What makes it mobile-shaped — the pieces every phone/tablet build needs and desktop games usually skip, all
// wired here so this doubles as the mobile-API showcase (each is a no-op on desktop, so the same code runs both):
//   1. The frame body is a `step` handed to platform::runMainLoop, so the OS can own the loop on iOS/Android/
//      web (a blocking while() there deadlocks). See WS3.
//   2. Input comes from input::VirtualControls (touch) unified with the keyboard, so the same build plays with
//      a thumb or a keyboard. See WS1/WS2.
//   3. Controls anchor inside the display safe area (platform::SafeArea) so they clear the notch/home bar.
//   4. Progress auto-saves the moment the app is backgrounded (PlatformBackend::setOnSuspend).
//   5. The loop is frame-capped by core::FramePacer, and the cap follows the battery/thermal state
//      (platform::PowerState) so a low phone doesn't burn power running flat out.
//   6. The action button fires a haptic buzz (PlatformBackend::triggerHaptic) for game feel.
//   Device orientation (PlatformBackend::orientation) is logged at boot; on a phone you'd re-anchor UI per turn.
//
// --headless / --frames N run the loop with no window (CI); ESC quits on desktop.

#include "maz/Engine.hpp"
#include "maz/core/FramePacer.hpp"
#include "maz/core/KeyValueStore.hpp"
#include "maz/input/VirtualControls.hpp"
#include "maz/io/VirtualFileSystem.hpp"
#include "maz/platform/DesktopBackend.hpp"
#include "maz/platform/Haptics.hpp"
#include "maz/platform/Orientation.hpp"
#include "maz/platform/PlatformBackend.hpp"
#include "maz/platform/PowerState.hpp"
#include "maz/platform/SafeArea.hpp"
#include "maz/platform/WebLoop.hpp"

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>

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

    // Power-aware frame pacing: cap the loop to a target fps so a phone doesn't burn battery running past the
    // display, and drop that cap automatically on low battery / low-power mode / thermal throttle. On desktop
    // the backend reports "plenty of power", so the cap stays at 60 and nothing changes. The pacer computes
    // how long to sleep after each frame's work; a real windowed build performs that sleep (below). See
    // core::FramePacer + platform::PowerState.
    core::FramePacer pacer;
    if (backend) {
        const double fpsCap = platform::recommendedFps(backend->powerState());
        pacer.setActiveFps(fpsCap);
        MAZ_LOG_INFO("_template: orientation=%s, fps cap=%.0f (power-adaptive)",
                     platform::orientationName(backend->orientation()), fpsCap);
    }

    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle whiteTex = renderer->createTexture(1, 1, white);

    // On-screen twin of a movement stick: a floating stick anchored to the lower-left, plus one action button
    // lower-right. Positioned in drawable pixels (touch/mouse coordinate space).
    uint32_t dw = 0, dh = 0;
    window.drawableSize(dw, dh);
    if (dw == 0) { dw = cfg.width; dh = cfg.height; }
    const float bw = static_cast<float>(dw);
    const float bh = static_cast<float>(dh);

    // Safe area: on a phone the notch / rounded corners / home-indicator eat into the drawable, so anchor
    // the controls inside the OS-reported safe rectangle instead of the raw screen edges (a stick pinned to
    // the very bottom-left would sit under the gesture bar). On desktop/headless the insets are zero, so the
    // safe rect is the whole drawable and every position below is unchanged — the code is identical on both.
    math::Rect2 safe(0.0f, 0.0f, bw, bh);
    if (backend) safe = platform::safeAreaRect(static_cast<int>(dw), static_cast<int>(dh), backend->safeAreaInsets());
    const float sx = safe.left(), sy = safe.top(), sw = safe.size.x, sh = safe.size.y;

    input::VirtualControls controls;
    controls.moveStick =
        input::VirtualStick(math::vec2(sx + sw * 0.18f, sy + sh * 0.75f), sh * 0.14f, /*floating=*/true);
    controls.moveStick.setActivationRadius(bw * 0.5f); // left half of the screen grabs the move stick
    controls.buttons.push_back(input::VirtualButton(math::vec2(sx + sw * 0.85f, sy + sh * 0.78f), sh * 0.09f));

    // The one piece of game state: a mover, in scene units, centered on screen.
    float posX = 0.0f, posY = 0.0f;
    const float speed = 40.0f; // units / second
    const float worldToPx = 12.0f;
    int flashes = 0;    // action-button presses (a trivial bit of gameplay to prove the button works)
    float flashT = 0.0f;
    int rendered = 0;

    // Auto-save on suspend: restore the mover from user:// at boot, then register a suspend hook that persists
    // it the moment the OS backgrounds the app. On desktop this fires when the window is minimized (driven by
    // syncLifecycle in the frame below); on iOS/Android the backend fires it on didEnterBackground/onPause —
    // exactly when a mobile game must save, because a backgrounded app can be killed without another chance.
    core::KeyValueStore save;
    if (desktop) {
        const std::string savePath = desktop->directory(platform::DirKind::UserData) + "mover.ini";
        save.load(savePath); // records the path for save(); empty if none yet
        posX = save.getFloat("x", 0.0f);
        posY = save.getFloat("y", 0.0f);
        backend->setOnSuspend([&] {
            save.set("x", posX);
            save.set("y", posY);
            save.save();
            MAZ_LOG_INFO("_template: autosaved on suspend (x=%.1f y=%.1f)", posX, posY);
        });
        backend->setOnResume([] { MAZ_LOG_INFO("_template: resumed from background"); });
    }

    auto frame = [&]() -> bool {
        const uint64_t frameStartNs = SDL_GetTicksNS(); // for power-aware frame pacing (below)
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

        // Action: the virtual button or SPACE triggers a brief flash — and a haptic buzz for game feel. On a
        // phone that vibrates the device; on desktop triggerHaptic is a no-op, so the same call is safe here.
        const bool act = controls.buttons[0].pressed() || input.keyPressed(SDL_SCANCODE_SPACE);
        if (act) {
            ++flashes;
            flashT = 0.4f;
            if (backend) backend->triggerHaptic(platform::HapticFeedback::ImpactMedium);
        }

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

        // Power-aware frame cap: re-read the recommended fps each frame (so a low-battery / thermal event
        // lowers it automatically) and sleep off the remainder of the budget after this frame's work. The
        // pacer's drift correction keeps the average on target. Headless CI skips the physical sleep so the
        // smoke test stays fast; a real windowed/mobile build performs it to save battery.
        if (backend) pacer.setActiveFps(platform::recommendedFps(backend->powerState()));
        const double workS = static_cast<double>(SDL_GetTicksNS() - frameStartNs) * 1e-9;
        const double sleepS = pacer.sleepFor(workS);
        if (sleepS > 0.0 && !cfg.headless) {
            SDL_DelayNS(static_cast<Uint64>(sleepS * 1e9));
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
