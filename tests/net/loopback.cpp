// tests/net/loopback.cpp — proves the real UDP transport (maz::net::UdpSocket) actually moves bytes
// between two independent sockets over the OS network stack, on the loopback interface.
//
// This is the "networking between two machines" proof at the smallest honest scale: two sockets bound
// to 127.0.0.1 exchange datagrams through the kernel exactly as two physical machines would — only the
// destination IP differs. It is a real socket round-trip, not a mock. Exits non-zero on any failure.

#include "maz/net/UdpSocket.hpp"

#include <cstdio>
#include <cstring>
#include <string>

using maz::net::Endpoint;
using maz::net::UdpSocket;

static int g_failures = 0;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s\n", (msg));                                  \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Poll a non-blocking socket for up to ~1s so the test can never hang.
static long recvWait(UdpSocket& s, void* buf, std::size_t cap, Endpoint& from) {
    for (int i = 0; i < 200000; ++i) {
        long n = s.recvFrom(buf, cap, from);
        if (n >= 0) return n;
    }
    return -1;
}

int main() {
    UdpSocket a, b;
    CHECK(a.open() && b.open(), "open two UDP sockets");
    CHECK(a.bind(0, "127.0.0.1"), "bind A to loopback (ephemeral)");
    CHECK(b.bind(0, "127.0.0.1"), "bind B to loopback (ephemeral)");
    CHECK(a.setNonBlocking(true) && b.setNonBlocking(true), "set non-blocking");

    const std::uint16_t pa = a.localPort();
    const std::uint16_t pb = b.localPort();
    CHECK(pa != 0 && pb != 0, "OS assigned real ephemeral ports");

    // B -> A
    const char* m1 = "hello from B";
    const long l1 = static_cast<long>(std::strlen(m1));
    CHECK(b.sendTo(m1, static_cast<std::size_t>(l1), Endpoint("127.0.0.1", pa)) == l1, "B sendTo A");

    char buf[256];
    Endpoint from;
    long got = recvWait(a, buf, sizeof(buf), from);
    CHECK(got == l1, "A received the datagram");
    if (got == l1) {
        buf[got] = '\0';
        CHECK(std::string(buf) == m1, "A payload matches");
        CHECK(from.port == pb, "A learned B's real source port");
        CHECK(from.ip == "127.0.0.1", "A saw loopback source ip");
    }

    // A -> B (reply to the address A just learned — the real request/response pattern)
    const char* m2 = "ack from A";
    const long l2 = static_cast<long>(std::strlen(m2));
    CHECK(a.sendTo(m2, static_cast<std::size_t>(l2), from) == l2, "A replies to B");
    Endpoint from2;
    got = recvWait(b, buf, sizeof(buf), from2);
    CHECK(got == l2, "B received the reply");
    if (got == l2) {
        buf[got] = '\0';
        CHECK(std::string(buf) == m2, "B payload matches");
        CHECK(from2.port == pa, "B learned A's real source port");
    }

    if (g_failures == 0) {
        std::printf("net_loopback: OK — real UDP round-trip verified between two sockets.\n");
        return 0;
    }
    std::printf("net_loopback: %d failure(s).\n", g_failures);
    return 1;
}
