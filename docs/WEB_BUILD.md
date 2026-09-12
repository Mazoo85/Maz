# Web / WebAssembly Build

Maz games can run **in a web browser** by compiling to WebAssembly (WASM) with Emscripten. This page
covers the one code seam that matters, the exact build steps, and what is verified where.

## What is verified in this repo vs. what needs the toolchain

- **Verified here (native path):** the portability seam every desktop engine must cross to run in a
  browser — the main loop. See `engine/include/maz/platform/WebLoop.hpp` (`maz::platform::runMainLoop`)
  and its test `ctest -R webloop`. On desktop the loop blocks (`while (running) frame();`); in the browser
  that would hang the tab, so on `__EMSCRIPTEN__` it registers a per-frame callback and returns. Game code
  calls `runMainLoop(step, user)` once and works on both. **[VERIFIABLE HERE]**
- **Needs the Emscripten toolchain:** producing the actual `.wasm` / `.js` requires `emcc`/`em++`, which is
  not installed on the CI/cloud box. The build is scripted and documented below; run it on a machine with
  the emsdk. **[NEEDS TOOLCHAIN]**

## Why the main loop must change (the one gotcha)

A browser tab runs your code on a single cooperative thread. A normal C++ game loop that never returns
starves the browser's event loop and the page freezes. Emscripten's model is: don't loop — hand the
browser a callback it invokes once per animation frame, and return from `main`. `WebLoop.hpp` hides this:

```cpp
#include "maz/platform/WebLoop.hpp"

static bool frame(void* user) {
    auto* game = static_cast<Game*>(user);
    game->update();
    game->render();
    return !game->done;   // return false to stop
}

int main() {
    Game game;
    maz::platform::runMainLoop(frame, &game);  // blocks on desktop, browser-driven on web
    return 0;
}
```

## One-time setup (Emscripten SDK)

```
git clone https://github.com/emscripten-core/emsdk
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh      # puts emcc/em++/emcmake on PATH (run in each new shell)
```

## Build

From the repo root, with the emsdk sourced:

```
tools/build_web.sh <app-target> <output-dir>
# e.g.
tools/build_web.sh pong web-dist
```

This configures an Emscripten CMake build (`emcmake cmake`), builds the target, and links a browser
bundle using the HTML shell in `web/shell.html`. Key link flags (already in the script):

- `-sUSE_WEBGL2=1 -sFULL_ES3=1` — the renderer targets WebGL2 (GLES3) in the browser.
- `-sALLOW_MEMORY_GROWTH=1` — let the WASM heap grow as assets load.
- `--shell-file web/shell.html` — the page hosting the `<canvas id="canvas">` the engine draws into.
- `--preload-file assets` — bundle the game's assets into the `.data` file the page fetches.

## Run

Browsers won't `fetch()` a `.wasm` from a `file://` URL, so serve the output over HTTP:

```
cd web-dist
python3 -m http.server        # then open http://localhost:8000/
# or, if the emsdk is sourced:
emrun web-dist/index.html
```

## Renderer note

The desktop renderer is Vulkan. The browser target uses the WebGL2 path (GLES3); the 2D/sprite/text
pipeline and the fixed-timestep loop are portable as-is. Some desktop-only features (compute-shader
particle dispatch, certain post passes) fall back or are disabled under WebGL2 — those are called out in
`docs/GODOT_GAPS_ROADMAP.md`. This split is the same one every engine makes for the web.
