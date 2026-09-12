#pragma once

#include <vector>

// maz::platform multi-monitor helpers — the pure geometry behind "which display is this window on?"
// and "put this window on that monitor." A multi-monitor desktop lays every display out in one
// virtual coordinate space (monitor 2 might start at x=1920); an OS decides a window's "current"
// monitor by which display its rectangle overlaps most. Games need this to open on the right screen,
// go fullscreen on the display the window is on, and center dialogs. Godot exposes it via
// DisplayServer.get_screen_* ; here the decision logic is a pure, unit-tested function set (the SDL
// side — enumerating live displays — lives in platform::Window).
namespace maz::platform {

// One display in the virtual desktop: its index, pixel bounds (x,y = top-left in the shared space,
// w,h = size), and HiDPI content scale.
struct DisplayInfo {
    int index = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float scale = 1.0f;
};

// Index of the display whose bounds contain the point, or -1 if none does.
inline int displayContainingPoint(const std::vector<DisplayInfo>& displays, int px, int py) {
    for (const DisplayInfo& d : displays) {
        if (px >= d.x && px < d.x + d.w && py >= d.y && py < d.y + d.h) {
            return d.index;
        }
    }
    return -1;
}

// Area of overlap between a window rect and a display (0 if disjoint).
inline long long overlapArea(const DisplayInfo& d, int x, int y, int w, int h) {
    const int ix0 = x > d.x ? x : d.x;
    const int iy0 = y > d.y ? y : d.y;
    const int ix1 = (x + w) < (d.x + d.w) ? (x + w) : (d.x + d.w);
    const int iy1 = (y + h) < (d.y + d.h) ? (y + h) : (d.y + d.h);
    const int ow = ix1 - ix0;
    const int oh = iy1 - iy0;
    if (ow <= 0 || oh <= 0) {
        return 0;
    }
    return static_cast<long long>(ow) * static_cast<long long>(oh);
}

// The display a window rect "belongs to": the one it overlaps most. Ties and no-overlap fall back
// to the first display (index 0-th entry). Returns -1 only when there are no displays.
inline int displayForRect(const std::vector<DisplayInfo>& displays, int x, int y, int w, int h) {
    if (displays.empty()) {
        return -1;
    }
    int best = displays.front().index;
    long long bestArea = -1;
    for (const DisplayInfo& d : displays) {
        const long long a = overlapArea(d, x, y, w, h);
        if (a > bestArea) {
            bestArea = a;
            best = d.index;
        }
    }
    return best;
}

// Top-left position that centers a w×h window on the given display.
struct Point2i {
    int x = 0;
    int y = 0;
};
inline Point2i centerRectOnDisplay(const DisplayInfo& d, int w, int h) {
    return {d.x + (d.w - w) / 2, d.y + (d.h - h) / 2};
}

} // namespace maz::platform
