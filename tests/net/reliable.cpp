// tests/net/reliable.cpp — proves net::ReliableChannel delivers messages reliably and IN ORDER over
// the real UDP transport, even across a link that drops ~40% of packets in both directions.
//
// Two real loopback sockets, a ReliableChannel on each. Side A sends 50 numbered messages; every
// send (data AND acks, both directions) is randomly dropped 40% of the time via the channel's
// dropSend hook. The retransmit + ack + reorder logic must still deliver all 50 exactly once, in
// order. Deterministic PRNG so the test is reproducible. Exits non-zero on any failure.

#include "maz/net/ReliableChannel.hpp"
#include "maz/net/UdpSocket.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using maz::net::Endpoint;
using maz::net::ReliableChannel;
using maz::net::UdpSocket;

static int g_failures = 0;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s\n", (msg));                                  \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Small deterministic LCG so the 40%-loss pattern is reproducible run to run.
struct Lcg {
    std::uint32_t s;
    explicit Lcg(std::uint32_t seed) : s(seed) {}
    std::uint32_t next() { s = s * 1664525u + 1013904223u; return s; }
    bool drop(int pctDrop) { return (next() >> 8) % 100u < static_cast<std::uint32_t>(pctDrop); }
};

int main() {
    UdpSocket a, b;
    CHECK(a.open() && b.open(), "open sockets");
    CHECK(a.bind(0, "127.0.0.1") && b.bind(0, "127.0.0.1"), "bind loopback");
    CHECK(a.setNonBlocking(true) && b.setNonBlocking(true), "non-blocking");
    const std::uint16_t pa = a.localPort();
    const std::uint16_t pb = b.localPort();
    CHECK(pa != 0 && pb != 0, "ephemeral ports");

    ReliableChannel cA(&a, Endpoint("127.0.0.1", pb));
    ReliableChannel cB(&b, Endpoint("127.0.0.1", pa));

    // 40% packet loss in BOTH directions (data from A, acks from B, and vice-versa).
    Lcg rngA(12345u), rngB(67890u);
    cA.dropSend = [&rngA] { return rngA.drop(40); };
    cB.dropSend = [&rngB] { return rngB.drop(40); };

    const int kMsgs = 50;
    for (int i = 0; i < kMsgs; ++i) {
        const std::string m = "message-" + std::to_string(i);
        cA.sendReliable(m.data(), m.size());
    }

    std::vector<int> receivedOrder;
    std::uint32_t now = 0;
    for (int step = 0; step < 6000 && static_cast<int>(receivedOrder.size()) < kMsgs; ++step) {
        cA.update(now);
        cB.update(now);
        std::vector<std::uint8_t> msg;
        while (cB.receive(msg)) {
            const std::string s(msg.begin(), msg.end());
            int idx = -1;
            if (s.rfind("message-", 0) == 0) idx = std::atoi(s.c_str() + 8);
            receivedOrder.push_back(idx);
        }
        now += 5;  // 5 ms of virtual time per step (retransmit timer is 40 ms)
    }

    CHECK(static_cast<int>(receivedOrder.size()) == kMsgs, "all 50 messages delivered over 40% loss");
    bool ordered = (static_cast<int>(receivedOrder.size()) == kMsgs);
    for (int i = 0; i < static_cast<int>(receivedOrder.size()); ++i) {
        if (receivedOrder[static_cast<std::size_t>(i)] != i) { ordered = false; break; }
    }
    CHECK(ordered, "messages delivered exactly once, strictly in order");

    if (g_failures == 0) {
        std::printf("net_reliable: OK — 50/50 messages reliable + in order across a 40%%-loss link.\n");
        return 0;
    }
    std::printf("net_reliable: %d failure(s); delivered %zu.\n", g_failures, receivedOrder.size());
    return 1;
}
