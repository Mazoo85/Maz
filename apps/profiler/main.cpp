// Maz Engine — "PROFILER" (hierarchical CPU profiler view)
// A core::Profiler records nested timing zones and reports each zone's inclusive time (whole span)
// and self time (minus children). This demo feeds a fixed synthetic frame (deterministic timestamps,
// no wall clock) into the profiler every frame and draws the result as an indented bar chart — the
// classic profiler tree, each zone a bar whose width is its share of the frame, indented by nesting
// depth, labeled with inclusive/self milliseconds and call count. Because the sample frame is fixed,
// the render is golden-stable. In real code you'd wrap work in Profiler::ScopedZone instead.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

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

// One (name, start, end) span of a synthetic frame, in microseconds. begin/end order is derived from
// nesting: a span is opened at start and closed at end, with children fully inside their parent.
struct Span {
    const char* name;
    uint64_t start;
    uint64_t end;
    int depth;
};

// A representative 16 ms frame: update (physics + ai + 3x particles) then render (shadow + opaque +
// transparent + ui). Fixed values -> deterministic profiler output.
const Span kFrame[] = {
    {"frame", 0, 16000, 0},
    {"update", 0, 6200, 1},
    {"physics", 200, 3600, 2},
    {"ai", 3600, 5200, 2},
    {"particles", 5200, 6200, 2},
    {"render", 6200, 15600, 1},
    {"shadow", 6200, 8400, 2},
    {"opaque", 8400, 13200, 2},
    {"transparent", 13200, 14800, 2},
    {"ui", 14800, 15600, 2},
};

// Replay the synthetic frame into the profiler using begin/end in the correct nesting order.
void feedFrame(core::Profiler& prof) {
    prof.beginFrame();
    // Emit begins/ends by scanning timestamps: since kFrame is authored in pre-order with proper
    // nesting, we can drive begin() at each span's start and end() as spans close. Simplest correct
    // approach: recursively open a span, open its children, then close it.
    const int n = static_cast<int>(sizeof(kFrame) / sizeof(kFrame[0]));
    std::vector<int> openStack; // indices of open spans
    for (int i = 0; i < n; ++i) {
        // Close any open spans that end at or before this span's start.
        while (!openStack.empty() && kFrame[openStack.back()].end <= kFrame[i].start) {
            prof.end(kFrame[openStack.back()].end);
            openStack.pop_back();
        }
        prof.begin(kFrame[i].name, kFrame[i].start);
        openStack.push_back(i);
    }
    while (!openStack.empty()) {
        prof.end(kFrame[openStack.back()].end);
        openStack.pop_back();
    }
    prof.endFrame();
}

render::Color depthColor(int depth) {
    static const render::Color palette[3] = {
        {0.30f, 0.55f, 0.85f, 1.0f}, // depth 0
        {0.35f, 0.75f, 0.60f, 1.0f}, // depth 1
        {0.85f, 0.65f, 0.35f, 1.0f}, // depth 2+
    };
    return palette[depth < 3 ? depth : 2];
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PROFILER (CPU profiler view) starting");

    core::Profiler prof(0.25);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — CPU Profiler";
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

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            // Feed the same synthetic profile each fixed step (EMA converges; display stays fixed).
            feedFrame(prof);
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Frame total = the root zone's inclusive time (for bar scaling).
            const core::Profiler::Zone* root =
                prof.zones().empty() ? nullptr : &prof.zones().front();
            const double frameUs = root ? static_cast<double>(root->inclusiveUs) : 1.0;

            const float left = 40.0f;
            const float top = 120.0f;
            const float rowH = 46.0f;
            const float fullW = sw - left - 360.0f;

            int row = 0;
            for (const core::Profiler::Zone& z : prof.zones()) {
                const float y = top + static_cast<float>(row) * rowH;
                const float x0 = left + static_cast<float>(z.depth) * 22.0f;
                const float frac = static_cast<float>(static_cast<double>(z.inclusiveUs) / frameUs);
                const float barW = frac * fullW;

                // Zone bar.
                render::SpriteDesc d;
                d.x = x0;
                d.y = y;
                d.width = barW < 3.0f ? 3.0f : barW;
                d.height = rowH - 10.0f;
                d.color = depthColor(z.depth);
                renderer->drawSprite(white, d);

                // Label: name + inclusive/self ms + calls.
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s   %.2f ms  (self %.2f, x%llu)", z.name.c_str(),
                              static_cast<double>(z.inclusiveUs) / 1000.0,
                              static_cast<double>(z.selfUs) / 1000.0,
                              static_cast<unsigned long long>(z.calls));
                font.drawText(*renderer, x0 + 8.0f, y + 6.0f, buf, render::Color{1, 1, 1, 1}, 0.42f);
                ++row;
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CPU PROFILER (scoped zones)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char hud[128];
            std::snprintf(hud, sizeof(hud), "frame %.2f ms  -  %zu zones  (inclusive vs self time)",
                          frameUs / 1000.0, prof.zones().size());
            font.drawText(*renderer, 16.0f, 46.0f, hud, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PROFILER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
