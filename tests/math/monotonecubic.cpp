// tests/math/monotonecubic.cpp — verifies PCHIP monotone cubic interpolation (math MonotoneCubic.hpp).
// Ground truths, deterministic:
//   * the curve passes through every control point exactly;
//   * on monotonically increasing data the interpolant is non-decreasing everywhere (the defining
//     no-overshoot property) and never leaves each segment's [y_i, y_{i+1}] bracket;
//   * the classic step data {0,0,0,1,1,1} stays within [0,1] everywhere (a natural spline would overshoot);
//   * a flat segment stays exactly flat;
//   * eval clamps to the endpoints outside the x-range; single-point and empty behave.
#include "maz/math/MonotoneCubic.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b, double e = 1e-9) { return std::fabs(a - b) < e; }

int main() {
    using maz::math::MonotoneCubic;

    // --- 1. Passes through control points. ---
    {
        const std::vector<double> xs{0, 1, 2, 3, 4};
        const std::vector<double> ys{0, 2, 1, 4, 3}; // non-monotone
        MonotoneCubic mc(xs, ys);
        CHECK(mc.valid() && mc.size() == 5, "built");
        bool ok = true;
        for (std::size_t i = 0; i < xs.size(); ++i)
            if (!near(mc.eval(xs[i]), ys[i], 1e-9)) ok = false;
        CHECK(ok, "interpolant hits every control point exactly");
    }

    // --- 2. Monotone data -> non-decreasing, no overshoot. ---
    {
        const std::vector<double> xs{0, 1, 2, 3, 4, 5};
        const std::vector<double> ys{0, 1, 1.5, 5, 8, 8.2}; // strictly non-decreasing
        MonotoneCubic mc(xs, ys);
        bool mono = true;
        double prev = mc.eval(0.0);
        for (int k = 1; k <= 2000; ++k) {
            const double x = 5.0 * static_cast<double>(k) / 2000.0;
            const double v = mc.eval(x);
            if (v < prev - 1e-9) mono = false;
            prev = v;
        }
        CHECK(mono, "monotone increasing data yields a non-decreasing interpolant (no dips)");

        // No overshoot: every sampled value lies within the bracket of its enclosing segment.
        bool bracket = true;
        for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
            const double lo = ys[i] < ys[i + 1] ? ys[i] : ys[i + 1];
            const double hi = ys[i] > ys[i + 1] ? ys[i] : ys[i + 1];
            for (int k = 0; k <= 50; ++k) {
                const double x = xs[i] + (xs[i + 1] - xs[i]) * static_cast<double>(k) / 50.0;
                const double v = mc.eval(x);
                if (v < lo - 1e-6 || v > hi + 1e-6) bracket = false;
            }
        }
        CHECK(bracket, "no segment overshoots its endpoints' value range");
    }

    // --- 3. Step data stays within [0,1] (a natural spline would ring). ---
    {
        const std::vector<double> xs{0, 1, 2, 3, 4, 5};
        const std::vector<double> ys{0, 0, 0, 1, 1, 1};
        MonotoneCubic mc(xs, ys);
        bool ok = true;
        for (int k = 0; k <= 5000; ++k) {
            const double x = 5.0 * static_cast<double>(k) / 5000.0;
            const double v = mc.eval(x);
            if (v < -1e-6 || v > 1.0 + 1e-6) ok = false;
        }
        CHECK(ok, "step data never overshoots outside [0,1]");
    }

    // --- 4. Flat segment stays flat. ---
    {
        const std::vector<double> xs{0, 1, 2, 3};
        const std::vector<double> ys{5, 5, 5, 5};
        MonotoneCubic mc(xs, ys);
        bool flat = true;
        for (int k = 0; k <= 300; ++k) {
            const double x = 3.0 * static_cast<double>(k) / 300.0;
            if (!near(mc.eval(x), 5.0, 1e-9)) flat = false;
        }
        CHECK(flat, "a constant data set interpolates to a constant");
    }

    // --- 5. Endpoint clamping + degenerate sizes. ---
    {
        const std::vector<double> xs{1, 2, 3};
        const std::vector<double> ys{10, 20, 30};
        MonotoneCubic mc(xs, ys);
        CHECK(near(mc.eval(-100.0), 10.0), "below range clamps to first y");
        CHECK(near(mc.eval(100.0), 30.0), "above range clamps to last y");

        MonotoneCubic one({std::vector<double>{7.0}}, {std::vector<double>{42.0}});
        CHECK(one.valid() && near(one.eval(999.0), 42.0), "single point is a constant");

        MonotoneCubic empty;
        CHECK(!empty.valid() && near(empty.eval(0.0), 0.0), "empty is invalid and evaluates to 0");

        MonotoneCubic mismatch({std::vector<double>{0.0, 1.0}}, {std::vector<double>{0.0}});
        CHECK(!mismatch.valid(), "length mismatch is rejected");
    }

    if (g_fail == 0) {
        std::printf("monotonecubic: OK — interpolation, no-overshoot, step, flat, clamp, edges.\n");
        return 0;
    }
    std::printf("monotonecubic: %d failure(s).\n", g_fail);
    return 1;
}
