#pragma once

#include "maz/render/Renderer.hpp"
#include "maz/ui/Font.hpp"

namespace maz::ui {

// A small profiling overlay: smoothed FPS + frame time and the renderer's per-frame draw counts,
// drawn with a Font. Off by default; toggle it (e.g. on F3). Engine-agnostic — the app owns the
// Font and decides where to place it.
class DebugOverlay {
public:
    void setEnabled(bool on) { m_enabled = on; }
    void toggle() { m_enabled = !m_enabled; }
    bool enabled() const { return m_enabled; }

    // Feed the real frame delta (seconds) each frame to keep the smoothed FPS current.
    void update(double frameDeltaSeconds);

    // Draw the overlay (no-op when disabled) at the given pixel position, in pixel-space.
    void draw(render::Renderer& renderer, Font& font, float x, float y, float scale = 0.42f) const;

private:
    bool m_enabled = false;
    double m_smoothedDt = 1.0 / 60.0;
};

} // namespace maz::ui
