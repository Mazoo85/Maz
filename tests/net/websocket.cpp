// tests/net/websocket.cpp — verifies the WebSocket handshake + frame codec (net::wsAcceptKey /
// wsEncodeFrame / wsDecodeFrame) against RFC 6455's OWN worked examples (golden vectors), so correctness is
// proven without a live socket: the canonical accept-key, the canonical unmasked and masked "Hello" frames,
// a header-key parse, an encode/decode round-trip, and the partial-buffer "need more" case.
#include "maz/net/WebSocket.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::net;

int main() {
    // --- 1. RFC 6455 §1.3 canonical accept-key. ---
    {
        const std::string accept = wsAcceptKey("dGhlIHNhbXBsZSBub25jZQ==");
        CHECK(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "canonical Sec-WebSocket-Accept matches RFC 6455");
    }

    // --- 2. Parse the key out of a raw client request; build the 101 response. ---
    {
        const std::string req =
            "GET /chat HTTP/1.1\r\nHost: example.com\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";
        const auto key = parseClientKey(req);
        CHECK(key.has_value() && *key == "dGhlIHNhbXBsZSBub25jZQ==", "client key parsed from request");
        const std::string resp = serverHandshakeResponse(*key);
        CHECK(resp.find("101 Switching Protocols") != std::string::npos, "response is a 101");
        CHECK(resp.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos,
              "response carries the correct accept key");
    }

    // --- 3. RFC 6455 §5.7 unmasked server frame for "Hello" = 0x81 0x05 H e l l o. ---
    {
        const std::vector<std::uint8_t> frame = wsEncodeFrame(WsOpcode::Text, "Hello");
        const std::vector<std::uint8_t> expect = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
        CHECK(frame == expect, "unmasked 'Hello' text frame matches RFC bytes");
    }

    // --- 4. RFC 6455 §5.7 masked client frame for "Hello" (mask 0x37 0xfa 0x21 0x3d). ---
    {
        std::uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
        const std::vector<std::uint8_t> frame = wsEncodeFrame(WsOpcode::Text, "Hello", mask);
        const std::vector<std::uint8_t> expect = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d,
                                                  0x7f, 0x9f, 0x4d, 0x51, 0x58};
        CHECK(frame == expect, "masked 'Hello' frame matches RFC bytes");

        // ...and decoding those bytes recovers the unmasked payload + opcode.
        std::size_t consumed = 0;
        const auto f = wsDecodeFrame(expect, consumed);
        CHECK(f.has_value(), "masked frame decodes");
        CHECK(f && f->opcode == WsOpcode::Text && f->fin, "opcode Text, FIN set");
        CHECK(f && std::string(f->payload.begin(), f->payload.end()) == "Hello", "payload unmasked to 'Hello'");
        CHECK(consumed == expect.size(), "consumed the whole frame");
    }

    // --- 5. Round-trip a larger binary payload through the 16-bit length path. ---
    {
        std::vector<std::uint8_t> big(300);
        for (std::size_t i = 0; i < big.size(); ++i) big[i] = static_cast<std::uint8_t>(i * 7 + 1);
        std::uint8_t mask[4] = {0x11, 0x22, 0x33, 0x44};
        const std::vector<std::uint8_t> frame = wsEncodeFrame(WsOpcode::Binary, big.data(), big.size(), mask);
        CHECK(frame[1] == (0x80 | 126), "300-byte payload uses the 16-bit extended length");
        std::size_t consumed = 0;
        const auto f = wsDecodeFrame(frame, consumed);
        CHECK(f && f->opcode == WsOpcode::Binary && f->payload == big, "binary payload round-trips exactly");
    }

    // --- 6. A partial buffer reports "need more" (nullopt, consumed 0). ---
    {
        const std::vector<std::uint8_t> partial = {0x81, 0x85, 0x37, 0xfa}; // header says masked len 5, truncated
        std::size_t consumed = 999;
        const auto f = wsDecodeFrame(partial, consumed);
        CHECK(!f.has_value() && consumed == 0, "incomplete frame yields nullopt, consumed 0");
    }

    if (g_fail == 0) {
        std::printf("websocket: OK — RFC accept-key, masked/unmasked frames, round-trip, need-more.\n");
        return 0;
    }
    std::printf("websocket: %d failure(s).\n", g_fail);
    return 1;
}
