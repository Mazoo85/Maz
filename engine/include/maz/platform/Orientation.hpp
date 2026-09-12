#pragma once

#include "maz/platform/PlatformBackend.hpp" // platform::ScreenOrientation (the enum the backend reports)
#include "maz/platform/SafeArea.hpp"        // SafeAreaInsets + rotateInsets

// maz::platform orientation helpers — the pure logic a game uses to react to a device rotating between
// portrait and landscape. PlatformBackend::orientation() reports WHICH way the screen is turned; these free
// functions classify it and, crucially, re-derive the safe-area insets for the current orientation from the
// insets you authored once in portrait. On a phone the notch/home-bar move to different edges as the device
// rotates, so a game that hard-codes portrait insets would put its HUD under the cutout in landscape; feed
// `orientedInsets(portraitInsets, backend->orientation())` into safeAreaRect instead and it follows the turn.
// Deterministic and header-only; on desktop the orientation is Unknown so everything is a no-op.
namespace maz::platform {

inline bool isPortrait(ScreenOrientation o) {
    return o == ScreenOrientation::Portrait || o == ScreenOrientation::PortraitFlipped;
}
inline bool isLandscape(ScreenOrientation o) {
    return o == ScreenOrientation::LandscapeLeft || o == ScreenOrientation::LandscapeRight;
}

// How many 90°-clockwise turns the screen is rotated from native portrait. This is exactly the argument
// SafeArea's rotateInsets takes, so the two compose directly. Unknown maps to 0 (no rotation).
inline int quarterTurnsFromPortrait(ScreenOrientation o) {
    switch (o) {
        case ScreenOrientation::Portrait: return 0;
        case ScreenOrientation::LandscapeLeft: return 1;
        case ScreenOrientation::PortraitFlipped: return 2;
        case ScreenOrientation::LandscapeRight: return 3;
        case ScreenOrientation::Unknown: break;
    }
    return 0;
}

inline const char* orientationName(ScreenOrientation o) {
    switch (o) {
        case ScreenOrientation::Portrait: return "portrait";
        case ScreenOrientation::LandscapeLeft: return "landscape-left";
        case ScreenOrientation::PortraitFlipped: return "portrait-flipped";
        case ScreenOrientation::LandscapeRight: return "landscape-right";
        case ScreenOrientation::Unknown: break;
    }
    return "unknown";
}

// Re-derive safe-area insets for the current orientation from the values authored in native portrait. A top
// notch in portrait becomes a right-edge inset after a 90°-CW turn (LandscapeLeft), etc. Unknown leaves the
// insets untouched. This is the whole point of the module: cache portrait insets once, call this per turn.
inline SafeAreaInsets orientedInsets(SafeAreaInsets portraitInsets, ScreenOrientation o) {
    return rotateInsets(portraitInsets, quarterTurnsFromPortrait(o));
}

} // namespace maz::platform
