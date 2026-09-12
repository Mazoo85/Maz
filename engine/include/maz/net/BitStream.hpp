#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

// maz::net bit stream — compact bit-level packet (de)serialization, the foundation of the
// networking layer. Games send state constantly, so every bit counts: a flag is 1 bit (not a byte),
// a value known to be 0..1000 is 10 bits (not 32), a normalized float can be quantized to 16.
// BitWriter packs values LSB-first into a byte buffer; BitReader unpacks them in the same order and
// reports underflow instead of reading past the end. This is the wire format snapshot/delta
// replication and RPC ride on — and a bandwidth win over Godot's byte-granular Variant encoding.
// Pure, std-only, deterministic, unit-tested. No sockets here (that's the transport layer).
namespace maz::net {

class BitWriter {
  public:
    void writeBit(bool b) {
        const size_t byte = m_bitPos >> 3;
        const int off = static_cast<int>(m_bitPos & 7u);
        if (byte >= m_bytes.size()) {
            m_bytes.push_back(0);
        }
        if (b) {
            m_bytes[byte] |= static_cast<uint8_t>(1u << off);
        }
        ++m_bitPos;
    }

    // Write the low `bits` (1..32) of `value`, LSB first.
    void writeBits(uint32_t value, int bits) {
        if (bits < 1) {
            return;
        }
        if (bits > 32) {
            bits = 32;
        }
        for (int i = 0; i < bits; ++i) {
            writeBit(((value >> i) & 1u) != 0u);
        }
    }

    void writeBool(bool b) { writeBit(b); }
    void writeUint(uint32_t v, int bits) { writeBits(v, bits); }

    // Signed value in `bits` (two's-complement low bits). Reader must sign-extend with the same bits.
    void writeInt(int32_t v, int bits) { writeBits(static_cast<uint32_t>(v), bits); }

    // Full-precision 32-bit float (bit-exact round-trip).
    void writeFloat(float f) {
        uint32_t u = 0;
        std::memcpy(&u, &f, sizeof(u));
        writeBits(u, 32);
    }

    void writeBytes(const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            writeBits(data[i], 8);
        }
    }

    // Pad with zero bits up to the next byte boundary.
    void align() {
        while ((m_bitPos & 7u) != 0u) {
            writeBit(false);
        }
    }

    const std::vector<uint8_t>& bytes() const { return m_bytes; }
    size_t bitCount() const { return m_bitPos; }
    size_t byteCount() const { return m_bytes.size(); }

  private:
    std::vector<uint8_t> m_bytes;
    size_t m_bitPos = 0;
};

class BitReader {
  public:
    BitReader(const uint8_t* data, size_t sizeBytes)
        : m_data(data), m_totalBits(sizeBytes * 8) {}
    explicit BitReader(const std::vector<uint8_t>& v) : m_data(v.data()), m_totalBits(v.size() * 8) {}

    bool readBit() {
        if (m_bitPos >= m_totalBits) {
            m_ok = false;
            return false;
        }
        const size_t byte = m_bitPos >> 3;
        const int off = static_cast<int>(m_bitPos & 7u);
        ++m_bitPos;
        return ((m_data[byte] >> off) & 1u) != 0u;
    }

    uint32_t readBits(int bits) {
        if (bits < 1) {
            return 0;
        }
        if (bits > 32) {
            bits = 32;
        }
        uint32_t v = 0;
        for (int i = 0; i < bits; ++i) {
            if (readBit()) {
                v |= (1u << i);
            }
        }
        return v;
    }

    bool readBool() { return readBit(); }
    uint32_t readUint(int bits) { return readBits(bits); }

    int32_t readInt(int bits) {
        uint32_t u = readBits(bits);
        // Sign-extend from the top bit of the `bits`-wide field.
        if (bits < 32 && (u & (1u << (bits - 1))) != 0u) {
            u |= ~((1u << bits) - 1u);
        }
        return static_cast<int32_t>(u);
    }

    float readFloat() {
        const uint32_t u = readBits(32);
        float f = 0.0f;
        std::memcpy(&f, &u, sizeof(f));
        return f;
    }

    void readBytes(uint8_t* out, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            out[i] = static_cast<uint8_t>(readBits(8));
        }
    }

    // False once any read has gone past the end of the buffer (the read returned zero-fill).
    bool ok() const { return m_ok; }
    size_t bitPos() const { return m_bitPos; }
    size_t bitsRemaining() const { return m_bitPos <= m_totalBits ? m_totalBits - m_bitPos : 0; }

  private:
    const uint8_t* m_data;
    size_t m_totalBits;
    size_t m_bitPos = 0;
    bool m_ok = true;
};

} // namespace maz::net
