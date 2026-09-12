// tests/game/chargepool.cpp — verifies the multi-charge ability pool (game::ChargePool). Ground truths: a
// pool starts full; tryUse spends charges and fails at zero; a shared recharge clock refills one charge per
// interval; spending mid-recharge does not reset the in-progress timer; fraction reports progress toward the
// next charge (0 when full); a large dt restores several charges without banking leftover time; a zero
// recharge time refills instantly; add/refill/drain and degenerate max=0 behave. Pure CPU, deterministic.
#include "maz/game/ChargePool.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::ChargePool;

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

int main() {
    // --- 1. Starts full; spend down to empty; empty use fails. ---
    {
        ChargePool p(3, 2.0);
        CHECK(p.charges() == 3 && p.max() == 3 && p.isFull(), "starts full 3/3");
        CHECK(near(p.fraction(), 0.0), "full -> fraction 0");
        CHECK(p.tryUse() && p.charges() == 2, "use 1 -> 2");
        CHECK(p.tryUse() && p.tryUse() && p.charges() == 0, "use down to 0");
        CHECK(!p.tryUse(), "use at 0 fails");
        CHECK(!p.canUse(), "canUse false at 0");
    }

    // --- 2. Recharge one charge per interval; fraction tracks progress. ---
    {
        ChargePool p(3, 2.0);
        p.tryUse(); // 2/3, clock starts at 0
        CHECK(near(p.fraction(), 0.0), "just used, fraction 0");
        p.tick(1.0);
        CHECK(p.charges() == 2 && near(p.fraction(), 0.5), "half-way to next charge");
        p.tick(1.0);
        CHECK(p.charges() == 3 && p.isFull() && near(p.fraction(), 0.0), "recharged to full");
    }

    // --- 3. Spending mid-recharge keeps the in-progress timer. ---
    {
        ChargePool p(3, 2.0);
        p.tryUse();       // 2/3, elapsed 0
        p.tick(1.0);      // elapsed 1.0 (halfway)
        p.tryUse();       // 1/3, elapsed STAYS 1.0
        CHECK(p.charges() == 1 && near(p.fraction(), 0.5), "mid-recharge use keeps elapsed");
        p.tick(1.0);      // elapsed 2.0 -> +1 charge
        CHECK(p.charges() == 2 && near(p.fraction(), 0.0), "next charge completes on schedule");
    }

    // --- 4. A large dt restores several charges; leftover time is not banked. ---
    {
        ChargePool p(3, 2.0);
        p.tryUse(); p.tryUse(); p.tryUse(); // 0/3
        p.tick(10.0); // 3 charges need 6.0; the extra 4.0 must not carry over
        CHECK(p.charges() == 3 && p.isFull(), "big dt refills to max");
        CHECK(near(p.fraction(), 0.0), "leftover time not banked when full");
    }

    // --- 5. Exactly-one-interval steps from empty. ---
    {
        ChargePool p(3, 2.0);
        p.drain();
        CHECK(p.charges() == 0, "drained to 0");
        p.tick(2.0);
        CHECK(p.charges() == 1, "one interval -> 1 charge");
        p.tick(2.0);
        CHECK(p.charges() == 2, "two intervals -> 2 charges");
    }

    // --- 6. Zero recharge time refills instantly on any positive tick. ---
    {
        ChargePool p(2, 0.0);
        p.tryUse();
        CHECK(p.charges() == 1, "used -> 1");
        p.tick(0.016);
        CHECK(p.charges() == 2 && p.isFull(), "zero recharge -> instant refill");
    }

    // --- 7. add / refill / drain and clamping. ---
    {
        ChargePool p(3, 1.0);
        p.drain();
        p.add(2);
        CHECK(p.charges() == 2, "add 2 -> 2");
        p.add(5);
        CHECK(p.charges() == 3, "add clamps to max");
        p.tryUse();
        p.refill();
        CHECK(p.charges() == 3 && p.isFull(), "refill -> full");
    }

    // --- 8. Degenerate max = 0. ---
    {
        ChargePool p(0, 1.0);
        CHECK(p.charges() == 0 && p.isFull(), "max 0 -> full-and-empty");
        CHECK(!p.tryUse(), "max 0 cannot use");
        p.tick(5.0); // no-op, no crash
        CHECK(p.charges() == 0, "max 0 stays 0");
    }

    if (g_fail == 0) {
        std::printf("charge pool: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
