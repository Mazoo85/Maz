// tests/render/dynamicresolution.cpp — verifies render::DynamicResolution, the adaptive render-scale (DRS)
// controller that trades resolution for frame rate on constrained (mobile) GPUs. Pure deterministic policy
// (no GPU): starts at native, scales DOWN when the smoothed frame time blows the budget and UP when there's
// headroom, holds inside the deadband, respects the [min,max] clamp + per-adjustment cooldown, and maps a
// scale onto a floored pixel size. Config sanitizing (bad thresholds/steps) is checked too.
#include "maz/render/DynamicResolution.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// Drive N frames at a constant frame time; returns the final scale.
static float run(DynamicResolution& drs, float frameMs, int frames) {
    float s = drs.scale();
    for (int i = 0; i < frames; ++i) s = drs.update(frameMs);
    return s;
}

int main() {
    const float target = 1000.0f / 60.0f; // 16.67 ms

    // --- 1. Starts at native resolution (maxScale). ---
    {
        DynamicResolution drs;
        CHECK(approx(drs.scale(), 1.0f), "starts at native scale 1.0");
    }

    // --- 2. Over budget -> scales down, one step per cooldown, never below the floor. ---
    {
        DynamicResolutionConfig cfg;
        cfg.step = 0.1f;
        cfg.minScale = 0.5f;
        cfg.settleFrames = 4;
        DynamicResolution drs(cfg);
        // A slow frame (2x budget) held long enough should ratchet the scale down to the floor.
        float s = run(drs, target * 2.0f, 200);
        CHECK(approx(s, 0.5f), "sustained over-budget frames scale down to the floor");
        CHECK(drs.scale() >= 0.5f - 1e-4f, "never scales below minScale");
    }

    // --- 3. One adjustment per cooldown window (not every frame). ---
    {
        DynamicResolutionConfig cfg;
        cfg.step = 0.1f;
        cfg.settleFrames = 5;
        cfg.smoothing = 1.0f; // no EMA lag: react to the instantaneous value
        DynamicResolution drs(cfg);
        float first = drs.update(target * 2.0f); // frame 0: over budget -> one step down, cooldown starts
        CHECK(approx(first, 0.9f), "first over-budget frame drops exactly one step");
        // The next few frames are in cooldown: scale must not move again yet.
        drs.update(target * 2.0f);
        drs.update(target * 2.0f);
        CHECK(approx(drs.scale(), 0.9f), "scale holds during the cooldown window");
    }

    // --- 4. Headroom -> scales back up toward native, capped at maxScale. ---
    {
        DynamicResolutionConfig cfg;
        cfg.step = 0.1f;
        cfg.minScale = 0.4f;
        cfg.settleFrames = 3;
        DynamicResolution drs(cfg);
        drs.setScale(0.4f);                    // pretend we're scaled down from a past spike
        CHECK(approx(drs.scale(), 0.4f), "setScale clamps + applies");
        float s = run(drs, target * 0.3f, 200); // very fast frames -> lots of headroom
        CHECK(approx(s, 1.0f), "sustained headroom scales back up to native");
    }

    // --- 5. Inside the deadband -> stable (no oscillation). A frame right at ~92% of budget sits between
    //        the up threshold (85%) and the down threshold (100%), so the scale must not change. ---
    {
        DynamicResolution drs; // default up=0.85, down=1.0
        float s = run(drs, target * 0.92f, 100);
        CHECK(approx(s, 1.0f), "frames inside the deadband hold at native");
        // From a scaled-down start, a deadband frame should also hold (not creep up or down).
        drs.setScale(0.7f);
        float s2 = run(drs, target * 0.92f, 100);
        CHECK(approx(s2, 0.7f), "deadband holds a scaled-down value steady");
    }

    // --- 6. renderSize maps the scale onto floored pixel dims (>=1). ---
    {
        DynamicResolution drs;
        drs.setScale(0.5f);
        RenderSize rs = drs.renderSize(1920, 1080);
        CHECK(rs.width == 960 && rs.height == 540, "0.5 scale halves the pixel dims");
        drs.setScale(0.5f);
        RenderSize tiny = drs.renderSize(1, 1);
        CHECK(tiny.width == 1 && tiny.height == 1, "pixel dims floor at 1x1, never zero");
    }

    // --- 7. Config sanitizing: nonsense inputs are made consistent instead of deadlocking. ---
    {
        DynamicResolutionConfig bad;
        bad.targetFrameMs = -5.0f;   // invalid -> default 60fps budget
        bad.maxScale = 2.0f;         // clamped to 1.0
        bad.minScale = 0.9f;         // <= max, ok
        bad.upThreshold = 1.5f;      // must end up below downThreshold
        bad.downThreshold = 1.0f;
        DynamicResolution drs(bad);
        const DynamicResolutionConfig& c = drs.config();
        CHECK(approx(c.targetFrameMs, target), "invalid target falls back to the 60fps budget");
        CHECK(approx(c.maxScale, 1.0f), "maxScale clamps to native");
        CHECK(c.upThreshold < c.downThreshold, "up threshold forced below down threshold (real deadband)");
    }

    // --- 8. reset() returns to native + a clean averaging window. ---
    {
        DynamicResolution drs;
        run(drs, target * 2.0f, 50); // scale down
        CHECK(drs.scale() < 1.0f, "scaled down before reset");
        drs.reset();
        CHECK(approx(drs.scale(), 1.0f), "reset restores native scale");
    }

    if (g_fail == 0) {
        std::printf("dynamic_resolution: OK — start/down/up/deadband, cooldown, clamp, pixel map, sanitize.\n");
        return 0;
    }
    std::printf("dynamic_resolution: %d failure(s).\n", g_fail);
    return 1;
}
