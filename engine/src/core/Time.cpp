#include "maz/core/Time.hpp"

#include <SDL3/SDL_timer.h>

#include <algorithm>

namespace maz::core {

namespace {
// Never advance more than this much real time per frame, or a stall would generate a burst of
// fixed steps that stalls even harder (the classic "spiral of death").
constexpr double kMaxFrameDelta = 0.25;
} // namespace

Clock::Clock(double fixedStepSeconds) : m_fixedStep(fixedStepSeconds) {}

void Clock::beginFrame() {
    const uint64_t now = SDL_GetPerformanceCounter();
    const uint64_t freq = SDL_GetPerformanceFrequency();

    if (!m_started) {
        m_started = true;
        m_lastTicks = now;
        m_frameDelta = 0.0;
        return;
    }

    m_frameDelta = static_cast<double>(now - m_lastTicks) / static_cast<double>(freq);
    m_lastTicks = now;
    m_frameDelta = std::min(m_frameDelta, kMaxFrameDelta);

    m_elapsed += m_frameDelta;
    m_accumulator += m_frameDelta;
    ++m_frameCount;
}

bool Clock::consumeFixedStep() {
    if (m_accumulator + 1e-12 >= m_fixedStep) {
        m_accumulator -= m_fixedStep;
        return true;
    }
    return false;
}

double Clock::interpolationAlpha() const {
    return m_fixedStep > 0.0 ? m_accumulator / m_fixedStep : 0.0;
}

} // namespace maz::core
