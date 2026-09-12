#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// maz::render dynamic-resolution scaling (DRS) — the single biggest frame-rate lever on a mobile GPU.
// When the scene gets heavy the GPU misses the frame budget and the game stutters; instead of dropping
// whole effects, DRS renders the 3D scene to a SMALLER offscreen target and upscales it to the display,
// which cuts fragment/bandwidth cost roughly with the square of the scale while the UI stays crisp at
// native resolution. This is exactly what console/mobile engines (and Godot's `scaling_3d` /
// `rendering/scaling_3d/mode`) do to hold 60 fps on constrained hardware.
//
// This class is the pure, deterministic controller: feed it each frame's measured time and it returns a
// linear render-scale factor in [minScale, maxScale]. It smooths the input with an EMA (so a single spike
// doesn't drop resolution), only adjusts after a cooldown (so it doesn't oscillate frame-to-frame), and
// separates the scale-up and scale-down thresholds into a deadband (hysteresis) around the target. The
// actual offscreen-target resize is the renderer's job on device; the policy — when and how far to
// scale — is here, header-only and unit-tested, with no GPU dependency.
namespace maz::render {

struct DynamicResolutionConfig {
    float targetFrameMs = 1000.0f / 60.0f; // frame budget (60 fps). <=0 is treated as this default.
    float minScale = 0.5f;                 // resolution floor (0.5 linear == 25% of the pixels)
    float maxScale = 1.0f;                 // never render above native
    float step = 0.05f;                    // scale delta applied per adjustment
    float upThreshold = 0.85f;             // avg frame < this * target -> headroom, scale up
    float downThreshold = 1.0f;            // avg frame > this * target -> over budget, scale down
    float smoothing = 0.25f;               // EMA weight of the newest sample in [0,1] (1 == no smoothing)
    int settleFrames = 8;                  // frames to wait between adjustments (anti-oscillation)
};

// The pixel size a scale maps to, floored to at least 1x1 so the swapchain/target is always valid.
struct RenderSize {
    int width = 0;
    int height = 0;
};

class DynamicResolution {
public:
    explicit DynamicResolution(DynamicResolutionConfig cfg = {}) : m_cfg(sanitize(cfg)) {
        m_scale = m_cfg.maxScale;
    }

    // Feed one frame's measured time (CPU+GPU, milliseconds) and get the current render scale back. The
    // scale changes at most once per `settleFrames` frames, by `step`, and stays within [min,max].
    float update(float frameMs) {
        if (!(std::isfinite(frameMs)) || frameMs < 0.0f) frameMs = 0.0f;
        // Exponential moving average of frame time; seed on the first sample so we don't ramp from zero.
        if (!m_seeded) {
            m_avgMs = frameMs;
            m_seeded = true;
        } else {
            m_avgMs = m_avgMs + m_cfg.smoothing * (frameMs - m_avgMs);
        }

        if (m_cooldown > 0) {
            --m_cooldown;
            return m_scale;
        }

        const float target = m_cfg.targetFrameMs;
        if (m_avgMs > m_cfg.downThreshold * target && m_scale > m_cfg.minScale) {
            m_scale = std::max(m_cfg.minScale, m_scale - m_cfg.step);
            m_cooldown = m_cfg.settleFrames;
        } else if (m_avgMs < m_cfg.upThreshold * target && m_scale < m_cfg.maxScale) {
            m_scale = std::min(m_cfg.maxScale, m_scale + m_cfg.step);
            m_cooldown = m_cfg.settleFrames;
        }
        return m_scale;
    }

    float scale() const { return m_scale; }
    float averageFrameMs() const { return m_avgMs; }

    // Map the current scale onto a native pixel size (each dimension rounded, floored to >=1).
    RenderSize renderSize(int nativeW, int nativeH) const {
        auto apply = [this](int n) {
            const int v = static_cast<int>(std::lround(static_cast<float>(n) * m_scale));
            return v < 1 ? 1 : v;
        };
        return {apply(nativeW), apply(nativeH)};
    }

    // Force a scale (e.g. from a user quality setting) and clear the averaging/cooldown state.
    void setScale(float s) {
        m_scale = std::min(m_cfg.maxScale, std::max(m_cfg.minScale, s));
        m_cooldown = m_cfg.settleFrames;
    }

    // Back to native resolution and a clean averaging window (call on scene/quality change).
    void reset() {
        m_scale = m_cfg.maxScale;
        m_avgMs = 0.0f;
        m_seeded = false;
        m_cooldown = 0;
    }

    const DynamicResolutionConfig& config() const { return m_cfg; }

private:
    // Keep the config self-consistent: positive target/step, a valid [min,max] window with min in (0,max],
    // smoothing in (0,1], non-negative cooldown. Bad inputs would otherwise deadlock or oscillate.
    static DynamicResolutionConfig sanitize(DynamicResolutionConfig c) {
        if (!(c.targetFrameMs > 0.0f) || !std::isfinite(c.targetFrameMs)) c.targetFrameMs = 1000.0f / 60.0f;
        c.maxScale = std::min(1.0f, std::max(0.05f, c.maxScale));
        c.minScale = std::min(c.maxScale, std::max(0.05f, c.minScale));
        c.step = std::min(c.maxScale, std::max(0.001f, c.step));
        if (!(c.smoothing > 0.0f) || c.smoothing > 1.0f) c.smoothing = 0.25f;
        if (c.settleFrames < 0) c.settleFrames = 0;
        // Keep the up threshold strictly below the down threshold so there is a real deadband.
        c.downThreshold = std::max(0.1f, c.downThreshold);
        c.upThreshold = std::min(c.upThreshold, c.downThreshold - 0.01f);
        if (c.upThreshold <= 0.0f) c.upThreshold = c.downThreshold * 0.85f;
        return c;
    }

    DynamicResolutionConfig m_cfg;
    float m_scale = 1.0f;
    float m_avgMs = 0.0f;
    bool m_seeded = false;
    int m_cooldown = 0;
};

} // namespace maz::render
