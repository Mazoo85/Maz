#pragma once

#include <cmath>

// maz::game day/night cycle — a looping time-of-day clock for open-world lighting, shop hours, and spawn
// schedules. One in-game day spans `dayLength` real seconds; `update(dt)` advances the clock, wraps it at
// midnight, and ticks a day counter. It reports the normalized time-of-day [0,1), the in-game hour [0,24),
// a DayPhase (Night / Dawn / Day / Dusk) from configurable thresholds, and a sun elevation in [-1,1]
// (-1 at midnight, 0 at sunrise/sunset, +1 at noon) the renderer can feed straight into a directional
// light. Godot ships no day/night system — games hand-roll it every time — so this is a beyond-Godot
// gameplay utility. Header-only, std-only, deterministic.
namespace maz::game {

enum class DayPhase { Night, Dawn, Day, Dusk };

class DayNightCycle {
public:
    // `dayLength`: real seconds per in-game day (clamped to > 0). `startNormalized`: initial time-of-day
    // fraction (wrapped into [0,1)).
    explicit DayNightCycle(double dayLength = 120.0, double startNormalized = 0.0)
        : m_dayLength(dayLength > 0.0 ? dayLength : 1.0) {
        m_clock = wrap01(startNormalized) * m_dayLength;
    }

    // Phase cut-offs as normalized fractions: Dawn starts at `dawn`, Day at `day`, Dusk at `dusk`, Night
    // at `night`. Night wraps around midnight (t >= night or t < dawn).
    void setPhaseThresholds(double dawn, double day, double dusk, double night) {
        m_dawnStart = dawn;
        m_dayStart = day;
        m_duskStart = dusk;
        m_nightStart = night;
    }

    // Advance the clock by `dt` seconds (non-positive dt ignored), wrapping at day's end and counting days.
    void update(double dt) {
        if (dt <= 0.0) return;
        double total = m_clock + dt;
        if (total >= m_dayLength) {
            const long long days = static_cast<long long>(total / m_dayLength);
            m_day += days;
            total -= static_cast<double>(days) * m_dayLength;
        }
        m_clock = total;
    }

    double dayLength() const { return m_dayLength; }
    long long day() const { return m_day; }

    // Time-of-day as a fraction [0,1): 0 = midnight, 0.5 = noon.
    double normalized() const { return m_clock / m_dayLength; }

    // In-game hour [0,24).
    double hour() const { return normalized() * 24.0; }

    // Sun elevation [-1,1]: -1 at midnight, 0 at sunrise (06:00) and sunset (18:00), +1 at noon.
    double sunElevation() const { return -std::cos(normalized() * 6.28318530717958647692); }

    bool isDaytime() const { return sunElevation() > 0.0; }

    DayPhase phase() const {
        const double t = normalized();
        if (t >= m_nightStart || t < m_dawnStart) return DayPhase::Night;
        if (t < m_dayStart) return DayPhase::Dawn;
        if (t < m_duskStart) return DayPhase::Day;
        return DayPhase::Dusk;
    }

    // Jump to a time-of-day fraction (wrapped) without changing the day counter.
    void setNormalized(double t) { m_clock = wrap01(t) * m_dayLength; }
    // Jump to an in-game hour (wrapped into [0,24)).
    void setHour(double h) { setNormalized(h / 24.0); }
    void setDay(long long d) { m_day = d; }

private:
    static double wrap01(double t) {
        double f = t - std::floor(t);
        if (f < 0.0) f += 1.0;
        if (f >= 1.0) f = 0.0;
        return f;
    }

    double m_dayLength;
    double m_clock = 0.0; // seconds into the current day, [0, dayLength)
    long long m_day = 0;
    double m_dawnStart = 0.2;
    double m_dayStart = 0.3;
    double m_duskStart = 0.7;
    double m_nightStart = 0.8;
};

} // namespace maz::game
