// tests/core/hungarian.cpp — verifies the Hungarian (Kuhn-Munkres) optimal assignment (core Hungarian.hpp).
// Ground truths, deterministic:
//   * hand-worked 2x2 and 3x3 cost matrices give the known optimal assignment and cost;
//   * a valid assignment is always a permutation of distinct tasks;
//   * a rectangular problem (fewer agents than tasks) picks the cheapest subset of tasks;
//   * negating the matrix turns a min-cost solve into the max-value assignment;
//   * randomized cross-check: for n up to 8, the Hungarian cost equals the true minimum over ALL
//     permutations (brute force), across many random matrices.
#include "maz/core/Hungarian.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

// Brute-force minimum assignment cost over all permutations (n <= 8).
static double bruteMin(const std::vector<double>& c, int n) {
    std::vector<int> perm(static_cast<std::size_t>(n));
    std::iota(perm.begin(), perm.end(), 0);
    double best = 1e300;
    do {
        double s = 0.0;
        for (int i = 0; i < n; ++i)
            s += c[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) +
                    static_cast<std::size_t>(perm[static_cast<std::size_t>(i)])];
        if (s < best) best = s;
    } while (std::next_permutation(perm.begin(), perm.end()));
    return best;
}

struct Lcg {
    std::uint64_t s;
    double f() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(s >> 33) / static_cast<double>(1ull << 31);
    }
};

// Assert the assignment is a valid permutation of [0,cols) restricted to distinct tasks.
static bool distinctTasks(const std::vector<int>& a, int cols) {
    std::vector<char> used(static_cast<std::size_t>(cols), 0);
    for (int t : a) {
        if (t < 0 || t >= cols || used[static_cast<std::size_t>(t)]) return false;
        used[static_cast<std::size_t>(t)] = 1;
    }
    return true;
}

int main() {
    using maz::core::Assignment;
    using maz::core::hungarian;

    // --- 1. Hand-worked 2x2. ---
    {
        // cost = [[1,2],[2,1]] -> assign agent0->task0, agent1->task1, total 2.
        const std::vector<double> c{1, 2, 2, 1};
        const Assignment a = hungarian(c, 2, 2);
        CHECK(near(a.totalCost, 2.0), "2x2 optimal cost is 2");
        CHECK(a.taskForAgent[0] == 0 && a.taskForAgent[1] == 1, "2x2 diagonal assignment");
    }

    // --- 2. Hand-worked 3x3 where greedy would fail. ---
    {
        // A greedy nearest pick (agent0->task0 at 1) blocks a better global solution.
        // cost:
        //   [ 1, 9, 9 ]
        //   [ 9, 9, 1 ]
        //   [ 9, 1, 9 ]
        // optimum: 0->0, 1->2, 2->1 = 3.
        const std::vector<double> c{1, 9, 9, 9, 9, 1, 9, 1, 9};
        const Assignment a = hungarian(c, 3, 3);
        CHECK(near(a.totalCost, 3.0), "3x3 optimal cost is 3");
        CHECK(distinctTasks(a.taskForAgent, 3), "3x3 assignment is a permutation");
        CHECK(near(a.totalCost, bruteMin(c, 3)), "3x3 matches brute-force minimum");
    }

    // --- 3. Rectangular: 2 agents, 3 tasks — pick the two cheapest compatible. ---
    {
        // rows=2, cols=3:
        //   [ 4, 1, 3 ]
        //   [ 2, 0, 5 ]
        // best: agent0->task2 (3) + agent1->task1 (0) = 3, or agent0->task0(4)+agent1->task1(0)=4...
        // actually agent1->task0(2)+agent0->task1(1)=3 as well; optimum total = 3.
        const std::vector<double> c{4, 1, 3, 2, 0, 5};
        const Assignment a = hungarian(c, 2, 3);
        CHECK(a.taskForAgent.size() == 2, "rectangular returns one task per agent");
        CHECK(distinctTasks(a.taskForAgent, 3), "rectangular assignment uses distinct tasks");
        CHECK(near(a.totalCost, 3.0), "rectangular optimum is 3");
    }

    // --- 4. Maximisation via negation. ---
    {
        // value matrix; we want the MAX total value assignment.
        //   [ 5, 1 ]
        //   [ 1, 5 ]  -> best is the diagonal, value 10.
        const std::vector<double> val{5, 1, 1, 5};
        std::vector<double> neg(val.size());
        for (std::size_t i = 0; i < val.size(); ++i) neg[i] = -val[i];
        const Assignment a = hungarian(neg, 2, 2);
        CHECK(near(-a.totalCost, 10.0), "negated solve yields the max-value assignment (10)");
    }

    // --- 5. Randomized cross-check vs brute force for n = 1..8. ---
    {
        Lcg rng{0x0FF1CEu};
        bool ok = true;
        for (int n = 1; n <= 8; ++n) {
            for (int trial = 0; trial < 40; ++trial) {
                std::vector<double> c(static_cast<std::size_t>(n * n));
                for (double& x : c) x = std::floor(rng.f() * 100.0);
                const Assignment a = hungarian(c, static_cast<std::size_t>(n), static_cast<std::size_t>(n));
                if (!distinctTasks(a.taskForAgent, n)) ok = false;
                if (!near(a.totalCost, bruteMin(c, n))) ok = false;
            }
        }
        CHECK(ok, "Hungarian cost equals the brute-force minimum for n=1..8 over many random matrices");
    }

    // --- 6. Degenerate inputs. ---
    {
        CHECK(hungarian({}, 0, 0).taskForAgent.empty(), "empty problem returns empty");
        CHECK(hungarian({1, 2}, 2, 1).taskForAgent.empty(), "more agents than tasks returns empty");
    }

    if (g_fail == 0) {
        std::printf("hungarian: OK — 2x2/3x3 hand cases, rectangular, maximisation, brute-force n<=8.\n");
        return 0;
    }
    std::printf("hungarian: %d failure(s).\n", g_fail);
    return 1;
}
