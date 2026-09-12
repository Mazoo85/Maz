// tests/platform/touchtarget.cpp — verifies platform::TouchTarget policy: converting a comfortable physical
// touch-target size (mm) to pixels across DPIs, the meets/expand accessibility helpers, and deriving DPI from
// a screen diagonal. Pure deterministic math; the live DPI comes from the display at runtime.
#include "maz/platform/TouchTarget.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

int main() {
    // --- 1. mmToPixels: 160 dpi is the dp baseline (1 mm ~= 6.299 px); DPI scales it linearly. ---
    {
        CHECK(approx(mmToPixels(kMmPerInch, 160.0f), 160.0f), "1 inch (25.4mm) at 160 dpi is 160 px");
        CHECK(approx(mmToPixels(1.0f, 160.0f), 160.0f / 25.4f), "1 mm at 160 dpi");
        CHECK(approx(mmToPixels(9.0f, 320.0f), 2.0f * mmToPixels(9.0f, 160.0f)),
              "double the dpi -> double the pixels");
        CHECK(approx(mmToPixels(9.0f, 0.0f), mmToPixels(9.0f, 160.0f)), "non-positive dpi falls back to 160");
    }

    // --- 2. recommendedTouchTargetPx: the 9 mm floor at a few real device DPIs. ---
    {
        CHECK(approx(recommendedTouchTargetPx(160.0f), 9.0f * 160.0f / 25.4f), "9mm at 160 dpi (~56.7 px)");
        // A ~440-dpi flagship needs a much larger pixel target for the same physical size.
        CHECK(recommendedTouchTargetPx(440.0f) > recommendedTouchTargetPx(160.0f),
              "higher dpi demands more pixels for the same physical target");
    }

    // --- 3. meetsTouchTarget: both edges must clear the minimum. ---
    {
        const float minPx = recommendedTouchTargetPx(400.0f); // ~141.7 px
        CHECK(meetsTouchTarget(minPx + 1.0f, minPx + 1.0f, 400.0f), "a big enough square passes");
        CHECK(!meetsTouchTarget(minPx - 1.0f, minPx + 1.0f, 400.0f), "too narrow fails");
        CHECK(!meetsTouchTarget(minPx + 1.0f, minPx - 1.0f, 400.0f), "too short fails");
        CHECK(meetsTouchTarget(40.0f, 40.0f, 160.0f) == (40.0f >= recommendedTouchTargetPx(160.0f)),
              "meets matches the raw comparison");
    }

    // --- 4. expandToTouchTarget: grows a too-small rect about its centre; leaves a big one alone. ---
    {
        const float dpi = 400.0f;
        const float minPx = recommendedTouchTargetPx(dpi);
        // A tiny 10x10 icon centred at (100,100): center is (105,105).
        maz::math::Rect2 icon{maz::math::vec2{100.0f, 100.0f}, maz::math::vec2{10.0f, 10.0f}};
        maz::math::Rect2 grown = expandToTouchTarget(icon, dpi);
        CHECK(approx(grown.size.x, minPx) && approx(grown.size.y, minPx), "small rect grows to the minimum");
        const float cx = grown.position.x + grown.size.x * 0.5f;
        const float cy = grown.position.y + grown.size.y * 0.5f;
        CHECK(approx(cx, 105.0f) && approx(cy, 105.0f), "expansion keeps the original centre");

        // An already-large rect is untouched.
        maz::math::Rect2 big{maz::math::vec2{0.0f, 0.0f}, maz::math::vec2{minPx + 50.0f, minPx + 50.0f}};
        maz::math::Rect2 same = expandToTouchTarget(big, dpi);
        CHECK(approx(same.size.x, big.size.x) && approx(same.size.y, big.size.y), "large rect is left as-is");

        // Per-axis: a wide-but-short rect only grows in height.
        maz::math::Rect2 wide{maz::math::vec2{0.0f, 0.0f}, maz::math::vec2{minPx + 20.0f, 4.0f}};
        maz::math::Rect2 tall = expandToTouchTarget(wide, dpi);
        CHECK(approx(tall.size.x, minPx + 20.0f) && approx(tall.size.y, minPx),
              "only the deficient axis grows");
    }

    // --- 5. dpiFromDiagonal: a 1080x2340 phone on a 6.1" diagonal is ~420 dpi; degenerate -> 160. ---
    {
        const float dpi = dpiFromDiagonal(1080, 2340, 6.1f);
        CHECK(dpi > 400.0f && dpi < 440.0f, "1080x2340 @ 6.1in is ~420 dpi");
        CHECK(approx(dpiFromDiagonal(0, 100, 5.0f), 160.0f), "degenerate width -> 160 baseline");
        CHECK(approx(dpiFromDiagonal(100, 100, 0.0f), 160.0f), "degenerate diagonal -> 160 baseline");
    }

    if (g_fail == 0) {
        std::printf("touchtarget: OK — mm->px, recommended size, meets/expand, dpi-from-diagonal.\n");
        return 0;
    }
    std::printf("touchtarget: %d failure(s).\n", g_fail);
    return 1;
}
