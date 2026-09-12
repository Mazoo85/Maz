// tests/core/movingaverage.cpp — verifies fixed-window rolling stats (core MovingAverage.hpp).
// Ground truths, deterministic (checked against a brute-force std::deque of the last N values):
//   * before the window fills, average/min/max use only the samples seen so far;
//   * once full, the oldest sample is forgotten and the mean tracks the recent window;
//   * count/full/capacity report the window state; clear() resets;
//   * a randomized (LCG) stress test agrees with brute force on average, min, and max every push,
//     across several window sizes.
#include "maz/core/MovingAverage.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <limits>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

struct Lcg {
    std::uint64_t s;
    int next(int lo, int hi) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return lo + static_cast<int>((s >> 33) % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

int main() {
    using maz::core::MovingAverage;

    // --- 1. Filling and basic averages. ---
    {
        MovingAverage<int> ma(3);
        CHECK(ma.capacity() == 3 && ma.count() == 0 && !ma.full(), "fresh window state");
        CHECK(near(ma.average(), 0.0), "empty average is 0");
        ma.push(10);
        CHECK(near(ma.average(), 10.0) && ma.min() == 10 && ma.max() == 10, "one sample");
        ma.push(20);
        CHECK(near(ma.average(), 15.0) && ma.count() == 2, "two samples average");
        ma.push(30);
        CHECK(near(ma.average(), 20.0) && ma.full(), "window full at three");
        CHECK(ma.min() == 10 && ma.max() == 30, "min/max over full window");
        // Fourth push evicts the 10.
        ma.push(60);
        CHECK(near(ma.average(), (20 + 30 + 60) / 3.0), "oldest forgotten after overflow");
        CHECK(ma.min() == 20 && ma.max() == 60, "min/max track the moved window");
    }

    // --- 2. Min/max eviction when the extreme leaves the window. ---
    {
        MovingAverage<int> ma(3);
        ma.push(5);
        ma.push(1); // current min
        ma.push(9); // current max
        CHECK(ma.min() == 1 && ma.max() == 9, "extremes inside the window");
        ma.push(7); // evicts the 5; window {1,9,7}
        CHECK(ma.min() == 1 && ma.max() == 9, "extremes still in window");
        ma.push(4); // evicts the 1; window {9,7,4} -> min becomes 4
        CHECK(ma.min() == 4 && ma.max() == 9, "min recomputed after its sample left");
        ma.push(3); // evicts the 9; window {7,4,3} -> max becomes 7
        CHECK(ma.max() == 7 && ma.min() == 3, "max recomputed after its sample left");
    }

    // --- 3. clear(). ---
    {
        MovingAverage<int> ma(4);
        for (int i = 0; i < 10; ++i) ma.push(i);
        ma.clear();
        CHECK(ma.count() == 0 && near(ma.average(), 0.0), "clear resets");
        ma.push(99);
        CHECK(near(ma.average(), 99.0) && ma.min() == 99, "works after clear");
    }

    // --- 4. Randomized stress vs brute force, several window sizes. ---
    {
        Lcg rng{0xA11CE5u};
        bool ok = true;
        for (std::size_t w : {std::size_t{1}, std::size_t{2}, std::size_t{5}, std::size_t{16}}) {
            MovingAverage<int> ma(w);
            std::deque<int> ref;
            for (int i = 0; i < 5000; ++i) {
                const int v = rng.next(-100, 100);
                ma.push(v);
                ref.push_back(v);
                if (ref.size() > w) ref.pop_front();
                // Brute-force average/min/max over the reference window.
                long s = 0;
                int mn = std::numeric_limits<int>::max();
                int mx = std::numeric_limits<int>::min();
                for (int x : ref) {
                    s += x;
                    mn = x < mn ? x : mn;
                    mx = x > mx ? x : mx;
                }
                const double refAvg = static_cast<double>(s) / static_cast<double>(ref.size());
                if (!near(ma.average(), refAvg)) ok = false;
                if (ma.min() != mn || ma.max() != mx) ok = false;
                if (ma.count() != ref.size()) ok = false;
            }
        }
        CHECK(ok, "randomized average/min/max/count agree with brute force across window sizes");
    }

    if (g_fail == 0) {
        std::printf("movingaverage: OK — fill, overflow, min/max eviction, clear, random stress.\n");
        return 0;
    }
    std::printf("movingaverage: %d failure(s).\n", g_fail);
    return 1;
}
