#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// maz::io::StreamPeerBuffer — Godot's StreamPeerBuffer: a growable byte buffer with a read cursor and
// explicit, fixed-WIDTH, endian-aware put/get for u8/16/32/64 (signed + unsigned), float, double, and
// length-prefixed strings. Unlike io::Serialize's ByteWriter/ByteReader (host byte order, template
// write<T>), this pins the on-the-wire layout, which is exactly what network protocols and portable
// file formats need — a big-endian sender and a little-endian receiver agree byte-for-byte. Reads
// past the end are safe: they return 0 / empty and leave the cursor put. Header-only, pure, C++20.
namespace maz::io {

class StreamPeerBuffer {
  public:
    void setBigEndian(bool big) { m_bigEndian = big; }
    bool isBigEndian() const { return m_bigEndian; }

    void clear() {
        m_data.clear();
        m_pos = 0;
    }
    void seek(std::size_t pos) { m_pos = pos <= m_data.size() ? pos : m_data.size(); }
    std::size_t getPosition() const { return m_pos; }
    std::size_t size() const { return m_data.size(); }
    std::size_t getAvailableBytes() const { return m_pos < m_data.size() ? m_data.size() - m_pos : 0; }
    const std::vector<std::uint8_t>& dataArray() const { return m_data; }
    std::vector<std::uint8_t>& dataArray() { return m_data; }

    // ---- put (append at the end) ----
    void putU8(std::uint8_t v) { m_data.push_back(v); }
    void putU16(std::uint16_t v) { putUint(v, 2); }
    void putU32(std::uint32_t v) { putUint(v, 4); }
    void putU64(std::uint64_t v) { putUint(v, 8); }
    void put8(std::int8_t v) { putU8(static_cast<std::uint8_t>(v)); }
    void put16(std::int16_t v) { putUint(static_cast<std::uint16_t>(v), 2); }
    void put32(std::int32_t v) { putUint(static_cast<std::uint32_t>(v), 4); }
    void put64(std::int64_t v) { putUint(static_cast<std::uint64_t>(v), 8); }
    void putFloat(float v) { putUint(std::bit_cast<std::uint32_t>(v), 4); }
    void putDouble(double v) { putUint(std::bit_cast<std::uint64_t>(v), 8); }
    void putBytes(const std::vector<std::uint8_t>& b) {
        m_data.insert(m_data.end(), b.begin(), b.end());
    }
    // 32-bit length prefix (endian-aware) followed by the raw UTF-8 bytes — Godot's put_string.
    void putString(const std::string& s) {
        putU32(static_cast<std::uint32_t>(s.size()));
        m_data.insert(m_data.end(), s.begin(), s.end());
    }

    // ---- get (advance the read cursor) ----
    std::uint8_t getU8() {
        if (m_pos >= m_data.size()) {
            return 0;
        }
        return m_data[m_pos++];
    }
    std::uint16_t getU16() { return static_cast<std::uint16_t>(getUint(2)); }
    std::uint32_t getU32() { return static_cast<std::uint32_t>(getUint(4)); }
    std::uint64_t getU64() { return getUint(8); }
    std::int8_t get8() { return static_cast<std::int8_t>(getU8()); }
    std::int16_t get16() { return static_cast<std::int16_t>(getUint(2)); }
    std::int32_t get32() { return static_cast<std::int32_t>(getUint(4)); }
    std::int64_t get64() { return static_cast<std::int64_t>(getUint(8)); }
    float getFloat() { return std::bit_cast<float>(static_cast<std::uint32_t>(getUint(4))); }
    double getDouble() { return std::bit_cast<double>(getUint(8)); }
    std::string getString() {
        const std::uint32_t len = getU32();
        std::string out;
        for (std::uint32_t i = 0; i < len; ++i) {
            if (m_pos >= m_data.size()) {
                break;
            }
            out.push_back(static_cast<char>(m_data[m_pos++]));
        }
        return out;
    }

  private:
    void putUint(std::uint64_t v, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            // Little-endian emits byte i from the low end; big-endian from the high end.
            const int shift = m_bigEndian ? (bytes - 1 - i) * 8 : i * 8;
            m_data.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
        }
    }
    std::uint64_t getUint(int bytes) {
        if (m_pos + static_cast<std::size_t>(bytes) > m_data.size()) {
            return 0; // not enough bytes: leave the cursor put, return 0 (Godot returns 0 on error)
        }
        std::uint64_t v = 0;
        for (int i = 0; i < bytes; ++i) {
            const int shift = m_bigEndian ? (bytes - 1 - i) * 8 : i * 8;
            v |= static_cast<std::uint64_t>(m_data[m_pos++]) << shift;
        }
        return v;
    }

    std::vector<std::uint8_t> m_data;
    std::size_t m_pos = 0;
    bool m_bigEndian = false;
};

} // namespace maz::io
