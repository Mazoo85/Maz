#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>  // std::move

namespace maz::core {

// A fixed-capacity FIFO ring buffer. The write position is DERIVED as
// (head + count) % capacity — there is no separate tail index. A live count
// (rather than a tail index) is what makes empty (count == 0) vs full
// (count == capacity) unambiguous: a bare head/tail pair cannot tell the two
// apart when the indices coincide. T is stored by value and assumed
// default-constructible (like Pool<T>). NOT thread-safe.
//
// Capacity 0 is a valid, permanently-empty buffer: push always returns false,
// pop always returns false, front() is always nullptr, and full() and empty()
// are both true. The emptiness/fullness guards run before every modulo, so a
// zero divisor is never reached.
template <class T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity) : m_buffer(capacity), m_capacity(capacity) {}

    // Copy a value into the write slot; false if full (no space).
    bool push(const T& value) {
        if (m_count == m_capacity) {
            return false;  // full — also true when capacity == 0, so the modulo
                           // below is never reached with a zero divisor
                           // (guard order is load-bearing)
        }
        m_buffer[(m_head + m_count) % m_capacity] = value;
        ++m_count;
        return true;
    }

    // Move a value into the write slot; false if full (no space).
    bool push(T&& value) {
        if (m_count == m_capacity) {
            return false;  // full — see the copy overload for the zero-divisor note
        }
        m_buffer[(m_head + m_count) % m_capacity] = std::move(value);
        ++m_count;
        return true;
    }

    // Move the oldest element into out and advance head; false if empty.
    bool pop(T& out) {
        if (m_count == 0) {
            return false;
        }
        out = std::move(m_buffer[m_head]);
        m_head = (m_head + 1) % m_capacity;
        --m_count;
        return true;
    }

    // Pointer to the oldest element, or nullptr when empty. Returns a pointer
    // into the fixed backing vector; UNLIKE Pool<T>::get() this buffer never
    // grows/reallocates, so the address stays valid for the buffer's lifetime —
    // but the slot's LOGICAL ownership rotates: after a pop() the T at that
    // address is no longer the front, and a later push() may overwrite it, so
    // re-fetch front() after any pop().
    T* front() { return m_count == 0 ? nullptr : &m_buffer[m_head]; }
    const T* front() const { return m_count == 0 ? nullptr : &m_buffer[m_head]; }

    bool empty() const { return m_count == 0; }
    bool full() const { return m_count == m_capacity; }
    std::size_t size() const { return m_count; }
    std::size_t capacity() const { return m_capacity; }

    // Resets head + count only; does NOT overwrite stale slot contents (a
    // popped/cleared slot's old T lingers until the next push overwrites it or
    // the buffer is destroyed). Push always overwrites a slot before any read
    // can observe it (all reads gate on m_count/m_head), so this is safe but
    // not resource-releasing — unlike Pool::destroy which assigns T{} to free
    // payloads.
    void clear() {
        m_head = 0;
        m_count = 0;
    }

private:
    std::vector<T> m_buffer;
    std::size_t m_head = 0;
    std::size_t m_count = 0;
    std::size_t m_capacity;
};

} // namespace maz::core
