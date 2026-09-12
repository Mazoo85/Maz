// tests/core/tokenbucket.cpp — verifies the token-bucket rate limiter (core TokenBucket.hpp).
// Ground truths, deterministic (explicit dt, no clock):
//   * a full bucket allows a burst up to capacity, then refuses;
//   * refill accrues at refillPerSecond and clamps at capacity;
//   * fractional consume works; n<=0 is a no-op success; n>capacity always fails;
//   * timeUntil reports 0 when available, the correct wait otherwise, and +inf when unreachable;
//   * a sustained-rate scenario: over a long run the number of allowed actions tracks the refill budget;
//   * setters (drain/refillToFull/setTokens/setRefillRate) behave.
#include "maz/core/TokenBucket.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b, double e = 1e-9) { return std::fabs(a - b) < e; }

int main() {
    using maz::core::TokenBucket;

    // --- 1. Burst then refuse. ---
    {
        TokenBucket tb(3.0, 1.0); // capacity 3, refill 1/s, starts full
        CHECK(tb.full() && near(tb.tokens(), 3.0), "starts full");
        CHECK(tb.tryConsume() && tb.tryConsume() && tb.tryConsume(), "three actions in a burst");
        CHECK(near(tb.tokens(), 0.0) && tb.empty(), "bucket empty after the burst");
        CHECK(!tb.tryConsume(), "fourth action refused when empty");
    }

    // --- 2. Refill accrues and clamps. ---
    {
        TokenBucket tb(3.0, 2.0, 0.0); // starts empty, refill 2/s
        tb.advance(0.5);               // +1.0
        CHECK(near(tb.tokens(), 1.0), "half a second refills one token at 2/s");
        CHECK(tb.tryConsume(1.0) && near(tb.tokens(), 0.0), "consume the accrued token");
        tb.advance(100.0);             // way past capacity
        CHECK(near(tb.tokens(), 3.0) && tb.full(), "refill clamps at capacity");
    }

    // --- 3. Fractional / edge consume. ---
    {
        TokenBucket tb(10.0, 1.0);
        CHECK(tb.tryConsume(2.5) && near(tb.tokens(), 7.5), "fractional consume");
        CHECK(tb.tryConsume(0.0), "consuming zero always succeeds");
        CHECK(tb.tryConsume(-5.0), "consuming a negative amount is a no-op success");
        CHECK(near(tb.tokens(), 7.5), "no-op consumes did not change tokens");
        CHECK(!tb.tryConsume(100.0), "consuming more than capacity fails");
        CHECK(near(tb.tokens(), 7.5), "a failed consume leaves the bucket unchanged");
    }

    // --- 4. timeUntil. ---
    {
        TokenBucket tb(5.0, 2.0, 1.0); // 1 token now, refill 2/s
        CHECK(near(tb.timeUntil(1.0), 0.0), "already have 1 -> 0 wait");
        CHECK(near(tb.timeUntil(5.0), (5.0 - 1.0) / 2.0), "wait for 5 tokens at 2/s = 2s");
        CHECK(std::isinf(tb.timeUntil(6.0)), "more than capacity is unreachable (+inf)");
        TokenBucket noRefill(5.0, 0.0, 1.0);
        CHECK(std::isinf(noRefill.timeUntil(2.0)), "no refill and not enough now -> +inf");
    }

    // --- 5. Sustained rate: allowed actions track the budget. ---
    {
        // capacity 5, refill 10/s. Try to consume 1 every 10 ms for 1 second (100 attempts).
        // Budget = starting 5 + refill 10/s * 1s = 15 tokens -> ~15 actions allowed.
        TokenBucket tb(5.0, 10.0);
        int allowed = 0;
        for (int i = 0; i < 100; ++i) {
            tb.advance(0.01);
            if (tb.tryConsume(1.0)) ++allowed;
        }
        // Starting full (5) plus 100*0.01*10 = 10 refilled => 15 total, minus at most one boundary token.
        CHECK(allowed >= 14 && allowed <= 16, "sustained allowed count matches the token budget (~15)");
    }

    // --- 6. Setters. ---
    {
        TokenBucket tb(4.0, 1.0);
        tb.drain();
        CHECK(tb.empty(), "drain empties");
        tb.refillToFull();
        CHECK(tb.full(), "refillToFull fills");
        tb.setTokens(2.0);
        CHECK(near(tb.tokens(), 2.0), "setTokens sets");
        tb.setTokens(99.0);
        CHECK(near(tb.tokens(), 4.0), "setTokens clamps to capacity");
        tb.setRefillRate(5.0);
        CHECK(near(tb.refillRate(), 5.0), "setRefillRate updates");
    }

    if (g_fail == 0) {
        std::printf("tokenbucket: OK — burst, refill/clamp, fractional, timeUntil, sustained, setters.\n");
        return 0;
    }
    std::printf("tokenbucket: %d failure(s).\n", g_fail);
    return 1;
}
