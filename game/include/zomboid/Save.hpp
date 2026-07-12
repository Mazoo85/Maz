// Tiny binary (de)serialization primitives for save games.
//
// Writer appends POD/strings to a byte buffer; Reader consumes them with bounds
// checking so a truncated or corrupt save fails gracefully (ok() == false) rather
// than crashing. Floats are copied bit-exact so a restored game continues
// identically to one that was never saved. Little-endian is assumed (all targets).
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace zb {

class Writer {
public:
    explicit Writer(std::vector<uint8_t>& out) : m_out(out) {}

    void u8(uint8_t v) { m_out.push_back(v); }
    void u32(uint32_t v) { raw(&v, sizeof(v)); }
    void u64(uint64_t v) { raw(&v, sizeof(v)); }
    void i32(int32_t v) { raw(&v, sizeof(v)); }
    void f32(float v) { raw(&v, sizeof(v)); }
    void boolean(bool v) { u8(v ? 1u : 0u); }

    void str(const std::string& s) {
        u32(static_cast<uint32_t>(s.size()));
        if (!s.empty()) raw(s.data(), s.size());
    }

private:
    void raw(const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        m_out.insert(m_out.end(), b, b + n);
    }
    std::vector<uint8_t>& m_out;
};

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : m_p(data), m_end(data + size) {}
    explicit Reader(const std::vector<uint8_t>& v) : Reader(v.data(), v.size()) {}

    bool ok() const { return m_ok; }

    uint8_t u8() {
        uint8_t v = 0;
        raw(&v, sizeof(v));
        return v;
    }
    uint32_t u32() {
        uint32_t v = 0;
        raw(&v, sizeof(v));
        return v;
    }
    uint64_t u64() {
        uint64_t v = 0;
        raw(&v, sizeof(v));
        return v;
    }
    int32_t i32() {
        int32_t v = 0;
        raw(&v, sizeof(v));
        return v;
    }
    float f32() {
        float v = 0.0f;
        raw(&v, sizeof(v));
        return v;
    }
    bool boolean() { return u8() != 0; }

    std::string str() {
        const uint32_t n = u32();
        if (!m_ok) return {};
        if (static_cast<size_t>(m_end - m_p) < n) {
            m_ok = false;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(m_p), n);
        m_p += n;
        return s;
    }

private:
    void raw(void* dst, size_t n) {
        if (!m_ok || static_cast<size_t>(m_end - m_p) < n) {
            m_ok = false;
            return;
        }
        std::memcpy(dst, m_p, n);
        m_p += n;
    }

    const uint8_t* m_p;
    const uint8_t* m_end;
    bool m_ok = true;
};

} // namespace zb
