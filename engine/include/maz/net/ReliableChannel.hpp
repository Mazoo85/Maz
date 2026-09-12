// maz/net/ReliableChannel.hpp — reliable, in-order message delivery over the real UDP transport.
//
// This is the piece the roadmap called out as the networking residual: wiring the engine's existing
// reliability layer (net::AckSender / net::AckReceiver — Glenn Fiedler's ack model, the same one
// ENet/Godot use) onto the real net::UdpSocket transport so that whole *messages* arrive reliably
// and in order even when the underlying link drops packets.
//
// How it works:
//   * Each reliable message gets a monotonically increasing 32-bit msgId (delivery order).
//   * Every packet carries a 16-bit packet sequence (AckSender), plus the peer-ack header
//     (ack + 32-bit ackBits from AckReceiver) so a single returning packet acknowledges up to 33.
//   * The sender keeps unconfirmed messages and retransmits them (with a fresh packet sequence) on
//     a timer until the peer's acks confirm the packet that carried them.
//   * The receiver delivers messages strictly in msgId order, buffering out-of-order arrivals and
//     discarding duplicates, and piggybacks/sends acks back.
//
// Verified by tests/net/reliable.cpp over a real loopback socket pair with a 40%-packet-loss link:
// all messages still arrive exactly once, in order. Wire fields are little-endian (endian-portable);
// an optional dropSend hook lets tests model an unreliable link.

#ifndef MAZ_NET_RELIABLECHANNEL_HPP
#define MAZ_NET_RELIABLECHANNEL_HPP

#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

#include "maz/net/Reliability.hpp"
#include "maz/net/UdpSocket.hpp"

namespace maz {
namespace net {

class ReliableChannel {
public:
    // sock must outlive the channel; peer is where reliable messages are sent.
    ReliableChannel(UdpSocket* sock, Endpoint peer) : m_sock(sock), m_peer(std::move(peer)) {}

    // Optional link-quality hook: if set and it returns true for a given send, the datagram is
    // dropped instead of transmitted (used by tests to model packet loss). Production leaves it null.
    std::function<bool()> dropSend;

    // Queue a message for reliable, in-order delivery. Copied; transmitted on the next update().
    void sendReliable(const void* data, std::size_t len) {
        Pending p;
        p.msgId = m_nextMsgId++;
        const auto* b = static_cast<const std::uint8_t*>(data);
        p.bytes.assign(b, b + len);
        m_pending.emplace(p.msgId, std::move(p));
    }

    // Pump the channel: drain the socket, resolve acks, deliver in-order messages, (re)transmit
    // anything unconfirmed, and return an ack to the peer. nowMs is a millisecond clock.
    void update(std::uint32_t nowMs) {
        drainSocket();

        bool sentData = false;
        // (Re)transmit every still-unconfirmed message whose retransmit timer has elapsed.
        for (auto& kv : m_pending) {
            Pending& p = kv.second;
            if (!p.everSent || (nowMs - p.lastSentMs) >= kRetransmitMs) {
                const std::uint16_t seq = m_ackTx.next();
                m_seqToMsg[seq] = p.msgId;
                sendPacket(kPacketData, seq, p.msgId, p.bytes.data(), p.bytes.size());
                p.everSent = true;
                p.lastSentMs = nowMs;
                sentData = true;
            }
        }
        // If we owe an acknowledgement and had no data packet to piggyback it on, send an ack-only.
        if (m_owesAck && !sentData) {
            const std::uint16_t seq = m_ackTx.next();
            sendPacket(kPacketAck, seq, 0, nullptr, 0);
        }
        m_owesAck = false;
    }

    // Pop the next in-order delivered message, if any. Returns false when none are ready.
    bool receive(std::vector<std::uint8_t>& out) {
        if (m_delivered.empty()) return false;
        out = std::move(m_delivered.front());
        m_delivered.pop_front();
        return true;
    }

    std::size_t unconfirmedCount() const { return m_pending.size(); }
    std::uint32_t nextDeliverId() const { return m_nextDeliver; }

private:
    static constexpr std::uint8_t kPacketAck = 0;
    static constexpr std::uint8_t kPacketData = 1;
    static constexpr std::uint32_t kRetransmitMs = 40;

    struct Pending {
        std::uint32_t msgId = 0;
        std::vector<std::uint8_t> bytes;
        std::uint32_t lastSentMs = 0;
        bool everSent = false;
    };

    // ---- little-endian wire helpers -------------------------------------------------------------
    static void putU16(std::vector<std::uint8_t>& b, std::uint16_t v) {
        b.push_back(static_cast<std::uint8_t>(v & 0xFFu));
        b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    }
    static void putU32(std::vector<std::uint8_t>& b, std::uint32_t v) {
        b.push_back(static_cast<std::uint8_t>(v & 0xFFu));
        b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
        b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
        b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
    }
    static std::uint16_t getU16(const std::uint8_t* p) {
        return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
    }
    static std::uint32_t getU32(const std::uint8_t* p) {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }

    void sendPacket(std::uint8_t type, std::uint16_t seq, std::uint32_t msgId,
                    const std::uint8_t* payload, std::size_t len) {
        std::vector<std::uint8_t> buf;
        buf.push_back(type);
        putU16(buf, seq);
        putU16(buf, m_ackRx.hasAck() ? m_ackRx.ack() : 0);
        putU32(buf, m_ackRx.hasAck() ? m_ackRx.ackBits() : 0);
        if (type == kPacketData) {
            putU32(buf, msgId);
            putU16(buf, static_cast<std::uint16_t>(len));
            buf.insert(buf.end(), payload, payload + len);
        }
        if (dropSend && dropSend()) return;  // model an unreliable link (tests only)
        m_sock->sendTo(buf.data(), buf.size(), m_peer);
    }

    void drainSocket() {
        std::uint8_t buf[2048];
        Endpoint from;
        for (;;) {
            long n = m_sock->recvFrom(buf, sizeof(buf), from);
            if (n < 9) break;  // no more datagrams (or a runt shorter than the header)
            const std::uint8_t type = buf[0];
            const std::uint16_t seq = getU16(buf + 1);
            const std::uint16_t ack = getU16(buf + 3);
            const std::uint32_t ackBits = getU32(buf + 5);

            m_ackRx.onReceived(seq);
            m_owesAck = true;

            // Confirm any of our in-flight packets the peer just acknowledged.
            for (std::uint16_t s : m_ackTx.onAck(ack, ackBits)) {
                auto it = m_seqToMsg.find(s);
                if (it != m_seqToMsg.end()) {
                    m_pending.erase(it->second);  // message confirmed delivered
                    m_seqToMsg.erase(it);
                }
            }

            if (type == kPacketData && n >= 15) {
                const std::uint32_t msgId = getU32(buf + 9);
                const std::uint16_t len = getU16(buf + 13);
                const std::size_t avail = static_cast<std::size_t>(n) - 15;
                const std::size_t take = len <= avail ? len : avail;
                std::vector<std::uint8_t> payload(buf + 15, buf + 15 + take);
                acceptMessage(msgId, std::move(payload));
            }
        }
    }

    void acceptMessage(std::uint32_t msgId, std::vector<std::uint8_t>&& payload) {
        if (msgId < m_nextDeliver) return;              // already delivered — duplicate
        if (m_recvBuf.count(msgId)) return;             // already buffered — duplicate
        m_recvBuf.emplace(msgId, std::move(payload));
        // Deliver every message that is now contiguous from the next expected id.
        auto it = m_recvBuf.find(m_nextDeliver);
        while (it != m_recvBuf.end()) {
            m_delivered.push_back(std::move(it->second));
            m_recvBuf.erase(it);
            ++m_nextDeliver;
            it = m_recvBuf.find(m_nextDeliver);
        }
    }

    UdpSocket* m_sock;
    Endpoint m_peer;
    AckSender m_ackTx;
    AckReceiver m_ackRx;

    std::unordered_map<std::uint32_t, Pending> m_pending;   // msgId -> unconfirmed message
    std::unordered_map<std::uint16_t, std::uint32_t> m_seqToMsg;  // packet seq -> msgId
    std::uint32_t m_nextMsgId = 0;

    std::map<std::uint32_t, std::vector<std::uint8_t>> m_recvBuf;  // out-of-order arrivals
    std::deque<std::vector<std::uint8_t>> m_delivered;            // ready, in order
    std::uint32_t m_nextDeliver = 0;
    bool m_owesAck = false;
};

}  // namespace net
}  // namespace maz

#endif  // MAZ_NET_RELIABLECHANNEL_HPP
