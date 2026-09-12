#pragma once

#include "maz/core/Hash.hpp"   // core::sha1
#include "maz/io/Base64.hpp"   // io::base64Encode

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// maz::net WebSocket (RFC 6455) handshake + frame codec — the transport a browser game MUST use for
// networking. A WASM build (see WebLoop.hpp / WEB_BUILD.md) cannot open the raw UDP sockets in
// `net::UdpSocket`; browsers only expose WebSocket (and WebRTC). WebSocket rides over a normal TCP/HTTP
// connection: the client sends an HTTP Upgrade with a random `Sec-WebSocket-Key`, the server replies with a
// `Sec-WebSocket-Accept` derived from it, and thereafter both sides exchange length-prefixed, optionally
// XOR-masked binary/text FRAMES. This header implements the two pure-logic pieces — the accept-key
// derivation and the frame encode/decode — so a Maz server can speak WebSocket to browser clients (over the
// real TCP socket the app owns). Both pieces are exact-spec and unit-tested against RFC 6455's own worked
// examples, with no live socket needed.
//
// Scope note (honest): framing + handshake key (the parts that are pure bytes). The TCP accept loop and the
// HTTP header exchange are the app's socket code; `serverHandshakeResponse` builds the response string for
// it, and `parseClientKey` pulls the key out of the client's request.
namespace maz::net {

// WebSocket frame opcodes (RFC 6455 §5.2).
enum class WsOpcode : std::uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA,
};

// The magic GUID every WebSocket server appends to the client key before hashing (RFC 6455 §1.3).
inline constexpr const char* kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Compute Sec-WebSocket-Accept from the client's Sec-WebSocket-Key: base64(SHA1(key + GUID)).
inline std::string wsAcceptKey(const std::string& clientKey) {
    const std::array<std::uint8_t, 20> digest = core::sha1(clientKey + kWsGuid);
    return io::base64Encode(digest.data(), digest.size());
}

// Build the full HTTP 101 handshake response a server sends back to a client, given the client's key.
inline std::string serverHandshakeResponse(const std::string& clientKey) {
    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + wsAcceptKey(clientKey) + "\r\n\r\n";
}

// Pull the Sec-WebSocket-Key value out of a raw client HTTP request (case-insensitive header name).
inline std::optional<std::string> parseClientKey(const std::string& request) {
    const char* needle = "sec-websocket-key:";
    // Case-insensitive find.
    std::string lower;
    lower.reserve(request.size());
    for (char c : request) lower += static_cast<char>((c >= 'A' && c <= 'Z') ? c + 32 : c);
    const std::size_t h = lower.find(needle);
    if (h == std::string::npos) return std::nullopt;
    std::size_t v = h + std::char_traits<char>::length(needle);
    while (v < request.size() && (request[v] == ' ' || request[v] == '\t')) ++v;
    std::size_t e = v;
    while (e < request.size() && request[e] != '\r' && request[e] != '\n') ++e;
    return request.substr(v, e - v);
}

// Encode one WebSocket frame. `mask` non-null => client-to-server framing (payload XOR-masked with the 4
// bytes; the spec requires clients to mask). Server-to-client frames pass mask = nullptr (unmasked).
inline std::vector<std::uint8_t> wsEncodeFrame(WsOpcode opcode, const std::uint8_t* payload,
                                               std::size_t len, const std::uint8_t mask[4] = nullptr) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(0x80 | static_cast<std::uint8_t>(opcode))); // FIN + opcode
    const std::uint8_t maskBit = mask ? 0x80 : 0x00;
    if (len < 126) {
        out.push_back(static_cast<std::uint8_t>(maskBit | len));
    } else if (len <= 0xFFFF) {
        out.push_back(static_cast<std::uint8_t>(maskBit | 126));
        out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(len & 0xff));
    } else {
        out.push_back(static_cast<std::uint8_t>(maskBit | 127));
        for (int s = 56; s >= 0; s -= 8) out.push_back(static_cast<std::uint8_t>((len >> s) & 0xff));
    }
    if (mask) {
        for (int i = 0; i < 4; ++i) out.push_back(mask[i]);
        for (std::size_t i = 0; i < len; ++i)
            out.push_back(static_cast<std::uint8_t>(payload[i] ^ mask[i % 4]));
    } else {
        out.insert(out.end(), payload, payload + len);
    }
    return out;
}

inline std::vector<std::uint8_t> wsEncodeFrame(WsOpcode opcode, const std::string& text,
                                               const std::uint8_t mask[4] = nullptr) {
    return wsEncodeFrame(opcode, reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), mask);
}

struct WsFrame {
    WsOpcode opcode = WsOpcode::Text;
    bool fin = true;
    std::vector<std::uint8_t> payload; // already unmasked
};

// Decode one frame from the front of `data`. Returns the frame and sets `consumed` to the bytes used; returns
// nullopt (consumed = 0) when the buffer doesn't yet hold a complete frame (caller should read more).
inline std::optional<WsFrame> wsDecodeFrame(const std::uint8_t* data, std::size_t n, std::size_t& consumed) {
    consumed = 0;
    if (n < 2) return std::nullopt;
    const bool fin = (data[0] & 0x80) != 0;
    const WsOpcode opcode = static_cast<WsOpcode>(data[0] & 0x0f);
    const bool masked = (data[1] & 0x80) != 0;
    std::uint64_t len = data[1] & 0x7f;
    std::size_t p = 2;
    if (len == 126) {
        if (n < 4) return std::nullopt;
        len = (static_cast<std::uint64_t>(data[2]) << 8) | data[3];
        p = 4;
    } else if (len == 127) {
        if (n < 10) return std::nullopt;
        len = 0;
        for (int i = 0; i < 8; ++i) len = (len << 8) | data[2 + static_cast<std::size_t>(i)];
        p = 10;
    }
    std::uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        if (n < p + 4) return std::nullopt;
        for (int i = 0; i < 4; ++i) mask[i] = data[p + static_cast<std::size_t>(i)];
        p += 4;
    }
    // Overflow-safe length check: `len` is a full 64-bit field from the frame, so a value near UINT64_MAX
    // would make `p + len` wrap and slip past a naive `n < p + len`, after which resize(len) throws
    // std::length_error and crashes the process — a remote DoS from a single malicious frame. `p <= n` holds
    // here, so compare `len` against the true remaining byte count instead of forming `p + len`.
    if (len > static_cast<std::uint64_t>(n - p)) return std::nullopt; // payload not fully arrived (or absurd)

    WsFrame f;
    f.fin = fin;
    f.opcode = opcode;
    f.payload.resize(len);
    for (std::uint64_t i = 0; i < len; ++i) {
        const std::uint8_t b = data[p + i];
        f.payload[i] = masked ? static_cast<std::uint8_t>(b ^ mask[i % 4]) : b;
    }
    consumed = p + static_cast<std::size_t>(len);
    return f;
}

inline std::optional<WsFrame> wsDecodeFrame(const std::vector<std::uint8_t>& data, std::size_t& consumed) {
    return wsDecodeFrame(data.data(), data.size(), consumed);
}

} // namespace maz::net
