// tests/platform/safearea.cpp — verifies platform::SafeArea, the pure geometry that keeps UI clear of a
// phone's notch / rounded corners / status bar / home indicator. All deterministic CPU math (the live inset
// values come from the mobile backend at runtime): inset sanitizing, the usable safe rectangle, clamping a
// point / fitting a rect inside it, orientation rotation, and the backend seam's zero default on desktop.
#include "maz/platform/SafeArea.hpp"

#include "maz/platform/DesktopBackend.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;
using maz::math::Rect2;
using maz::math::vec2;

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main() {
    // --- 1. Sanitizing: negatives/NaN -> 0, and colliding pairs scale to fit the extent. ---
    {
        SafeAreaInsets in{-5.0f, std::nanf(""), 10.0f, 20.0f};
        SafeAreaInsets s = sanitizeInsets(in, 1080, 1920);
        CHECK(approx(s.left, 0.0f), "negative left clamps to 0");
        CHECK(approx(s.top, 0.0f), "NaN top clamps to 0");
        CHECK(approx(s.right, 10.0f) && approx(s.bottom, 20.0f), "valid insets pass through");

        // left+right exceed width -> scaled proportionally so they exactly fill it.
        SafeAreaInsets big{800.0f, 0.0f, 800.0f, 0.0f};
        SafeAreaInsets bs = sanitizeInsets(big, 1000, 2000);
        CHECK(approx(bs.left, 500.0f) && approx(bs.right, 500.0f),
              "colliding left+right scale to fill the width, not invert");
    }

    // --- 2. Safe rectangle: insets carve inward; origin shifts by (left,top), size shrinks by both edges. ---
    {
        // A portrait phone with a top status/notch (60) and a bottom home bar (34), tiny side rounding (5).
        SafeAreaInsets in{5.0f, 60.0f, 5.0f, 34.0f};
        Rect2 r = safeAreaRect(1080, 1920, in);
        CHECK(approx(r.left(), 5.0f) && approx(r.top(), 60.0f), "safe rect origin is (left,top) inset");
        CHECK(approx(r.size.x, 1070.0f), "safe width = display - left - right");
        CHECK(approx(r.size.y, 1826.0f), "safe height = display - top - bottom");

        // Zero insets (desktop) -> the whole drawable is safe.
        Rect2 full = safeAreaRect(1280, 720, SafeAreaInsets{});
        CHECK(approx(full.left(), 0.0f) && approx(full.top(), 0.0f) && approx(full.size.x, 1280.0f) &&
                  approx(full.size.y, 720.0f),
              "zero insets leave the full drawable safe");
    }

    // --- 3. clampPointToSafeArea keeps an anchor inside; a point already inside is untouched. ---
    {
        Rect2 safe(5.0f, 60.0f, 1070.0f, 1826.0f);
        vec2 under = clampPointToSafeArea(vec2(540.0f, 10.0f), safe); // under the notch
        CHECK(approx(under.y, 60.0f) && approx(under.x, 540.0f), "point under the top inset snaps to safe top");
        vec2 corner = clampPointToSafeArea(vec2(2000.0f, 2000.0f), safe);
        CHECK(approx(corner.x, safe.right()) && approx(corner.y, safe.bottom()),
              "off-screen corner clamps to the safe max corner");
        vec2 inside = clampPointToSafeArea(vec2(500.0f, 500.0f), safe);
        CHECK(approx(inside.x, 500.0f) && approx(inside.y, 500.0f), "an interior point is unchanged");
    }

    // --- 4. fitRectInSafeArea: shift a HUD panel off the home bar; shrink one bigger than the safe area. ---
    {
        Rect2 safe(0.0f, 60.0f, 1080.0f, 1826.0f); // top inset only
        // A HUD strip that would sit under the top notch -> pushed down to the safe top, size preserved.
        Rect2 fitted = fitRectInSafeArea(Rect2(0.0f, 0.0f, 400.0f, 80.0f), safe);
        CHECK(approx(fitted.top(), 60.0f) && approx(fitted.size.y, 80.0f),
              "a panel overlapping the top inset shifts down, size preserved");
        // A rect wider/taller than the safe area shrinks to fit and lands at the safe origin.
        Rect2 huge = fitRectInSafeArea(Rect2(0.0f, 0.0f, 5000.0f, 5000.0f), safe);
        CHECK(approx(huge.size.x, 1080.0f) && approx(huge.size.y, 1826.0f), "oversized rect shrinks to safe size");
        CHECK(approx(huge.left(), 0.0f) && approx(huge.top(), 60.0f), "shrunk rect sits at the safe origin");
    }

    // --- 5. rotateInsets remaps edges under quarter turns; full turn is identity; mod-4 wraps. ---
    {
        SafeAreaInsets portrait{1.0f, 2.0f, 3.0f, 4.0f}; // l,t,r,b
        // 90° CW: top<-left, right<-top, bottom<-right, left<-bottom.
        SafeAreaInsets cw = rotateInsets(portrait, 1);
        CHECK(approx(cw.top, 1.0f) && approx(cw.right, 2.0f) && approx(cw.bottom, 3.0f) && approx(cw.left, 4.0f),
              "90° CW rotates portrait insets to landscape");
        SafeAreaInsets full = rotateInsets(portrait, 4);
        CHECK(approx(full.left, 1.0f) && approx(full.top, 2.0f) && approx(full.right, 3.0f) &&
                  approx(full.bottom, 4.0f),
              "four quarter-turns is the identity");
        // -1 (mod 4 -> 3) equals three CW turns == one CCW turn.
        SafeAreaInsets ccw = rotateInsets(portrait, -1);
        SafeAreaInsets three = rotateInsets(portrait, 3);
        CHECK(approx(ccw.left, three.left) && approx(ccw.top, three.top) && approx(ccw.right, three.right) &&
                  approx(ccw.bottom, three.bottom),
              "negative turns wrap mod 4");
    }

    // --- 6. Backend seam: safeAreaInsets() defaults to zero on desktop (no notch), matching the honest report. ---
    {
        DesktopBackend db("MazEngine", "SafeAreaTest");
        SafeAreaInsets in = db.safeAreaInsets();
        CHECK(approx(in.left, 0.0f) && approx(in.top, 0.0f) && approx(in.right, 0.0f) && approx(in.bottom, 0.0f),
              "desktop reports zero safe-area insets");
        // The whole drawable is therefore safe on desktop.
        Rect2 r = safeAreaRect(1280, 720, in);
        CHECK(approx(r.size.x, 1280.0f) && approx(r.size.y, 720.0f), "desktop safe rect is the full drawable");
    }

    if (g_fail == 0) {
        std::printf("safe_area: OK — inset sanitize, safe rect, clamp/fit, orientation rotate, backend seam.\n");
        return 0;
    }
    std::printf("safe_area: %d failure(s).\n", g_fail);
    return 1;
}
