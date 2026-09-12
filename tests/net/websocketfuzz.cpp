// Malformed-input robustness fuzz for the WebSocket frame decoder (net::wsDecodeFrame) — the parser a Maz
// server runs on every byte a *remote* browser client sends, the most exposed untrusted-input surface in
// the engine. A single hostile frame must never crash the process. Feeds: the specific length-field overflow
// that crashed it (a 127-marked frame whose 64-bit length is near UINT64_MAX made `p + len` wrap past the
// `n < p + len` guard, then resize(len) threw std::length_error), truncations/mutations of valid encoded
// frames, and random buffers. wsDecodeFrame may return nullopt on bad/partial input — it just must never
// throw, over-allocate, or read out of bounds. Built under ASan+UBSan (the sanitizer CI job runs it).
#include "maz/net/WebSocket.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feed(const std::uint8_t* d, std::size_t n) {
    std::size_t consumed = 0;
    auto f = net::wsDecodeFrame(d, n, consumed);
    if (f) g_sink += f->payload.size() + consumed;
}

static void truncateAndMutate(std::vector<std::uint8_t> seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) feed(seed.data(), k);
    const std::uint8_t patches[3] = {0x00, 0xFF, 0x7f};
    for (std::size_t i = 0; i < seed.size(); ++i) {
        const std::uint8_t orig = seed[i];
        for (std::uint8_t patch : patches) {
            seed[i] = patch;
            feed(seed.data(), seed.size());
        }
        seed[i] = orig;
    }
}

int main() {
    // The exact regression: 127-marked length near UINT64_MAX must be rejected, not crash.
    {
        std::vector<std::uint8_t> f = {0x82, 0x7f, 0xFF, 0xFF, 0xFF, 0xFF,
                                       0xFF, 0xFF, 0xFF, 0xFF, 1, 2, 3};
        feed(f.data(), f.size());
        // A 126-marked 16-bit huge length against a short buffer, too.
        std::vector<std::uint8_t> g = {0x82, 0x7e, 0xFF, 0xFF, 1, 2, 3};
        feed(g.data(), g.size());
    }

    // Valid encoded frames (masked + unmasked, each opcode) → truncation + mutation sweeps.
    const std::uint8_t mask[4] = {0xA1, 0xB2, 0xC3, 0xD4};
    truncateAndMutate(net::wsEncodeFrame(net::WsOpcode::Text, "hello world", nullptr));
    truncateAndMutate(net::wsEncodeFrame(net::WsOpcode::Binary, std::string(200, 'x'), mask));
    truncateAndMutate(net::wsEncodeFrame(net::WsOpcode::Ping, "", nullptr));
    truncateAndMutate(net::wsEncodeFrame(net::WsOpcode::Close, "bye", mask));

    // Random buffers, half with the FIN|binary first byte so the parser gets past the opcode.
    std::mt19937 rng(0x7A9Bu);
    std::vector<std::uint8_t> buf;
    for (int iter = 0; iter < 40000; ++iter) {
        const std::size_t n = rng() % 273u;
        buf.resize(n);
        for (std::size_t i = 0; i < n; ++i) buf[i] = static_cast<std::uint8_t>(rng());
        if (n >= 1 && (iter & 1) == 0) buf[0] = 0x82; // FIN + binary
        feed(buf.data(), n);
    }

    std::printf("websocket fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
