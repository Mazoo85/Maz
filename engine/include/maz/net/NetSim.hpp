#pragma once

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

// maz::net network-condition simulator — the deterministic "bad network" harness that lets the ack,
// interpolation, and prediction layers be validated without a real (flaky) network. You submit
// packets via send(now); the sim holds each until its delivery time (now + latency ± jitter) and
// drops or duplicates it per the configured probabilities; receive(now) returns everything due by
// then, in delivery-time order. Randomness comes from a seeded xorshift PRNG so a test replays
// identically, and time is supplied by the caller (no clock here). This is the same tool Godot and
// Gaffer-style netcode use to prove the layers above cope with loss, reordering, and jitter. Pure,
// std-only, unit-tested. All times share whatever unit the caller uses for `now` (e.g. seconds).
namespace maz::net {

struct NetConditions {
    double latency = 0.0;   // base one-way delay added to every packet
    double jitter = 0.0;    // uniform ± this much added to latency (can reorder packets)
    float lossChance = 0.0f; // [0,1] probability a sent packet is dropped outright
    float dupChance = 0.0f;  // [0,1] probability a delivered packet is also duplicated
};

class NetSim {
  public:
    explicit NetSim(uint64_t seed = 0x9E3779B97F4A7C15ull) : m_state(seed != 0 ? seed : 1u) {}

    void setConditions(const NetConditions& c) { m_cond = c; }
    const NetConditions& conditions() const { return m_cond; }

    // Submit a packet sent at time `now`. Returns false if loss dropped it (nothing scheduled);
    // otherwise it's queued for delivery at now + latency ± jitter, and possibly duplicated.
    bool send(const std::vector<uint8_t>& packet, double now) {
        if (rollChance(m_cond.lossChance)) {
            ++m_dropped;
            return false;
        }
        schedule(packet, now);
        if (rollChance(m_cond.dupChance)) {
            schedule(packet, now);
            ++m_duplicated;
        }
        return true;
    }

    // Return (and remove) every packet whose delivery time is <= now, earliest-delivery first.
    std::vector<std::vector<uint8_t>> receive(double now) {
        std::vector<std::vector<uint8_t>> out;
        while (!m_queue.empty() && m_queue.front().deliveryTime <= now) {
            out.push_back(std::move(m_queue.front().data));
            m_queue.pop_front();
        }
        return out;
    }

    std::size_t pending() const { return m_queue.size(); }
    uint32_t dropped() const { return m_dropped; }
    uint32_t duplicated() const { return m_duplicated; }
    void clear() { m_queue.clear(); }

  private:
    struct Item {
        double deliveryTime;
        std::vector<uint8_t> data;
    };

    // xorshift64* -> [0,1) with 53 bits of mantissa. Deterministic for a given seed.
    double nextUnit() {
        uint64_t x = m_state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        m_state = x;
        const uint64_t r = x * 0x2545F4914F6CDD1Dull;
        return static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0);
    }

    bool rollChance(float p) {
        if (p <= 0.0f) {
            return false;
        }
        if (p >= 1.0f) {
            return true;
        }
        return nextUnit() < static_cast<double>(p);
    }

    void schedule(const std::vector<uint8_t>& packet, double now) {
        double delay = m_cond.latency;
        if (m_cond.jitter > 0.0) {
            delay += (nextUnit() * 2.0 - 1.0) * m_cond.jitter;
        }
        if (delay < 0.0) {
            delay = 0.0;
        }
        insertSorted(now + delay, packet);
    }

    // Keep the queue sorted by delivery time; equal times preserve insertion order (stable).
    void insertSorted(double t, const std::vector<uint8_t>& data) {
        std::size_t i = m_queue.size();
        while (i > 0 && m_queue[i - 1].deliveryTime > t) {
            --i;
        }
        m_queue.insert(m_queue.begin() + static_cast<std::ptrdiff_t>(i), Item{t, data});
    }

    NetConditions m_cond;
    std::deque<Item> m_queue;
    uint64_t m_state;
    uint32_t m_dropped = 0;
    uint32_t m_duplicated = 0;
};

} // namespace maz::net
