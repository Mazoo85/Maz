#pragma once

#include <algorithm>
#include <cmath>

namespace maz::ui {

// Range — Godot's Range, the shared value model behind ProgressBar, HSlider/VSlider, ScrollBar, and
// SpinBox. It holds a scalar `value` clamped to [min, max], optionally snapped to a `step`, and exposes
// it as a normalized `ratio` in [0,1] — the single number a bar or slider draws from. `page` supports
// scrollbar-style ranges where a visible window of size `page` means the value can only reach `max-page`
// (so ratio still spans 0..1). `allowGreater`/`allowLesser` lift the clamp when a field may legitimately
// exceed its nominal bounds. Pure logic, header-only, deterministic — it unit-tests exactly.
class Range {
public:
    double minValue = 0.0;
    double maxValue = 100.0;
    double step = 0.0;  // 0 => continuous (no snapping)
    double page = 0.0;  // visible-window size (scrollbars); effective max = maxValue - page
    bool allowGreater = false;
    bool allowLesser = false;

    void setValue(double v) { m_value = coerce(v); }
    double value() const { return m_value; }

    // Normalized position in [0,1] across [min, max-page]. A degenerate span reports 0.
    double ratio() const {
        const double span = maxValue - minValue - page;
        if (span <= 0.0) {
            return 0.0;
        }
        return (m_value - minValue) / span;
    }

    // Set the value from a [0,1] ratio across [min, max-page].
    void setRatio(double r) {
        const double span = maxValue - minValue - page;
        setValue(minValue + r * span);
    }

    // Nudge by a number of steps (or by 1/10th of the span when continuous). Godot's Range step-scroll.
    void step_(double count) {
        const double delta = step > 0.0 ? step : (maxValue - minValue - page) * 0.1;
        setValue(m_value + delta * count);
    }

private:
    double m_value = 0.0;

    double coerce(double v) const {
        // Snap to the step grid (anchored at min) first, then clamp.
        if (step > 0.0) {
            v = minValue + std::round((v - minValue) / step) * step;
        }
        const double lo = minValue;
        const double hi = maxValue - page;
        if (!allowLesser && v < lo) {
            v = lo;
        }
        if (!allowGreater && v > hi) {
            v = hi;
        }
        return v;
    }
};

// ProgressBar — Godot's ProgressBar: a Range shown as a fill from 0 to its ratio, with an optional
// percentage readout. The reusable logic (fill fraction, percent) lives here; the app draws the bar.
struct ProgressBar {
    Range range;
    bool showPercentage = true;

    ProgressBar() { range.maxValue = 100.0; }

    void setValue(double v) { range.setValue(v); }
    double value() const { return range.value(); }
    float fillFraction() const { return static_cast<float>(range.ratio()); } // 0..1 for the fill width
    int percent() const { return static_cast<int>(std::lround(range.ratio() * 100.0)); }
};

} // namespace maz::ui
