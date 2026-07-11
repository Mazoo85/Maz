#include "maz/ui/DebugOverlay.hpp"

#include <cstdio>

namespace maz::ui {

void DebugOverlay::update(double frameDeltaSeconds) {
    if (frameDeltaSeconds > 0.0) {
        // Exponential smoothing so the readout doesn't jitter frame to frame.
        m_smoothedDt = m_smoothedDt * 0.9 + frameDeltaSeconds * 0.1;
    }
}

void DebugOverlay::draw(render::Renderer& renderer, Font& font, float x, float y,
                        float scale) const {
    if (!m_enabled) {
        return;
    }
    const double ms = m_smoothedDt * 1000.0;
    const double fps = m_smoothedDt > 0.0 ? 1.0 / m_smoothedDt : 0.0;
    const render::RenderStats s = renderer.renderStats();
    const render::Color col{0.6f, 1.0f, 0.7f, 1.0f};

    char buf[96];
    std::snprintf(buf, sizeof(buf), "FPS %4.0f    FRAME %5.2f ms", fps, ms);
    font.drawText(renderer, x, y, buf, col, scale);
    std::snprintf(buf, sizeof(buf), "MESH %u (culled %u)   PART %u   SPR %u", s.meshDraws, s.culled,
                  s.particles, s.sprites);
    font.drawText(renderer, x, y + 22.0f, buf, col, scale);
}

} // namespace maz::ui
