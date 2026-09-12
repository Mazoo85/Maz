// tests/platform/thumbzone.cpp — verifies platform::ThumbZone control placement: anchoring controls in the
// bottom-left / bottom-right thumb zones inside a safe rect, the right/left-handed twin layout, size clamping
// on a small screen, and the thumb-reach helpers. Pure deterministic geometry (drawable pixels, +y down).
#include "maz/platform/ThumbZone.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

int main() {
    // A 1000x600 safe rect at origin (top-left 0,0; bottom edge y=600).
    const maz::math::Rect2 safe{maz::math::vec2{0.0f, 0.0f}, maz::math::vec2{1000.0f, 600.0f}};

    // --- 1. bottomLeftZone / bottomRightZone: corner-anchored with margin. ---
    {
        const maz::math::Rect2 bl = bottomLeftZone(safe, 120.0f, 120.0f, 20.0f);
        CHECK(approx(bl.left(), 20.0f), "bottom-left hugs the left edge + margin");
        CHECK(approx(bl.bottom(), 580.0f), "bottom-left bottom = safe bottom - margin");
        CHECK(approx(bl.size.x, 120.0f) && approx(bl.size.y, 120.0f), "bottom-left keeps its size");

        const maz::math::Rect2 br = bottomRightZone(safe, 160.0f, 160.0f, 20.0f);
        CHECK(approx(br.right(), 980.0f), "bottom-right hugs the right edge - margin");
        CHECK(approx(br.left(), 980.0f - 160.0f), "bottom-right left = right - width");
        CHECK(approx(br.bottom(), 580.0f), "bottom-right bottom = safe bottom - margin");
    }

    // --- 2. Right-handed twin layout: stick left, actions right. ---
    {
        const TouchControlLayout lo = thumbControlLayout(safe, 140.0f, 180.0f, 24.0f);
        CHECK(approx(lo.moveStick.left(), 24.0f), "RH: move stick is on the left");
        CHECK(approx(lo.actionCluster.right(), 1000.0f - 24.0f), "RH: action cluster is on the right");
        CHECK(approx(lo.moveStick.size.x, 140.0f), "RH: stick uses stickPx");
        CHECK(approx(lo.actionCluster.size.x, 180.0f), "RH: cluster uses clusterPx");
        // Both sit along the bottom.
        CHECK(approx(lo.moveStick.bottom(), 576.0f) && approx(lo.actionCluster.bottom(), 576.0f),
              "RH: both controls sit on the bottom");
    }

    // --- 3. Left-handed swaps which thumb gets which control. ---
    {
        const TouchControlLayout lo = thumbControlLayout(safe, 140.0f, 180.0f, 24.0f, Handedness::LeftHanded);
        CHECK(approx(lo.actionCluster.left(), 24.0f), "LH: action cluster moves to the left");
        CHECK(approx(lo.moveStick.right(), 1000.0f - 24.0f), "LH: move stick moves to the right");
        CHECK(approx(lo.actionCluster.size.x, 180.0f), "LH: cluster still uses clusterPx");
        CHECK(approx(lo.moveStick.size.x, 140.0f), "LH: stick still uses stickPx");
    }

    // --- 4. Size clamping: an oversized control never spills outside a small safe rect. ---
    {
        const maz::math::Rect2 tiny{maz::math::vec2{0.0f, 0.0f}, maz::math::vec2{100.0f, 100.0f}};
        const maz::math::Rect2 z = bottomLeftZone(tiny, 500.0f, 500.0f, 10.0f);
        CHECK(z.size.x <= tiny.size.x && z.size.y <= tiny.size.y, "oversized control is clamped to fit");
        CHECK(z.left() >= tiny.left() - 1e-3f && z.bottom() <= tiny.bottom() + 1e-3f,
              "clamped control stays inside the safe rect");
    }

    // --- 5. Thumb reach: radius scales with the shorter edge; the corner is reachable, far corner is not. ---
    {
        const float r = thumbReachRadius(safe); // 0.62 * min(1000,600) = 372
        CHECK(approx(r, 0.62f * 600.0f), "reach radius is a fraction of the shorter edge");
        const maz::math::vec2 blCorner{safe.left(), safe.bottom()};
        CHECK(withinThumbReach(maz::math::vec2{100.0f, 500.0f}, blCorner, r), "a nearby point is reachable");
        CHECK(!withinThumbReach(maz::math::vec2{safe.right(), safe.top()}, blCorner, r),
              "the opposite (top-right) corner is out of reach");
    }

    if (g_fail == 0) {
        std::printf("thumbzone: OK — corner zones, RH/LH twin layout, clamping, thumb reach.\n");
        return 0;
    }
    std::printf("thumbzone: %d failure(s).\n", g_fail);
    return 1;
}
