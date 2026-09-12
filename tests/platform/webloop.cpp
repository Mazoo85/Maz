// tests/platform/webloop.cpp — verifies the native path of the web/native main-loop driver
// (platform::runMainLoopBounded / runMainLoop). The native path is what this headless box can prove: the
// step callback runs with the right user pointer, stops when it returns false, and honors the iteration cap.
// (The __EMSCRIPTEN__ path is compiled only under the Emscripten toolchain; see docs/WEB_BUILD.md.)
#include "maz/platform/WebLoop.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

namespace {

struct Game {
    int frames = 0;
    int stopAfter = 0; // step returns false once frames reaches this
};

bool step(void* user) {
    Game* g = static_cast<Game*>(user);
    ++g->frames;
    return g->frames < g->stopAfter;
}

bool forever(void* user) {
    Game* g = static_cast<Game*>(user);
    ++g->frames;
    return true; // never asks to stop
}

} // namespace

int main() {
    // --- 1. The loop stops when step returns false, passing the user pointer through each frame. ---
    {
        Game g;
        g.stopAfter = 5;
        const int ran = runMainLoopBounded(step, &g, 1000);
        CHECK(ran == 5, "loop ran exactly until step returned false");
        CHECK(g.frames == 5, "step saw the user pointer and advanced state each frame");
    }

    // --- 2. The iteration cap bounds a step that never stops. ---
    {
        Game g;
        const int ran = runMainLoopBounded(forever, &g, 8);
        CHECK(ran == 8, "iteration cap honored");
        CHECK(g.frames == 8, "step invoked exactly maxIterations times");
    }

    // --- 3. Zero iterations runs nothing. ---
    {
        Game g;
        g.stopAfter = 3;
        const int ran = runMainLoopBounded(step, &g, 0);
        CHECK(ran == 0 && g.frames == 0, "maxIterations 0 runs no frames");
    }

    // --- 4. runMainLoop (native path) blocks until step stops — equivalent to the bounded form. ---
    {
        Game g;
        g.stopAfter = 4;
        runMainLoop(step, &g); // native: blocking while-loop
        CHECK(g.frames == 4, "runMainLoop native path drives step to completion");
    }

    if (g_fail == 0) {
        std::printf("webloop: OK — native main-loop driver runs, stops, and bounds correctly.\n");
        return 0;
    }
    std::printf("webloop: %d failure(s).\n", g_fail);
    return 1;
}
