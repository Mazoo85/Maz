#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

// maz::net reliability layer — the ack system that turns raw unreliable packets (UDP) into a
// channel that KNOWS what arrived, without forcing TCP's head-of-line blocking. Every packet
// carries a 16-bit sequence number plus an ACK of the most recent sequence the peer received and a
// 32-bit bitfield of the 32 before it — so one returning packet acknowledges up to 33 at once, and
// lost acks self-heal on the next packet. From the acks a sender learns which of its in-flight
// packets landed (resend the rest) and can measure RTT. This is Glenn Fiedler's reliable-UDP model
// (what ENet/Godot's multiplayer do under the hood), kept pure and unit-tested — no sockets here.
namespace maz::net {

// Sequence comparison with 16-bit wraparound: is `a` more recent than `b`? (65535 -> 0 counts as
// forward). The half-range test handles the wrap.
inline bool seqGreaterThan(uint16_t a, uint16_t b) {
    return ((a > b) && (a - b <= 32768)) || ((a < b) && (b - a > 32768));
}

// Receive side: records which recent sequences arrived and produces the (ack, ackBits) to send
// back to the peer. bit i of ackBits corresponds to sequence (ack - 1 - i).
class AckReceiver {
  public:
    void onReceived(uint16_t seq) {
        if (!m_any) {
            m_any = true;
            m_latest = seq;
            m_bits = 0;
            return;
        }
        if (seqGreaterThan(seq, m_latest)) {
            const uint16_t shift = static_cast<uint16_t>(seq - m_latest);
            if (shift >= 32) {
                m_bits = 0;
            } else {
                m_bits <<= shift;
                m_bits |= (1u << (shift - 1)); // the old latest is now `shift` back
            }
            m_latest = seq;
        } else {
            const uint16_t diff = static_cast<uint16_t>(m_latest - seq);
            if (diff >= 1 && diff <= 32) {
                m_bits |= (1u << (diff - 1));
            }
        }
    }

    bool hasAck() const { return m_any; }
    uint16_t ack() const { return m_latest; }
    uint32_t ackBits() const { return m_bits; }

    bool wasReceived(uint16_t seq) const {
        if (!m_any) {
            return false;
        }
        if (seq == m_latest) {
            return true;
        }
        const uint16_t diff = static_cast<uint16_t>(m_latest - seq);
        return diff >= 1 && diff <= 32 && (m_bits & (1u << (diff - 1))) != 0u;
    }

  private:
    bool m_any = false;
    uint16_t m_latest = 0;
    uint32_t m_bits = 0;
};

// Send side: allocates outgoing sequence numbers and, given a peer's (ack, ackBits), reports which
// of its still-in-flight sequences are now acknowledged (so the caller can stop resending them and
// sample RTT).
class AckSender {
  public:
    // Allocate the next sequence number and mark it in-flight (awaiting ack).
    uint16_t next() {
        const uint16_t s = m_next++;
        m_inFlight.insert(s);
        return s;
    }

    // Resolve a peer's ack header. Returns the sequences newly acknowledged this call.
    std::vector<uint16_t> onAck(uint16_t ack, uint32_t ackBits) {
        std::vector<uint16_t> newly;
        auto tryAck = [&](uint16_t s) {
            auto it = m_inFlight.find(s);
            if (it != m_inFlight.end()) {
                m_inFlight.erase(it);
                newly.push_back(s);
            }
        };
        tryAck(ack);
        for (int i = 0; i < 32; ++i) {
            if ((ackBits & (1u << i)) != 0u) {
                tryAck(static_cast<uint16_t>(ack - 1 - i));
            }
        }
        return newly;
    }

    bool inFlight(uint16_t seq) const { return m_inFlight.count(seq) != 0; }
    size_t inFlightCount() const { return m_inFlight.size(); }

  private:
    uint16_t m_next = 0;
    std::unordered_set<uint16_t> m_inFlight;
};

} // namespace maz::net
