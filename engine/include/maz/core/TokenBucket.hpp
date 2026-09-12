#pragma once

#include <limits>

// maz::core::TokenBucket — the classic token-bucket rate limiter: a bucket holds up to `capacity` tokens,
// refills at a steady `refillPerSecond`, and an action spends tokens; if the bucket is dry the action is
// refused. This is the standard way to allow a controlled BURST (spend the whole bucket at once) while
// capping the sustained rate over time. Distinct from game::CooldownManager (a per-ability binary "ready or
// not" timer with no burst): a token bucket accumulates several charges and refills fractionally, so it
// models "3 dashes, one back every 2 seconds", chat/emote spam limits, outgoing packet or RPC throttling in
// netcode, and spawn/particle-emission budgets. Advance it by the frame delta each tick, then tryConsume().
// Deterministic, header-only, std-only — no clock inside, so tests drive it with explicit dt. The leaky
// bucket's cousin; Godot ships neither.
namespace maz::core {

class TokenBucket {
public:
    // capacity: max tokens the bucket can hold (burst size). refillPerSecond: steady refill rate.
    // initial: starting tokens (defaults to full); all clamped to sane ranges.
    TokenBucket(double capacity, double refillPerSecond, double initial)
        : m_capacity(capacity < 0.0 ? 0.0 : capacity),
          m_refill(refillPerSecond < 0.0 ? 0.0 : refillPerSecond),
          m_tokens(clamp(initial)) {}

    TokenBucket(double capacity, double refillPerSecond)
        : TokenBucket(capacity, refillPerSecond, capacity) {}

    // Add refillPerSecond * dt tokens (clamped to capacity). Call once per frame with the frame delta.
    void advance(double dt) {
        if (dt <= 0.0) {
            return;
        }
        m_tokens += m_refill * dt;
        if (m_tokens > m_capacity) {
            m_tokens = m_capacity;
        }
    }

    // Spend `n` tokens if at least `n` are available; returns true and deducts them, else false unchanged.
    // n <= 0 always succeeds without changing the bucket; n > capacity can never succeed.
    bool tryConsume(double n = 1.0) {
        if (n <= 0.0) {
            return true;
        }
        if (n > m_tokens) {
            return false;
        }
        m_tokens -= n;
        return true;
    }

    // Seconds until `n` tokens are available (0 if already, +inf if unreachable: n > capacity, or rate 0).
    double timeUntil(double n) const {
        if (n <= m_tokens) {
            return 0.0;
        }
        if (n > m_capacity || m_refill <= 0.0) {
            return std::numeric_limits<double>::infinity();
        }
        return (n - m_tokens) / m_refill;
    }

    double tokens() const { return m_tokens; }
    double capacity() const { return m_capacity; }
    double refillRate() const { return m_refill; }
    bool full() const { return m_tokens >= m_capacity; }
    bool empty() const { return m_tokens <= 0.0; }

    void refillToFull() { m_tokens = m_capacity; }
    void drain() { m_tokens = 0.0; }
    void setTokens(double t) { m_tokens = clamp(t); }
    void setRefillRate(double r) { m_refill = r < 0.0 ? 0.0 : r; }

private:
    double clamp(double t) const { return t < 0.0 ? 0.0 : (t > m_capacity ? m_capacity : t); }

    double m_capacity;
    double m_refill;
    double m_tokens;
};

} // namespace maz::core
