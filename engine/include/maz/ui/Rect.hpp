#pragma once

namespace maz::ui {

// Screen-space rectangle (pixel coordinates, origin top-left). Shared by the immediate-mode UI
// (hit-testing) and the retained layout system (computed node rects).
struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    float centerX() const { return x + w * 0.5f; }
    float centerY() const { return y + h * 0.5f; }
};

} // namespace maz::ui
