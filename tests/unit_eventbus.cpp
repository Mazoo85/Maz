// Unit tests for maz::core::EventBus<Event> + Connection. Exercises named-signal
// dispatch (subscribe/emit/disconnect), channel isolation, the generational
// stale-Connection guard, and re-entrant subscribe/disconnect during an emit.
// Pure C++, no GPU/display.

#include "maz/core/EventBus.hpp"

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
    // --- SUBSCRIBE + EMIT -----------------------------------------------------
    {
        EventBus<int> bus;
        int sink = 0;
        auto c = bus.subscribe("a"_sid, [&](const int& v) { sink += v; });
        (void)c;
        bus.emit("a"_sid, 5);
        check(sink == 5, "emit invokes the subscriber");
        bus.emit("a"_sid, 3);
        check(sink == 8, "second emit accumulates");
        check(bus.size("a"_sid) == 1, "one live subscriber on channel a");
    }

    // --- MULTIPLE SUBSCRIBERS, SAME CHANNEL -----------------------------------
    {
        EventBus<int> bus;
        int a = 0, b = 0;
        auto c1 = bus.subscribe("a"_sid, [&](const int& v) { a += v; });
        auto c2 = bus.subscribe("a"_sid, [&](const int& v) { b += v; });
        (void)c1;
        (void)c2;
        bus.emit("a"_sid, 10);
        check(a == 10 && b == 10, "both subscribers fire");
        check(bus.size("a"_sid) == 2, "two live subscribers on channel a");
    }

    // --- CHANNEL ISOLATION ----------------------------------------------------
    {
        EventBus<int> bus;
        int sink = 0;
        auto c = bus.subscribe("a"_sid, [&](const int& v) { sink += v; });
        (void)c;
        bus.emit("b"_sid, 7);
        check(sink == 0, "emit on b does not reach a's subscriber");
        check(bus.size("b"_sid) == 0, "channel b has no subscribers");
    }

    // --- DISCONNECT -----------------------------------------------------------
    {
        EventBus<int> bus;
        int sink = 0;
        auto c = bus.subscribe("a"_sid, [&](const int& v) { sink += v; });
        check(bus.disconnect(c) == true, "disconnect returns true for a live token");
        bus.emit("a"_sid, 4);
        check(sink == 0, "disconnected subscriber does not fire");
        check(bus.size("a"_sid) == 0, "no live subscribers after disconnect");
        check(bus.disconnect(c) == false, "double disconnect is a no-op");
        check(bus.disconnect(Connection{}) == false, "null disconnect is a no-op");
    }

    // --- STALE AFTER RESUBSCRIBE (generational gate) --------------------------
    {
        EventBus<int> bus;
        int s1 = 0, s2 = 0;
        auto c1 = bus.subscribe("a"_sid, [&](const int& v) { s1 += v; });
        bus.disconnect(c1);
        auto c2 = bus.subscribe("a"_sid, [&](const int& v) { s2 += v; });
        check(c2.index == c1.index, "resubscribe reuses the freed slot");
        check(c2.generation != c1.generation, "reused slot has a new generation");
        check(bus.disconnect(c1) == false, "stale disconnect is a no-op");
        bus.emit("a"_sid, 5);
        check(s2 == 5 && s1 == 0, "stale disconnect did not kill the live subscriber");
        check(bus.size("a"_sid) == 1, "one live subscriber after resubscribe");
    }

    // --- RE-ENTRANCY: DISCONNECT DURING EMIT ----------------------------------
    {
        EventBus<int> bus;
        int aHits = 0, bHits = 0;
        Connection cB;
        auto cA = bus.subscribe("a"_sid, [&](const int&) {
            ++aHits;
            bus.disconnect(cB);
        });
        cB = bus.subscribe("a"_sid, [&](const int&) { ++bHits; });
        (void)cA;
        bus.emit("a"_sid, 1);
        check(aHits == 1 && bHits == 0, "B (index 1) disconnected by A before the loop reaches it");
        bus.emit("a"_sid, 1);
        check(aHits == 2 && bHits == 0, "B stays gone on the next emit");
        check(bus.size("a"_sid) == 1, "only A remains live");
    }

    // --- RE-ENTRANCY: SUBSCRIBE DURING EMIT -----------------------------------
    {
        EventBus<int> bus;
        int aHits = 0, nHits = 0;
        bool subscribed = false;
        auto cA = bus.subscribe("a"_sid, [&](const int&) {
            ++aHits;
            if (!subscribed) {
                subscribed = true;
                bus.subscribe("a"_sid, [&](const int&) { ++nHits; });
            }
        });
        (void)cA;
        bus.emit("a"_sid, 1);
        check(aHits == 1 && nHits == 0, "subscriber appended during emit is past the snapshot, not fired");
        bus.emit("a"_sid, 1);
        check(aHits == 2 && nHits == 1, "the new subscriber fires on the next emit");
        check(bus.size("a"_sid) == 2, "two live subscribers after the re-entrant subscribe");
    }

    // --- RE-ENTRANCY: SELF-DISCONNECT DURING EMIT -----------------------------
    {
        EventBus<int> bus;
        int hits = 0;
        Connection cS;
        cS = bus.subscribe("a"_sid, [&](const int&) {
            ++hits;
            bus.disconnect(cS);
        });
        bus.emit("a"_sid, 1);
        check(hits == 1, "self-disconnecting callback completes (fn copied before call)");
        check(bus.size("a"_sid) == 0, "no live subscribers after self-disconnect");
        bus.emit("a"_sid, 1);
        check(hits == 1, "self-disconnected callback is not called again");
    }

    // --- CLEAR ----------------------------------------------------------------
    {
        EventBus<int> bus;
        int s = 0;
        auto ca = bus.subscribe("a"_sid, [&](const int& v) { s += v; });
        auto cb = bus.subscribe("b"_sid, [&](const int& v) { s += v; });
        auto cc = bus.subscribe("c"_sid, [&](const int& v) { s += v; });
        (void)ca;
        (void)cb;
        (void)cc;
        bus.clear();
        check(bus.size("a"_sid) == 0 && bus.size("b"_sid) == 0 && bus.size("c"_sid) == 0
                  && bus.channelCount() == 0,
              "clear() drops every channel and subscriber");
        bus.emit("a"_sid, 1);
        bus.emit("b"_sid, 1);
        bus.emit("c"_sid, 1);
        check(s == 0, "emit after clear reaches nobody");
    }

    // --- CHURN (deterministic subscribe/disconnect interleave) ----------------
    {
        EventBus<int> bus;
        const int kCount = 100;
        Connection connsA[kCount];
        Connection connsB[kCount];
        bool aliveA[kCount] = {};
        bool aliveB[kCount] = {};

        // Build: subscribe all, then disconnect a fixed subset of each channel.
        for (int i = 0; i < kCount; ++i) {
            connsA[i] = bus.subscribe("a"_sid, [&](const int& v) { (void)v; });
            connsB[i] = bus.subscribe("b"_sid, [&](const int& v) { (void)v; });
            aliveA[i] = true;
            aliveB[i] = true;
        }
        for (int i = 0; i < kCount; ++i) {
            if (i % 3 == 0) {  // drop every third A
                bus.disconnect(connsA[i]);
                aliveA[i] = false;
            }
            if (i % 2 == 0) {  // drop every even B
                bus.disconnect(connsB[i]);
                aliveB[i] = false;
            }
        }

        int liveA = 0, liveB = 0;
        for (int i = 0; i < kCount; ++i) {
            if (aliveA[i]) ++liveA;
            if (aliveB[i]) ++liveB;
        }

        // Emit a known value and count how many times each channel's callbacks
        // fire by mutating a shared sink; cross-check against the shadow counts.
        int sinkA = 0, sinkB = 0;
        // The primary `bus` above verifies live-count accounting, but its
        // callbacks are no-ops so they can't measure dispatch. Build a parallel
        // bus2 with sink-mutating callbacks under the SAME subscribe/disconnect
        // pattern, then cross-check the emitted sums against the shadow counts.
        EventBus<int> bus2;
        Connection c2A[kCount];
        Connection c2B[kCount];
        for (int i = 0; i < kCount; ++i) {
            c2A[i] = bus2.subscribe("a"_sid, [&](const int& v) { sinkA += v; });
            c2B[i] = bus2.subscribe("b"_sid, [&](const int& v) { sinkB += v; });
        }
        int expectLiveA = kCount, expectLiveB = kCount;
        for (int i = 0; i < kCount; ++i) {
            if (i % 3 == 0) { bus2.disconnect(c2A[i]); --expectLiveA; }
            if (i % 2 == 0) { bus2.disconnect(c2B[i]); --expectLiveB; }
        }
        bus2.emit("a"_sid, 1);
        bus2.emit("b"_sid, 1);

        bool churnOk = true;
        churnOk = churnOk && (sinkA == expectLiveA);
        churnOk = churnOk && (sinkB == expectLiveB);
        churnOk = churnOk && (bus.size("a"_sid) == static_cast<std::size_t>(liveA));
        churnOk = churnOk && (bus.size("b"_sid) == static_cast<std::size_t>(liveB));
        churnOk = churnOk && (bus2.size("a"_sid) == static_cast<std::size_t>(expectLiveA));
        churnOk = churnOk && (bus2.size("b"_sid) == static_cast<std::size_t>(expectLiveB));
        check(churnOk, "churn: live counts and summed dispatch match the shadow model");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
