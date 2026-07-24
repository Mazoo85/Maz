// tests/math/integrate.cpp — verifies numerical integration (math Integrate.hpp).
// Ground truths, deterministic (closed-form integrals, no <random>, no clock):
//   * GAUSS EXACTNESS (airtight): 5-point Gauss-Legendre is EXACT (to rounding) for polynomials up to degree
//     9 — integral of x^k over [a,b] equals (b^(k+1)-a^(k+1))/(k+1) for k = 0..9, single panel;
//   * KNOWN INTEGRALS: sin, e^x, 4/(1+x^2)=pi, x^3 match their closed forms;
//   * ADDITIVITY: integral over [a,c] equals [a,b] + [b,c];
//   * ADAPTIVE SIMPSON: converges to the closed form to its tolerance, and agrees with Gauss on a wiggly f;
//   * ARC LENGTH: integrating the speed of a circle of radius R over a quarter turn gives R*pi/2.
#include "maz/math/Integrate.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    using namespace maz::math;

    // --- 1. Gauss-Legendre 5-point exact for polynomials up to degree 9. ---
    {
        const double a = -1.3, b = 2.7;
        double worst = 0.0;
        for (int k = 0; k <= 9; ++k) {
            const double got = integrateGauss([k](double x) { return std::pow(x, static_cast<double>(k)); }, a, b, 1);
            const double want = (std::pow(b, k + 1) - std::pow(a, k + 1)) / static_cast<double>(k + 1);
            worst = std::max(worst, std::fabs(got - want));
        }
        CHECK(worst < 1e-9, "5-point Gauss is exact for polynomials up to degree 9");
    }

    // --- 2. Known integrals. ---
    {
        const double PI = 3.14159265358979324;
        CHECK(std::fabs(integrateGauss([](double x) { return std::sin(x); }, 0.0, PI, 8) - 2.0) < 1e-9,
              "integral of sin over [0,pi] is 2");
        CHECK(std::fabs(integrateGauss([](double x) { return std::exp(x); }, 0.0, 1.0, 8) - (std::exp(1.0) - 1.0)) < 1e-9,
              "integral of e^x over [0,1] is e-1");
        CHECK(std::fabs(integrateGauss([](double x) { return 4.0 / (1.0 + x * x); }, 0.0, 1.0, 16) - PI) < 1e-9,
              "integral of 4/(1+x^2) over [0,1] is pi");
        CHECK(std::fabs(integrateGauss([](double x) { return x * x * x; }, 0.0, 2.0, 1) - 4.0) < 1e-9,
              "integral of x^3 over [0,2] is 4");
    }

    // --- 3. Additivity. ---
    {
        auto f = [](double x) { return std::sin(x) * std::exp(-0.2 * x) + x * x; };
        const double whole = integrateGauss(f, 0.0, 3.0, 20);
        const double part = integrateGauss(f, 0.0, 1.2, 20) + integrateGauss(f, 1.2, 3.0, 20);
        CHECK(std::fabs(whole - part) < 1e-8, "integral over [a,c] equals [a,b] + [b,c]");
    }

    // --- 4. Adaptive Simpson: converges to closed form and agrees with Gauss on a wiggly f. ---
    {
        const double PI = 3.14159265358979324;
        CHECK(std::fabs(integrateAdaptiveSimpson([](double x) { return std::sin(x); }, 0.0, PI, 1e-10) - 2.0) < 1e-8,
              "adaptive Simpson: integral of sin over [0,pi] is 2");
        auto wig = [](double x) { return std::sin(8.0 * x) * std::cos(3.0 * x) + 0.5 * x; };
        const double as = integrateAdaptiveSimpson(wig, 0.0, 5.0, 1e-11);
        const double gl = integrateGauss(wig, 0.0, 5.0, 200);
        CHECK(std::fabs(as - gl) < 1e-7, "adaptive Simpson agrees with composite Gauss on a wiggly function");
    }

    // --- 5. Arc length of a quarter circle of radius R: integrate |derivative| = R over [0, pi/2] => R*pi/2. ---
    {
        const double R = 2.5;
        const double PI = 3.14159265358979324;
        // Circle (R cos t, R sin t); speed = R. Arc length integrand computed from the parametric derivative.
        auto speed = [R](double t) {
            const double dx = -R * std::sin(t), dy = R * std::cos(t);
            return std::sqrt(dx * dx + dy * dy);
        };
        const double len = integrateGauss(speed, 0.0, PI * 0.5, 16);
        CHECK(std::fabs(len - R * PI * 0.5) < 1e-9, "quarter-circle arc length is R*pi/2");
    }

    if (g_fail == 0) {
        std::printf("integrate: OK — Gauss exactness, known integrals, additivity, adaptive Simpson, arc length.\n");
        return 0;
    }
    std::printf("integrate: %d failure(s).\n", g_fail);
    return 1;
}
