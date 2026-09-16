// film3d/wasm.cpp — the 3D renderer, as something a web page can call.
//
// SCRIPT FORGE promises one thing on its own front page: you type an idea, you watch the film, and it
// all happens on your machine with no account, no key and the wifi off. The 3D renderer broke that
// promise by existing only as a command-line program on a build machine — I could render a film and
// send somebody a picture of it, which is not the same thing at all.
//
// So the whole renderer compiles to WebAssembly and runs inside the page, next to the flat one. It was
// written headless from the first line — no Vulkan, no window, no display, nothing but arithmetic into
// an image buffer — and that turns out to be exactly the precondition for this. Nothing in the
// renderer had to change to make it work in a browser.
//
// The interface is deliberately tiny: hand it the reel as JSON, then ask for a frame at a time. The
// pixels come back in a buffer the module owns, so the caller copies them into a canvas and nothing is
// allocated per frame.
#include "Frame3D.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <map>
#include <string>

namespace {

maz::film::Reel g_reel;
std::map<std::string, film3d::Cast> g_cast;
film3d::Cache g_cache;
maz::render::Image g_frame;
std::string g_error;
std::string g_title;
std::string g_genre;

} // namespace

extern "C" {

// Read a reel. Returns 1 if it is one, 0 if it is not, in which case maz3d_error says why.
EMSCRIPTEN_KEEPALIVE int maz3d_load(const char* json) {
    g_error.clear();
    if (json == nullptr) {
        g_error = "nothing to read";
        return 0;
    }
    g_reel = maz::film::parseReel(std::string(json));
    if (!g_reel.valid) {
        g_error = g_reel.error.empty() ? "not a film reel" : g_reel.error;
        return 0;
    }
    g_cast = film3d::castReel(g_reel);
    g_cache = film3d::Cache{};
    g_title = g_reel.title;
    g_genre = g_reel.genreLabel;
    return 1;
}

EMSCRIPTEN_KEEPALIVE const char* maz3d_error() { return g_error.c_str(); }
EMSCRIPTEN_KEEPALIVE const char* maz3d_title() { return g_title.c_str(); }
EMSCRIPTEN_KEEPALIVE const char* maz3d_genre() { return g_genre.c_str(); }
EMSCRIPTEN_KEEPALIVE double maz3d_duration() { return g_reel.duration; }
EMSCRIPTEN_KEEPALIVE int maz3d_shots() { return static_cast<int>(g_reel.shots.size()); }
EMSCRIPTEN_KEEPALIVE int maz3d_people() { return static_cast<int>(g_cast.size()); }

// The index of the shot on screen at this moment, or -1. The page uses it to know when a cut has
// happened without having to model the edit itself.
EMSCRIPTEN_KEEPALIVE int maz3d_shot_at(double seconds) {
    const maz::film::Shot* shot = maz::film::shotAt(g_reel, seconds);
    return shot == nullptr ? -1 : shot->index;
}

// Draw one frame and return a pointer to width * height * 4 bytes of RGBA, owned by the module and
// good until the next call. `supersample` is 1 to play and 2 to keep; `shadows` is 0 none, 1 hard,
// 2 soft; `shutter` is how far the shutter opens in hundredths of a frame, 50 being what a film
// camera does and 0 a stills camera.
//
// The shutter is passed in rather than left at its default because it is the one setting whose right
// answer differs between a page playing a film to time and a command line with all night to render
// it. Anything that calls this without saying gets no shutter, which is the safe answer for a caller
// that has not thought about its frame budget.
EMSCRIPTEN_KEEPALIVE const std::uint8_t* maz3d_render(double seconds, int width, int height,
                                                      int supersample, int shadows, int shutter) {
    if (!g_reel.valid || width < 8 || height < 8) {
        return nullptr;
    }
    film3d::Look look;
    look.supersample = supersample;
    look.shadows = shadows;
    look.shutter = shutter < 0 ? 0 : (shutter > 100 ? 100 : shutter);
    g_frame = film3d::drawFrame3D(g_reel, g_cast, seconds, width, height, look, &g_cache);
    return g_frame.data().data();
}

} // extern "C"
