#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <string_view>
#include <bit>
#include <cstring>  // std::memcpy for readBytes

#include "maz/core/StringId.hpp"

namespace maz::core {

// A little-endian, length-prefixed PRIVATE engine binary format — NOT
// self-describing and NOT versioned yet (versioned schemas are a future
// refinement). The on-disk layout is a FIXED little-endian byte order regardless
// of host endianness: multibyte scalars are written/read with explicit byte
// shifts (no memcpy of multibyte scalars, no reinterpret_cast of scalars), so a
// buffer produced on one machine decodes identically on another. ByteReader is a
// NON-OWNING view — the backing bytes must outlive the reader. Neither type is
// thread-safe. The reader FAILS-SAFE: it never throws, never reads out of
// bounds, and never triggers a huge allocation on a corrupt length — a
// truncated/corrupt buffer sets failed() and yields defaults, and failure is
// STICKY (once failed, every subsequent read fails). float/double assume an
// IEEE-754 host (sizes asserted below; the bit representation is what round-trips).

static_assert(sizeof(float) == sizeof(std::uint32_t), "bit_cast<u32>(float) requires 4-byte float");
static_assert(sizeof(double) == sizeof(std::uint64_t), "bit_cast<u64>(double) requires 8-byte double");

namespace detail {

// Append U as sizeof(U) little-endian bytes. The mask keeps the promoted int in
// range; the outer static_cast<uint8_t> silences -Wconversion on the narrowing store.
template <class U> inline void putLE(std::vector<std::uint8_t>& buf, U v) {
    for (std::size_t i = 0; i < sizeof(U); ++i) {
        buf.push_back(static_cast<std::uint8_t>((v >> (8u * i)) & 0xFFu));
    }
}

// Reconstruct U from sizeof(U) little-endian bytes at p. Each byte is widened to U
// BEFORE the shift — shifting a uint8_t left by 8*i would promote only to int and
// TRUNCATE the high bytes of a u64. Outer static_cast<U> narrows the promoted OR
// result for -Wconversion.
template <class U> inline U getLE(const std::uint8_t* p) {
    U result = 0;
    for (std::size_t i = 0; i < sizeof(U); ++i) {
        result = static_cast<U>(result | (static_cast<U>(p[i]) << (8u * i)));
    }
    return result;
}

} // namespace detail

class ByteWriter {
public:
    void writeU8(std::uint8_t value)   { detail::putLE(m_buf, value); }
    void writeU16(std::uint16_t value) { detail::putLE(m_buf, value); }
    void writeU32(std::uint32_t value) { detail::putLE(m_buf, value); }
    void writeU64(std::uint64_t value) { detail::putLE(m_buf, value); }
    void writeI8(std::int8_t value)    { writeU8(static_cast<std::uint8_t>(value)); }
    void writeI16(std::int16_t value)  { writeU16(static_cast<std::uint16_t>(value)); }
    void writeI32(std::int32_t value)  { writeU32(static_cast<std::uint32_t>(value)); }
    void writeI64(std::int64_t value)  { writeU64(static_cast<std::uint64_t>(value)); }
    void writeBool(bool value)   { writeU8(value ? std::uint8_t{1} : std::uint8_t{0}); }
    void writeFloat(float value) { writeU32(std::bit_cast<std::uint32_t>(value)); }
    void writeDouble(double value) { writeU64(std::bit_cast<std::uint64_t>(value)); }
    void writeBytes(const void* data, std::size_t len) {
        writeU64(static_cast<std::uint64_t>(len));
        const auto* p = static_cast<const std::uint8_t*>(data);
        m_buf.insert(m_buf.end(), p, p + len);   // len==0 with data==nullptr is a safe no-op
    }
    void writeString(std::string_view s) {
        writeU64(static_cast<std::uint64_t>(s.size()));
        const auto* p = reinterpret_cast<const std::uint8_t*>(s.data());  // char*->byte* aliasing is legal
        m_buf.insert(m_buf.end(), p, p + s.size());
    }
    void writeStringId(StringId id) { writeU64(id.value); }  // compose iter7
    const std::uint8_t* data() const { return m_buf.data(); }
    std::size_t size() const { return m_buf.size(); }
    const std::vector<std::uint8_t>& buffer() const { return m_buf; }
    void clear() { m_buf.clear(); }
    void reserve(std::size_t n) { m_buf.reserve(n); }

private:
    std::vector<std::uint8_t> m_buf;
};

class ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}
    explicit ByteReader(const std::vector<std::uint8_t>& v) : ByteReader(v.data(), v.size()) {}
    bool readU8(std::uint8_t& out)   { return readScalar(out); }
    bool readU16(std::uint16_t& out) { return readScalar(out); }
    bool readU32(std::uint32_t& out) { return readScalar(out); }
    bool readU64(std::uint64_t& out) { return readScalar(out); }
    bool readI8(std::int8_t& out)   { std::uint8_t u; if (!readU8(u)) return false; out = static_cast<std::int8_t>(u); return true; }
    bool readI16(std::int16_t& out) { std::uint16_t u; if (!readU16(u)) return false; out = static_cast<std::int16_t>(u); return true; }
    bool readI32(std::int32_t& out) { std::uint32_t u; if (!readU32(u)) return false; out = static_cast<std::int32_t>(u); return true; }
    bool readI64(std::int64_t& out) { std::uint64_t u; if (!readU64(u)) return false; out = static_cast<std::int64_t>(u); return true; }
    bool readBool(bool& out)   { std::uint8_t u; if (!readU8(u)) return false; out = (u != 0); return true; }
    bool readFloat(float& out) { std::uint32_t u; if (!readU32(u)) return false; out = std::bit_cast<float>(u); return true; }
    bool readDouble(double& out) { std::uint64_t u; if (!readU64(u)) return false; out = std::bit_cast<double>(u); return true; }
    bool readString(std::string& out) {
        std::uint64_t len = 0;
        if (!readU64(len)) return false;  // truncated length prefix
        // OVERFLOW-SAFE gate: compare the u64 length against bytes remaining BEFORE
        // any allocation. A corrupt 0xFFFF... length fails here instead of wrapping
        // or triggering a huge alloc.
        if (len > static_cast<std::uint64_t>(remaining())) { m_failed = true; return false; }
        const std::size_t n = static_cast<std::size_t>(len);  // safe: len <= remaining() <= SIZE_MAX
        out.assign(reinterpret_cast<const char*>(m_data + m_pos), n);
        m_pos += n;
        return true;
    }
    bool readBytes(void* dst, std::size_t len) {
        if (!ensure(len)) return false;
        std::memcpy(dst, m_data + m_pos, len);
        m_pos += len;
        return true;
    }
    bool readStringId(StringId& out) { std::uint64_t v; if (!readU64(v)) return false; out = StringId(v); return true; }
    bool ok() const { return !m_failed; }
    bool failed() const { return m_failed; }
    std::size_t position() const { return m_pos; }
    std::size_t remaining() const { return m_size - m_pos; }  // invariant m_pos <= m_size keeps this safe
    bool skip(std::size_t n) { if (!ensure(n)) return false; m_pos += n; return true; }

private:
    // THE key safety line. m_pos <= m_size is an invariant, so (m_size - m_pos)
    // never underflows; writing the check as (n > m_size - m_pos) avoids the
    // (m_pos + n) overflow a corrupt length could cause.
    bool ensure(std::size_t n) {
        if (m_failed) return false;  // sticky
        if (n > m_size - m_pos) { m_failed = true; return false; }
        return true;
    }
    template <class U> bool readScalar(U& out) {
        if (!ensure(sizeof(U))) return false;
        out = detail::getLE<U>(m_data + m_pos);
        m_pos += sizeof(U);
        return true;
    }
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_pos = 0;
    bool m_failed = false;
};

} // namespace maz::core
