#pragma once

#include <cstddef>
#include <vector>

namespace maz::core {

// RingBuffer<T> — a fixed-capacity circular buffer, the workhorse behind rolling histories (frame-time /
// FPS graphs, moving averages), input buffers (a fighting game's last-N button presses, jump "coyote"
// windows), replay traces, and streaming audio/network queues. It serves two idioms from one structure:
//   * a ROLLING WINDOW — `push` always succeeds; once full it overwrites the OLDEST element, so the buffer
//     always holds the most recent `capacity` items (the frame-time graph pattern).
//   * a bounded FIFO QUEUE — `pushBack` reports whether it accepted (rejecting when full) and `popFront`
//     drains oldest-first.
// Indexing is logical: `at(0)` is always the oldest live element, `at(size()-1)` the newest, regardless of
// where they physically wrap. Godot keeps a `RingBuffer` for exactly these jobs. Pure container,
// header-only, deterministic — it unit-tests exactly and drives a golden (a scrolling history plot).

template <typename T>
class RingBuffer {
public:
    RingBuffer() = default;
    explicit RingBuffer(std::size_t capacity) { reset(capacity); }

    // (Re)allocate to `capacity` slots and empty the buffer. A zero capacity makes every push a no-op.
    void reset(std::size_t capacity) {
        m_data.assign(capacity, T{});
        m_head = 0;
        m_count = 0;
    }

    void clear() {
        m_head = 0;
        m_count = 0;
    }

    std::size_t capacity() const { return m_data.size(); }
    std::size_t size() const { return m_count; }
    bool empty() const { return m_count == 0; }
    bool full() const { return m_count == m_data.size(); }

    // Rolling-window push: overwrites the oldest element when full. Returns true if an element was evicted.
    bool push(const T& value) {
        if (m_data.empty()) {
            return false;
        }
        if (m_count < m_data.size()) {
            m_data[physical(m_count)] = value;
            ++m_count;
            return false;
        }
        // Full: overwrite the oldest, advance the head.
        m_data[m_head] = value;
        m_head = advance(m_head);
        return true;
    }

    // Bounded-FIFO push: appends only if there is room. Returns false (and does nothing) when full.
    bool pushBack(const T& value) {
        if (m_count >= m_data.size()) {
            return false;
        }
        m_data[physical(m_count)] = value;
        ++m_count;
        return true;
    }

    // Remove and return the oldest element into `out`. Returns false when empty.
    bool popFront(T& out) {
        if (m_count == 0) {
            return false;
        }
        out = m_data[m_head];
        m_head = advance(m_head);
        --m_count;
        return true;
    }

    // Logical access: index 0 = oldest, size()-1 = newest. Out-of-range indices are clamped to a valid slot
    // (callers should stay within [0,size()); this only guards against UB).
    const T& at(std::size_t logical) const {
        const std::size_t i = logical < m_count ? logical : (m_count ? m_count - 1 : 0);
        return m_data[physical(i)];
    }
    T& at(std::size_t logical) {
        const std::size_t i = logical < m_count ? logical : (m_count ? m_count - 1 : 0);
        return m_data[physical(i)];
    }
    const T& operator[](std::size_t logical) const { return at(logical); }
    T& operator[](std::size_t logical) { return at(logical); }

    const T& front() const { return m_data[m_head]; }                 // oldest
    const T& back() const { return m_data[physical(m_count - 1)]; }   // newest

    // Snapshot oldest -> newest into a flat vector (handy for plotting / tests / serialization).
    std::vector<T> toVector() const {
        std::vector<T> out;
        out.reserve(m_count);
        for (std::size_t i = 0; i < m_count; ++i) {
            out.push_back(m_data[physical(i)]);
        }
        return out;
    }

private:
    std::size_t advance(std::size_t i) const { return (i + 1) % m_data.size(); }
    std::size_t physical(std::size_t logical) const { return (m_head + logical) % m_data.size(); }

    std::vector<T> m_data;
    std::size_t m_head = 0;  // physical index of the oldest element
    std::size_t m_count = 0; // number of live elements
};

} // namespace maz::core
