// tests/core/simulatedannealing.cpp — verifies the generic optimizer (core SimulatedAnnealing.hpp).
// Ground truths, deterministic (seeded; no <random>, no clock):
//   * a continuous convex bowl f(x)=(x-2)^2+1 is minimised near x=2 with energy near 1;
//   * a multimodal 1D landscape is solved to its known global basin (SA escapes local minima);
//   * a travelling-salesman tour over points on a circle is driven from a shuffled start down to near the
//     optimal (in-order) tour length, and always no worse than the start;
//   * the best energy never exceeds the starting energy.
#include "maz/core/SimulatedAnnealing.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    using maz::core::simulatedAnnealing;

    // --- 1. Continuous convex bowl. ---
    {
        auto energy = [](double x) { return (x - 2.0) * (x - 2.0) + 1.0; };
        auto neighbour = [](double x, auto rand01) { return x + (rand01() * 2.0 - 1.0) * 0.5; };
        const auto r = simulatedAnnealing<double>(-8.0, energy, neighbour, 20000, 10.0, 1e-4, 0xABCu);
        CHECK(std::fabs(r.state - 2.0) < 0.1, "convex bowl minimised near x=2");
        CHECK(std::fabs(r.energy - 1.0) < 0.01, "convex bowl energy near 1");
    }

    // --- 2. Multimodal landscape (SA must escape local minima). ---
    {
        // f(x) = (x-3)^2 / 10 + sin(3x): wavy, but the (x-3)^2 bowl makes the global basin near x≈3.
        // Brute-force the true global minimum on a fine grid for the reference.
        auto f = [](double x) { return (x - 3.0) * (x - 3.0) * 0.1 + std::sin(3.0 * x); };
        double gx = -10.0, gmin = f(-10.0);
        for (int i = 0; i <= 200000; ++i) {
            const double x = -10.0 + 20.0 * static_cast<double>(i) / 200000.0;
            const double v = f(x);
            if (v < gmin) { gmin = v; gx = x; }
        }
        auto neighbour = [](double x, auto rand01) {
            double nx = x + (rand01() * 2.0 - 1.0) * 1.0;
            if (nx < -10.0) nx = -10.0;
            if (nx > 10.0) nx = 10.0;
            return nx;
        };
        const auto r = simulatedAnnealing<double>(9.5, f, neighbour, 40000, 5.0, 1e-4, 0x5EEDu);
        CHECK(r.energy < gmin + 0.05, "multimodal solved to within 0.05 of the true global minimum");
        CHECK(std::fabs(r.state - gx) < 0.3, "landed in the global basin");
    }

    // --- 3. Travelling salesman over points on a circle. ---
    {
        const int N = 14;
        std::vector<double> px(static_cast<std::size_t>(N)), py(static_cast<std::size_t>(N));
        const double pi = 3.14159265358979323846;
        for (int i = 0; i < N; ++i) {
            const double a = 2.0 * pi * static_cast<double>(i) / static_cast<double>(N);
            px[static_cast<std::size_t>(i)] = std::cos(a);
            py[static_cast<std::size_t>(i)] = std::sin(a);
        }
        const double optimal = static_cast<double>(N) * 2.0 * std::sin(pi / static_cast<double>(N));

        auto tourLen = [&](const std::vector<int>& t) {
            double s = 0.0;
            for (std::size_t i = 0; i < t.size(); ++i) {
                const int a = t[i];
                const int b = t[(i + 1) % t.size()];
                const double dx = px[static_cast<std::size_t>(a)] - px[static_cast<std::size_t>(b)];
                const double dy = py[static_cast<std::size_t>(a)] - py[static_cast<std::size_t>(b)];
                s += std::sqrt(dx * dx + dy * dy);
            }
            return s;
        };
        auto neighbour = [](std::vector<int> t, auto rand01) {
            const std::size_t n = t.size();
            std::size_t i = static_cast<std::size_t>(rand01() * static_cast<double>(n)) % n;
            std::size_t j = static_cast<std::size_t>(rand01() * static_cast<double>(n)) % n;
            std::swap(t[i], t[j]);
            return t;
        };

        // A deliberately bad (interleaved) starting tour.
        std::vector<int> start(static_cast<std::size_t>(N));
        for (int i = 0; i < N; ++i) start[static_cast<std::size_t>(i)] = (i * 5) % N;
        const double startLen = tourLen(start);

        const auto r = simulatedAnnealing<std::vector<int>>(start, tourLen, neighbour, 60000, 2.0, 1e-4,
                                                            0xC0FFEEu);
        CHECK(r.energy <= startLen, "SA never returns a worse tour than the start");
        CHECK(r.energy < optimal * 1.15, "SA reaches within 15% of the optimal circular tour");
        CHECK(r.energy < startLen * 0.9, "SA meaningfully improves on the bad starting tour");
        // The result must be a valid permutation.
        std::vector<char> seen(static_cast<std::size_t>(N), 0);
        bool perm = r.state.size() == static_cast<std::size_t>(N);
        for (int c : r.state) {
            if (c < 0 || c >= N || seen[static_cast<std::size_t>(c)]) perm = false;
            else seen[static_cast<std::size_t>(c)] = 1;
        }
        CHECK(perm, "the result tour is a valid permutation of all cities");
    }

    if (g_fail == 0) {
        std::printf("simulatedannealing: OK — convex bowl, multimodal escape, TSP near-optimal.\n");
        return 0;
    }
    std::printf("simulatedannealing: %d failure(s).\n", g_fail);
    return 1;
}
