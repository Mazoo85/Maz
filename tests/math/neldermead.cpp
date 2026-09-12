// tests/math/neldermead.cpp — verifies the Nelder-Mead downhill simplex minimiser (math NelderMead.hpp).
// Two independent kinds of ground truth per problem:
//   * KNOWN ANALYTIC MINIMA: standard optimisation test functions whose exact minimisers are known —
//       sphere (min 0 at the origin), Rosenbrock (min 0 at (1,1), the classic banana valley),
//       Booth (min 0 at (1,3)), Beale (min 0 at (3,0.5)) — the solver must land on them;
//   * LOCAL-OPTIMALITY (oracle that does NOT trust the known answer): sample many points in a small ball
//       around the returned minimiser with a seeded LCG and assert NONE has a lower value — the returned
//       point really is a local minimum, proven independently of the analytic optimum;
//   * MONOTONE: the returned value is never worse than f(x0);
//   * DEGENERATE: an empty start returns cleanly.
// Deterministic: fixed starts + a seeded LCG, no <random>, no clock.
#include "maz/math/NelderMead.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::nelderMead;
using maz::math::NelderMeadOptions;
using maz::math::NelderMeadResult;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 32); }
    double sym() { return static_cast<double>(next()) / 4294967296.0 * 2.0 - 1.0; } // [-1,1)
};

static double sphere(const std::vector<double>& x) {
    double s = 0.0;
    for (double v : x) s += v * v;
    return s;
}
static double rosenbrock(const std::vector<double>& x) {
    const double a = 1.0 - x[0], b = x[1] - x[0] * x[0];
    return a * a + 100.0 * b * b;
}
static double booth(const std::vector<double>& x) {
    const double a = x[0] + 2.0 * x[1] - 7.0, b = 2.0 * x[0] + x[1] - 5.0;
    return a * a + b * b;
}
static double beale(const std::vector<double>& x) {
    const double u = x[0], v = x[1];
    const double a = 1.5 - u + u * v;
    const double b = 2.25 - u + u * v * v;
    const double c = 2.625 - u + u * v * v * v;
    return a * a + b * b + c * c;
}

// Independent local-optimality oracle: no nearby point is lower than the returned one.
template <typename F>
static bool isLocalMin(F&& f, const std::vector<double>& x, double radius, Lcg& rng) {
    const double fx = f(x);
    for (int t = 0; t < 4000; ++t) {
        std::vector<double> p = x;
        for (double& c : p) c += rng.sym() * radius;
        if (f(p) < fx - 1e-9) {
            return false;
        }
    }
    return true;
}

int main() {
    Lcg rng{0xC0FFEEu};

    // --- 1. Sphere in 5-D from a far start -> origin. ---
    {
        const std::vector<double> x0 = {3.0, -4.0, 5.0, -2.0, 1.5};
        const NelderMeadResult r = nelderMead(sphere, x0);
        CHECK(r.fx < 1e-8, "sphere: found value ~0");
        double worstCoord = 0.0;
        for (double c : r.x) worstCoord = std::fabs(c) > worstCoord ? std::fabs(c) : worstCoord;
        CHECK(worstCoord < 1e-4, "sphere: minimiser is the origin");
        CHECK(r.fx <= sphere(x0), "sphere: never worse than the start");
        CHECK(isLocalMin(sphere, r.x, 1e-3, rng), "sphere: result is a genuine local minimum");
    }

    // --- 2. Rosenbrock from the classic hard start (-1.2, 1) -> (1, 1). ---
    {
        const std::vector<double> x0 = {-1.2, 1.0};
        NelderMeadOptions opt;
        opt.restarts = 4; // the banana valley rewards a couple of restarts
        const NelderMeadResult r = nelderMead(rosenbrock, x0, opt);
        CHECK(r.fx < 1e-6, "rosenbrock: found value ~0");
        CHECK(std::fabs(r.x[0] - 1.0) < 1e-3 && std::fabs(r.x[1] - 1.0) < 1e-3, "rosenbrock: minimiser is (1,1)");
        CHECK(r.fx <= rosenbrock(x0), "rosenbrock: never worse than the start");
        CHECK(isLocalMin(rosenbrock, r.x, 1e-3, rng), "rosenbrock: result is a genuine local minimum");
    }

    // --- 3. Booth -> (1, 3). ---
    {
        const std::vector<double> x0 = {0.0, 0.0};
        const NelderMeadResult r = nelderMead(booth, x0);
        CHECK(r.fx < 1e-8, "booth: found value ~0");
        CHECK(std::fabs(r.x[0] - 1.0) < 1e-4 && std::fabs(r.x[1] - 3.0) < 1e-4, "booth: minimiser is (1,3)");
        CHECK(isLocalMin(booth, r.x, 1e-3, rng), "booth: result is a genuine local minimum");
    }

    // --- 4. Beale -> (3, 0.5). ---
    {
        const std::vector<double> x0 = {1.0, 1.0};
        NelderMeadOptions opt;
        opt.restarts = 3;
        const NelderMeadResult r = nelderMead(beale, x0, opt);
        CHECK(r.fx < 1e-6, "beale: found value ~0");
        CHECK(std::fabs(r.x[0] - 3.0) < 1e-3 && std::fabs(r.x[1] - 0.5) < 1e-3, "beale: minimiser is (3,0.5)");
        CHECK(isLocalMin(beale, r.x, 1e-4, rng), "beale: result is a genuine local minimum");
    }

    // --- 5. Convergence flag + degenerate input. ---
    {
        const NelderMeadResult r = nelderMead(sphere, std::vector<double>{2.0, 2.0});
        CHECK(r.converged, "a smooth well converges to tolerance (not the iteration cap)");
        CHECK(r.iterations > 0, "some iterations were performed");
        const NelderMeadResult empty = nelderMead(sphere, std::vector<double>{});
        CHECK(empty.x.empty() && !empty.converged, "empty start returns cleanly");
    }

    if (g_fail == 0) {
        std::printf("neldermead: OK — sphere, rosenbrock, booth, beale (known minima + independent local-optimality).\n");
        return 0;
    }
    std::printf("neldermead: %d failure(s).\n", g_fail);
    return 1;
}
