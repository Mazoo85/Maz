#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// maz::platform web/native main-loop driver — the ONE portability seam a desktop engine must cross to run in
// a browser. On desktop the game loop is an ordinary blocking `while (running) { frame(); }`. In a WebAssembly
// build that pattern DEADLOCKS: the browser is single-threaded and cooperative, so a C++ loop that never
// returns starves the event loop and the tab hangs. Emscripten's answer is to hand the browser a per-frame
// callback (`emscripten_set_main_loop_arg`) and RETURN from main; the browser then calls back once per
// animation frame. This header hides that split behind a single `runMainLoop(step, user)` so game code is
// written once: on `__EMSCRIPTEN__` it registers the callback, everywhere else it runs the blocking loop —
// identical `step` semantics on both. That is what lets the same Maz game target desktop and the web.
//
// Honest tag (see docs/GODOT_GAPS_ROADMAP.md + docs/WEB_BUILD.md): this abstraction + the native path are
// compiled and unit-tested HERE; producing the actual `.wasm`/`.js` requires the Emscripten toolchain
// (emcc/em++), which this box does not have — the documented `tools/build_web.sh` runs on a machine that
// does. [NATIVE PATH VERIFIABLE HERE / WASM NEEDS TOOLCHAIN]
namespace maz::platform {

// Per-frame callback. Return true to keep running, false to stop. `user` is an opaque pointer passed through
// untouched (typically your game/app struct).
using MainStepFn = bool (*)(void* user);

namespace detail {

struct LoopContext {
    MainStepFn step = nullptr;
    void* user = nullptr;
};

#ifdef __EMSCRIPTEN__
// Browser-driven trampoline: run one frame; if the game asked to stop, cancel the registered loop.
inline void emscriptenStep(void* ctxPtr) {
    LoopContext* ctx = static_cast<LoopContext*>(ctxPtr);
    if (!ctx->step(ctx->user)) {
        emscripten_cancel_main_loop();
    }
}
#endif

} // namespace detail

// Run the main loop. `fps <= 0` means "use the browser's requestAnimationFrame cadence" on web (ignored on
// native). On desktop this blocks until `step` returns false; on web it registers the callback and returns,
// with the browser driving subsequent frames.
inline void runMainLoop(MainStepFn step, void* user, int fps = 0) {
#ifdef __EMSCRIPTEN__
    static detail::LoopContext ctx;
    ctx.step = step;
    ctx.user = user;
    // simulate_infinite_loop = 1: emscripten unwinds the stack and keeps the runtime alive between frames.
    emscripten_set_main_loop_arg(detail::emscriptenStep, &ctx, fps, 1);
#else
    (void)fps;
    while (step(user)) {
    }
#endif
}

// Bounded native driver: run `step` until it returns false OR `maxIterations` is reached, and report how many
// frames ran. This is the deterministic, headless-testable form of the loop (and a handy way to run a fixed
// number of native frames for CI / golden capture) — the web path is inherently browser-driven.
inline int runMainLoopBounded(MainStepFn step, void* user, int maxIterations) {
    int n = 0;
    while (n < maxIterations) {
        ++n;
        if (!step(user)) break; // step still ran this frame; count it, then stop
    }
    return n;
}

} // namespace maz::platform
