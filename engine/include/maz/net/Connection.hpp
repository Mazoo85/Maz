#pragma once

#include "maz/net/BitStream.hpp"
#include "maz/net/Reliability.hpp"

#include <cstdint>
#include <vector>

// maz::net connection / packet framing — the glue that turns the reliability layer (M213) into
// actual framed packets, the piece that sits directly beneath a real UDP socket. Every packet
// carries a header: a protocol id (a magic number that rejects foreign or corrupt datagrams) plus
// the reliability triplet — this packet's sequence, the latest sequence we've received from the
// peer, and the 32-bit bitfield of the 32 before it. A Connection assigns outgoing sequences,
// learns from each incoming header which of its in-flight packets the peer has acknowledged (so it
// can stop resending them and sample RTT), and records the peer's sequences to build its own ack
// header. There are NO sockets here — you hand it a payload to frame (pack) and hand it received
// bytes to parse (unpack); binding this to a real UDP/ENet socket is the one remaining [DESK] step.
// Pure, deterministic, unit-tested.
namespace maz::net {

struct PacketHeader {
    uint32_t protocolId = 0;
    uint16_t seq = 0;
    uint16_t ack = 0;
    uint32_t ackBits = 0;
};

// Header is a whole number of bytes (32 + 16 + 16 + 32 = 96 bits) so the payload that follows is
// byte-aligned and can be sliced out directly.
inline constexpr std::size_t kPacketHeaderBytes = 12;

inline void writePacketHeader(BitWriter& w, const PacketHeader& h) {
    w.writeBits(h.protocolId, 32);
    w.writeBits(h.seq, 16);
    w.writeBits(h.ack, 16);
    w.writeBits(h.ackBits, 32);
}

inline bool readPacketHeader(BitReader& r, PacketHeader& h) {
    h.protocolId = r.readBits(32);
    h.seq = static_cast<uint16_t>(r.readBits(16));
    h.ack = static_cast<uint16_t>(r.readBits(16));
    h.ackBits = r.readBits(32);
    return r.ok();
}

class Connection {
  public:
    explicit Connection(uint32_t protocolId) : m_protocol(protocolId) {}

    // Frame an outgoing packet: header (fresh sequence + our current ack state for the peer)
    // followed by the payload bytes. `outSeq` receives the assigned sequence so the caller can
    // timestamp it for RTT. The returned bytes are what you'd hand to sendto().
    std::vector<uint8_t> pack(const uint8_t* payload, std::size_t n, uint16_t& outSeq) {
        PacketHeader h;
        h.protocolId = m_protocol;
        h.seq = m_sender.next();
        h.ack = m_receiver.hasAck() ? m_receiver.ack() : 0;
        h.ackBits = m_receiver.ackBits();
        outSeq = h.seq;

        BitWriter w;
        writePacketHeader(w, h);
        if (payload != nullptr && n > 0) {
            w.writeBytes(payload, n);
        }
        return w.bytes();
    }

    std::vector<uint8_t> pack(const std::vector<uint8_t>& payload, uint16_t& outSeq) {
        return pack(payload.data(), payload.size(), outSeq);
    }

    // Parse a received packet. Rejects a truncated header or a mismatched protocol id (returns
    // false, incrementing rejectedPackets). On success: records the peer's sequence for our future
    // ack headers, resolves the peer's ack header into our newly-acknowledged sequences
    // (lastAcked()), and copies the payload (everything after the header) into `payloadOut`.
    bool unpack(const std::vector<uint8_t>& bytes, std::vector<uint8_t>& payloadOut) {
        BitReader r(bytes);
        PacketHeader h;
        if (!readPacketHeader(r, h)) {
            ++m_rejected;
            return false;
        }
        if (h.protocolId != m_protocol) {
            ++m_rejected;
            return false;
        }
        m_receiver.onReceived(h.seq);
        m_lastAcked = m_sender.onAck(h.ack, h.ackBits);
        payloadOut.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kPacketHeaderBytes),
                          bytes.end());
        return true;
    }

    const std::vector<uint16_t>& lastAcked() const { return m_lastAcked; }
    std::size_t inFlightCount() const { return m_sender.inFlightCount(); }
    bool inFlight(uint16_t seq) const { return m_sender.inFlight(seq); }
    bool wasReceived(uint16_t seq) const { return m_receiver.wasReceived(seq); }
    uint32_t rejectedPackets() const { return m_rejected; }
    uint32_t protocolId() const { return m_protocol; }

  private:
    uint32_t m_protocol;
    AckSender m_sender;
    AckReceiver m_receiver;
    std::vector<uint16_t> m_lastAcked;
    uint32_t m_rejected = 0;
};

} // namespace maz::net
