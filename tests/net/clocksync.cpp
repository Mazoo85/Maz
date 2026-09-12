// tests/net/clocksync.cpp — verifies NTP-style clock offset / RTT estimation (net ClockSync.hpp).
// Ground truths, deterministic (simulated exchanges with a known server offset and controlled delays):
//   * with symmetric one-way delays the recovered offset equals the true offset exactly;
//   * the round-trip delay equals up+down; latency is half of it;
//   * with jittery delays the min-delay ("best") sample recovers the offset closely;
//   * asymmetric constant delays bias the offset by exactly (up-down)/2 (the known NTP limitation);
//   * toServerTime / toLocalTime round-trip; clear()/ready() behave.
#include "maz/net/ClockSync.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b, double e = 1e-9) { return std::fabs(a - b) < e; }

// Simulate one exchange: client sends at t0 (client clock); server clock = client clock + trueOffset;
// up/down are the one-way delays; p is server processing time. Fills the four NTP timestamps.
static void exchange(double t0, double trueOffset, double up, double down, double p, double& t1, double& t2,
                     double& t3) {
    const double serverRecvLocal = t0 + up;          // client-clock instant the server receives
    t1 = serverRecvLocal + trueOffset;               // stamped on server clock
    t2 = t1 + p;                                     // server replies p later (server clock)
    t3 = t0 + up + p + down;                         // client-clock instant of reply arrival
}

struct Lcg {
    std::uint64_t s;
    double u01() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(s >> 11) / static_cast<double>(1ull << 53);
    }
};

int main() {
    using maz::net::ClockSync;

    // --- 1. Symmetric delays: exact offset + RTT. ---
    {
        ClockSync cs;
        const double O = 1000.0;  // server is 1000 units ahead
        const double d = 25.0;    // one-way, symmetric
        double t1, t2, t3;
        exchange(500.0, O, d, d, 5.0, t1, t2, t3);
        cs.addSample(500.0, t1, t2, t3);
        CHECK(cs.ready(), "ready after one sample");
        CHECK(near(cs.offset(), O), "symmetric delays recover the exact offset");
        CHECK(near(cs.rtt(), 2.0 * d), "RTT equals up + down");
        CHECK(near(cs.latency(), d), "latency is half the RTT");
        CHECK(near(cs.toServerTime(500.0), 1500.0), "toServerTime adds the offset");
        CHECK(near(cs.toLocalTime(1500.0), 500.0), "toLocalTime subtracts the offset");
    }

    // --- 2. Jittery delays: best (min-delay) sample recovers the offset. ---
    {
        ClockSync cs(16);
        const double O = -350.0; // server behind
        Lcg rng{0x2468ACEu};
        double t = 0.0;
        for (int i = 0; i < 200; ++i) {
            const double base = 20.0;
            const double up = base + rng.u01() * 40.0;   // 20..60, jittered
            const double down = base + rng.u01() * 40.0; // independent jitter
            double t1, t2, t3;
            exchange(t, O, up, down, 2.0, t1, t2, t3);
            cs.addSample(t, t1, t2, t3);
            t += 100.0;
        }
        // The min-delay sample has the least |up-down| bias on average, so offset lands near O.
        CHECK(std::fabs(cs.offset() - O) < 20.0, "min-delay sample recovers the offset under jitter");
        CHECK(std::fabs(cs.smoothedOffset() - O) < 30.0, "smoothed offset is also near the truth");
        CHECK(cs.rtt() >= 40.0 && cs.rtt() <= 120.0, "best RTT is within the simulated range");
    }

    // --- 3. Asymmetric constant delays bias offset by (up-down)/2. ---
    {
        ClockSync cs;
        const double O = 200.0;
        const double up = 30.0, down = 10.0; // asymmetric
        double t1, t2, t3;
        exchange(0.0, O, up, down, 1.0, t1, t2, t3);
        cs.addSample(0.0, t1, t2, t3);
        const double expected = O + (up - down) / 2.0; // known NTP path-asymmetry bias
        CHECK(near(cs.offset(), expected), "asymmetric delays bias the offset by (up-down)/2");
        CHECK(near(cs.rtt(), up + down), "RTT is independent of the asymmetry");
    }

    // --- 4. clear() + window. ---
    {
        ClockSync cs(4);
        for (int i = 0; i < 10; ++i) {
            double t1, t2, t3;
            exchange(static_cast<double>(i), 0.0, 5.0, 5.0, 0.0, t1, t2, t3);
            cs.addSample(static_cast<double>(i), t1, t2, t3);
        }
        CHECK(cs.sampleCount() == 4, "window caps retained samples");
        cs.clear();
        CHECK(!cs.ready() && cs.sampleCount() == 0, "clear resets");
    }

    if (g_fail == 0) {
        std::printf("clocksync: OK — symmetric exact, jitter best-sample, asymmetric bias, window/clear.\n");
        return 0;
    }
    std::printf("clocksync: %d failure(s).\n", g_fail);
    return 1;
}
