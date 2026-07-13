// Unit tests for maz::core::RingBuffer<T>. Exercises the fixed-capacity FIFO:
// push/pop, wrap-around correctness, front() tracking the oldest element, and
// the degenerate capacity-0 buffer. Pure C++, no GPU/display.

#include "maz/core/RingBuffer.hpp"

#include <cstdint>
#include <cstdio>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- CONSTRUCT ------------------------------------------------------------
    {
        RingBuffer<int> r(4);
        check(r.empty(), "fresh buffer is empty()");
        check(!r.full(), "fresh buffer is not full()");
        check(r.size() == 0, "fresh buffer size() == 0");
        check(r.capacity() == 4, "capacity() reports the constructed capacity");
        check(r.front() == nullptr, "front() is nullptr when empty");
        int out = 0;
        check(!r.pop(out), "pop() on empty returns false");
    }

    // --- PUSH UNTIL FULL ------------------------------------------------------
    {
        RingBuffer<int> r(4);
        check(r.push(1), "push 1 succeeds");
        check(r.push(2), "push 2 succeeds");
        check(r.push(3), "push 3 succeeds");
        check(r.push(4), "push 4 succeeds");
        check(!r.push(99), "push on full returns false");
        check(r.size() == 4, "size() == 4 when full");
        check(r.full(), "full() true when at capacity");
        check(!r.empty(), "empty() false when full");
        check(r.front() != nullptr && *r.front() == 1, "front() is the first pushed value");
    }

    // --- FIFO ORDER -----------------------------------------------------------
    {
        RingBuffer<int> r(3);
        r.push(1);
        r.push(2);
        r.push(3);
        int out = 0;
        check(r.pop(out) && out == 1, "pop yields 1 first");
        check(r.size() == 2, "size() == 2 after first pop");
        check(r.pop(out) && out == 2, "pop yields 2 second");
        check(r.size() == 1, "size() == 1 after second pop");
        check(r.pop(out) && out == 3, "pop yields 3 third");
        check(r.size() == 0, "size() == 0 after third pop");
        check(r.empty(), "empty() after draining");
        check(!r.pop(out), "pop() on drained buffer returns false");
    }

    // --- WRAP-AROUND (KEY gate) ----------------------------------------------
    {
        RingBuffer<int> r(4);
        check(r.push(1) && r.push(2) && r.push(3) && r.push(4), "fill 1,2,3,4");
        int out = 0;
        check(r.pop(out) && out == 1, "pop 1");
        check(r.pop(out) && out == 2, "pop 2");
        check(r.size() == 2, "size() == 2 after two pops");
        check(r.push(5), "push 5 into wrapped slot");
        check(r.push(6), "push 6 into wrapped slot");
        check(r.pop(out) && out == 3, "wrapped pop yields 3");
        check(r.pop(out) && out == 4, "wrapped pop yields 4");
        check(r.pop(out) && out == 5, "wrapped pop yields 5");
        check(r.pop(out) && out == 6, "wrapped pop yields 6");
        check(r.empty(), "empty() after draining wrapped buffer");
    }

    // --- front() REFLECTS OLDEST ACROSS PUSH/POP -----------------------------
    {
        RingBuffer<int> r(4);
        r.push(10);
        r.push(20);
        check(r.front() != nullptr && *r.front() == 10, "front() == 10 (oldest)");
        int out = 0;
        r.pop(out);
        check(r.front() != nullptr && *r.front() == 20, "front() == 20 after pop");
        r.pop(out);
        check(r.front() == nullptr, "front() nullptr after draining");
    }

    // --- CLEAR ----------------------------------------------------------------
    {
        RingBuffer<int> r(4);
        r.push(1);
        r.push(2);
        r.push(3);
        r.clear();
        check(r.empty(), "empty() after clear()");
        check(r.size() == 0, "size() == 0 after clear()");
        check(r.capacity() == 4, "capacity() unchanged after clear()");
        int out = 0;
        check(!r.pop(out), "pop() on cleared buffer returns false");
        check(r.push(7), "push after clear() succeeds");
        check(r.front() != nullptr && *r.front() == 7, "front() == 7 after push post-clear");
        check(r.size() == 1, "size() == 1 after push post-clear");
    }

    // --- CAPACITY 0 -----------------------------------------------------------
    {
        RingBuffer<int> z(0);
        check(z.capacity() == 0, "capacity() == 0");
        check(z.empty(), "capacity-0 buffer is empty()");
        check(z.full(), "capacity-0 buffer is full()");
        check(!z.push(1), "push on capacity-0 returns false (no div-by-zero)");
        int out = 0;
        check(!z.pop(out), "pop on capacity-0 returns false");
        check(z.front() == nullptr, "front() nullptr on capacity-0");
    }

    // --- CHURN LOOP WITH SHADOW COUNT ----------------------------------------
    {
        RingBuffer<int> r(8);
        int nextPush = 0;
        int expectPop = 0;
        int shadow = 0;
        bool churnFifoOk = true;
        bool churnSizeOk = true;
        for (int i = 0; i < 1000; ++i) {
            if (i % 3 != 0 && !r.full()) {
                r.push(nextPush++);
                ++shadow;
            } else if (!r.empty()) {
                int out = 0;
                r.pop(out);
                if (out != expectPop) {
                    churnFifoOk = false;
                }
                ++expectPop;
                --shadow;
            }
            if (static_cast<int>(r.size()) != shadow) {
                churnSizeOk = false;
            }
        }
        check(churnFifoOk, "churn FIFO order");
        check(churnSizeOk, "churn size matches shadow");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
