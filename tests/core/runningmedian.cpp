// tests/core/runningmedian.cpp — verifies the streaming median filter (core RunningMedian.hpp).
// Ground truths, deterministic (checked against a brute-force sorted window):
//   * odd and even window medians match a brute-force computation before and after the window fills;
//   * a lone spike is rejected (the median filter's output at the spike is a real neighbour value, unlike a
//     moving average which would be dragged toward it);
//   * a genuine step change is preserved (edges survive);
//   * a randomized stress test agrees with brute force at every push;
//   * clear() / full() behave.
#include "maz/core/RunningMedian.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

// Brute-force median of a window (a deque of the last N values).
static double bruteMedian(std::deque<double> w) {
    std::vector<double> v(w.begin(), w.end());
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n == 0) return 0.0;
    if (n % 2 == 1) return v[n / 2];
    return (v[n / 2 - 1] + v[n / 2]) * 0.5;
}

struct Lcg {
    std::uint64_t s;
    int next(int lo, int hi) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return lo + static_cast<int>((s >> 33) % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

int main() {
    using maz::core::RunningMedian;

    // --- 1. Odd window, hand-checked. ---
    {
        RunningMedian rm(3);
        rm.push(10);
        CHECK(near(rm.median(), 10.0), "one sample -> itself");
        rm.push(30);
        CHECK(near(rm.median(), 20.0), "two samples -> average");
        rm.push(20); // window {10,30,20} -> sorted 10,20,30 -> 20
        CHECK(near(rm.median(), 20.0) && rm.full(), "median of {10,30,20} is 20");
        rm.push(100); // evict 10 -> {30,20,100} -> median 30
        CHECK(near(rm.median(), 30.0), "after evicting the oldest, median is 30");
    }

    // --- 2. Spike rejection vs a mean. ---
    {
        RunningMedian rm(5);
        const double vals[] = {5, 5, 1000, 5, 5}; // one giant spike in the middle
        double sum = 0.0;
        for (double v : vals) { rm.push(v); sum += v; }
        CHECK(near(rm.median(), 5.0), "median ignores the 1000 spike (stays at 5)");
        CHECK(sum / 5.0 > 200.0, "a mean would be dragged to >200 by the same spike");
    }

    // --- 3. Step edge is preserved. ---
    {
        RunningMedian rm(3);
        for (int i = 0; i < 3; ++i) rm.push(0.0);
        CHECK(near(rm.median(), 0.0), "flat low level");
        rm.push(10); rm.push(10); rm.push(10); // fully crossed to the high level
        CHECK(near(rm.median(), 10.0), "after the step, the median reaches the new level (edge preserved)");
    }

    // --- 4. Randomized cross-check, several window sizes. ---
    {
        Lcg rng{0xFEEDu};
        bool ok = true;
        for (std::size_t w : {std::size_t{1}, std::size_t{2}, std::size_t{4}, std::size_t{7},
                              std::size_t{16}}) {
            RunningMedian rm(w);
            std::deque<double> ref;
            for (int i = 0; i < 4000; ++i) {
                const double v = static_cast<double>(rng.next(-1000, 1000));
                rm.push(v);
                ref.push_back(v);
                if (ref.size() > w) ref.pop_front();
                if (!near(rm.median(), bruteMedian(ref))) ok = false;
                if (rm.count() != ref.size()) ok = false;
            }
        }
        CHECK(ok, "randomized medians agree with brute force across window sizes");
    }

    // --- 5. clear(). ---
    {
        RunningMedian rm(5);
        for (int i = 0; i < 10; ++i) rm.push(static_cast<double>(i));
        rm.clear();
        CHECK(rm.count() == 0 && near(rm.median(), 0.0), "clear resets");
        rm.push(42.0);
        CHECK(near(rm.median(), 42.0), "works after clear");
    }

    if (g_fail == 0) {
        std::printf("runningmedian: OK — hand cases, spike rejection, edge, random, clear.\n");
        return 0;
    }
    std::printf("runningmedian: %d failure(s).\n", g_fail);
    return 1;
}
