// tests/platform/orientation.cpp — verifies platform::Orientation: classifying a ScreenOrientation as
// portrait/landscape, mapping it to quarter-turns-from-portrait, re-deriving safe-area insets for the
// current orientation (the SafeArea tie-in), the name strings, and PlatformBackend::orientation()'s
// Unknown default on desktop. Pure deterministic logic; the live orientation comes from the mobile backend.
#include "maz/platform/Orientation.hpp"

#include "maz/platform/DesktopBackend.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
    // --- 1. Portrait / landscape classification (Unknown is neither). ---
    {
        CHECK(isPortrait(ScreenOrientation::Portrait), "Portrait is portrait");
        CHECK(isPortrait(ScreenOrientation::PortraitFlipped), "PortraitFlipped is portrait");
        CHECK(isLandscape(ScreenOrientation::LandscapeLeft), "LandscapeLeft is landscape");
        CHECK(isLandscape(ScreenOrientation::LandscapeRight), "LandscapeRight is landscape");
        CHECK(!isPortrait(ScreenOrientation::LandscapeLeft) && !isLandscape(ScreenOrientation::Portrait),
              "the two categories are disjoint");
        CHECK(!isPortrait(ScreenOrientation::Unknown) && !isLandscape(ScreenOrientation::Unknown),
              "Unknown is neither portrait nor landscape");
    }

    // --- 2. Quarter-turns-from-portrait mapping (feeds SafeArea::rotateInsets). ---
    {
        CHECK(quarterTurnsFromPortrait(ScreenOrientation::Portrait) == 0, "Portrait = 0 turns");
        CHECK(quarterTurnsFromPortrait(ScreenOrientation::LandscapeLeft) == 1, "LandscapeLeft = 1 turn CW");
        CHECK(quarterTurnsFromPortrait(ScreenOrientation::PortraitFlipped) == 2, "PortraitFlipped = 2 turns");
        CHECK(quarterTurnsFromPortrait(ScreenOrientation::LandscapeRight) == 3, "LandscapeRight = 3 turns CW");
        CHECK(quarterTurnsFromPortrait(ScreenOrientation::Unknown) == 0, "Unknown = 0 turns (no rotation)");
    }

    // --- 3. orientedInsets re-derives a portrait notch's insets for the current orientation. ---
    {
        // A phone with a top notch (top=60) authored in portrait.
        SafeAreaInsets portrait{0.0f, 60.0f, 0.0f, 34.0f}; // l,t,r,b (top notch + bottom home bar)

        // Portrait: unchanged.
        SafeAreaInsets p = orientedInsets(portrait, ScreenOrientation::Portrait);
        CHECK(approx(p.top, 60.0f) && approx(p.bottom, 34.0f), "portrait insets are unchanged");

        // LandscapeLeft (90° CW): the top notch moves to the RIGHT edge; bottom bar to the LEFT.
        SafeAreaInsets ll = orientedInsets(portrait, ScreenOrientation::LandscapeLeft);
        CHECK(approx(ll.right, 60.0f) && approx(ll.left, 34.0f),
              "a 90° CW turn sends the top notch to the right edge");
        CHECK(approx(ll.top, 0.0f) && approx(ll.bottom, 0.0f), "top/bottom clear in landscape-left");

        // PortraitFlipped (180°): top<->bottom swap.
        SafeAreaInsets pf = orientedInsets(portrait, ScreenOrientation::PortraitFlipped);
        CHECK(approx(pf.bottom, 60.0f) && approx(pf.top, 34.0f), "180° swaps top and bottom");

        // Unknown: no rotation.
        SafeAreaInsets u = orientedInsets(portrait, ScreenOrientation::Unknown);
        CHECK(approx(u.top, 60.0f), "Unknown leaves insets untouched");
    }

    // --- 4. Name strings. ---
    {
        CHECK(std::strcmp(orientationName(ScreenOrientation::Portrait), "portrait") == 0, "portrait name");
        CHECK(std::strcmp(orientationName(ScreenOrientation::LandscapeLeft), "landscape-left") == 0,
              "landscape-left name");
        CHECK(std::strcmp(orientationName(ScreenOrientation::Unknown), "unknown") == 0, "unknown name");
    }

    // --- 5. Backend seam: DesktopBackend reports Unknown (no device orientation). ---
    {
        DesktopBackend db("MazEngine", "OrientationTest");
        CHECK(db.orientation() == ScreenOrientation::Unknown, "desktop orientation is Unknown");
    }

    if (g_fail == 0) {
        std::printf("orientation: OK — classify, quarter-turns, oriented insets, names, backend seam.\n");
        return 0;
    }
    std::printf("orientation: %d failure(s).\n", g_fail);
    return 1;
}
