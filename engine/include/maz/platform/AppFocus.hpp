#pragma once

// maz::platform app-focus policy — what the engine should do each frame when the window loses
// keyboard focus or is minimized. A real engine doesn't keep burning a CPU core (and the laptop
// battery, and the GPU) rendering at full speed to a window nobody is looking at; it throttles the
// frame rate, optionally pauses the simulation, and skips rendering entirely while minimized (there
// is no visible surface to draw to). Godot exposes this via project settings (run/pause when
// unfocused, low-processor mode); here it's a small, explicit, deterministic policy.
//
// The SDL side (tracking focus/minimize from window events) lives in platform::Window; this header
// is the PURE decision — given the current activation state and a policy, decide whether to advance
// the sim, whether to render, and how long to sleep after the frame to hit the background frame
// rate. Pure and header-only, so it is unit-tested without a window or a GPU.
namespace maz::platform {

// Current window activation, updated from SDL focus/minimize events (see Window::activation()).
struct WindowActivation {
    bool focused = true;    // has keyboard/input focus
    bool minimized = false; // iconified — no visible drawable surface
};

// How the app should behave when it is not the active, visible window.
struct FocusPolicy {
    bool pauseSimWhenUnfocused = false; // freeze fixed-timestep game logic while unfocused
    bool throttleWhenUnfocused = true;  // cap the frame rate (sleep) while unfocused
    bool renderWhenUnfocused = true;    // keep drawing while unfocused-but-visible
    bool skipRenderWhenMinimized = true; // don't render while minimized (no surface to present)
    double backgroundFps = 10.0;        // target frame rate while throttled (<= 0 disables sleep)
};

// The per-frame decision the main loop acts on.
struct FrameAction {
    bool advanceSim = true;  // step game logic this frame
    bool render = true;      // draw + present this frame
    double throttleMs = 0.0; // sleep this many ms after the frame (0 = run at full speed)
};

// Milliseconds per frame for a target FPS (0 when fps <= 0, i.e. "no throttle").
inline double frameMsForFps(double fps) {
    return fps > 0.0 ? 1000.0 / fps : 0.0;
}

// Decide what to do this frame from the window's activation state and the chosen policy.
// Focused + visible → full speed, always render, never sleep. Unfocused-but-visible → optionally
// throttle and/or pause sim, still render unless told not to. Minimized → optionally skip render
// and throttle harder; sim still advances unless pauseSimWhenUnfocused (so timers/netcode keep up).
inline FrameAction decideFrame(const WindowActivation& act, const FocusPolicy& policy) {
    FrameAction a;

    if (act.minimized) {
        a.render = !policy.skipRenderWhenMinimized;
        a.advanceSim = !policy.pauseSimWhenUnfocused;
        a.throttleMs = frameMsForFps(policy.backgroundFps);
        return a;
    }

    if (!act.focused) {
        a.render = policy.renderWhenUnfocused;
        a.advanceSim = !policy.pauseSimWhenUnfocused;
        a.throttleMs = policy.throttleWhenUnfocused ? frameMsForFps(policy.backgroundFps) : 0.0;
        return a;
    }

    // Active and visible: run flat out.
    return a;
}

} // namespace maz::platform
