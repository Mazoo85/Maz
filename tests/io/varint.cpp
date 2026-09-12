// tests/io/varint.cpp — verifies LEB128 + zigzag varint encoding (io::appendVarint / readVarint / signed).
// Ground truths: small values take one byte and the length boundaries (127/128, 16383/16384) are exact;
// varintSize predicts the byte count; a wide sweep of unsigned values round-trips; UINT64_MAX round-trips in
// 10 bytes; zigzag folds signed values so small negatives are one byte and round-trip; several values pack
// into one buffer and decode back sequentially with the offset advancing correctly; truncated and over-long
// (overflow) streams are rejected without reading past the buffer. Checked against the LEB128 spec. Pure CPU.
#include "maz/io/Varint.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::io;

int main() {
    // --- 1. One-byte range and the length boundaries are exact. ---
    {
        CHECK(varintSize(0) == 1 && varintSize(127) == 1, "0..127 fit in one byte");
        CHECK(varintSize(128) == 2 && varintSize(16383) == 2, "128..16383 fit in two bytes");
        CHECK(varintSize(16384) == 3, "16384 needs three bytes");
        CHECK(varintSize(UINT64_MAX) == 10, "UINT64_MAX needs ten bytes");

        std::vector<std::uint8_t> b;
        appendVarint(b, 0);
        CHECK(b.size() == 1 && b[0] == 0x00, "0 encodes as a single 0x00");
        b.clear();
        appendVarint(b, 127);
        CHECK(b.size() == 1 && b[0] == 0x7F, "127 encodes as 0x7F");
        b.clear();
        appendVarint(b, 128);
        CHECK(b.size() == 2 && b[0] == 0x80 && b[1] == 0x01, "128 encodes as 0x80 0x01 (LEB128)");
        b.clear();
        appendVarint(b, 300);
        CHECK(b.size() == 2 && b[0] == 0xAC && b[1] == 0x02, "300 encodes as 0xAC 0x02 (spec example)");
    }

    // --- 2. Unsigned round-trip across a wide sweep + the extremes. ---
    {
        std::vector<std::uint64_t> vals = {0, 1, 2, 63, 64, 127, 128, 255, 256, 16383, 16384,
                                           1000000, 1ull << 32, (1ull << 63), UINT64_MAX};
        for (std::uint64_t step = 1; step < 5000; step += 37) vals.push_back(step * step);
        for (std::uint64_t v : vals) {
            std::vector<std::uint8_t> buf;
            appendVarint(buf, v);
            CHECK(buf.size() == varintSize(v), "encoded length matches varintSize");
            std::size_t off = 0;
            std::uint64_t got = 0;
            CHECK(readVarint(buf, off, got) && got == v && off == buf.size(), "unsigned round-trips exactly");
        }
    }

    // --- 3. Zigzag folds signed values; small negatives are one byte. ---
    {
        CHECK(zigzagEncode(0) == 0 && zigzagEncode(-1) == 1 && zigzagEncode(1) == 2, "zigzag maps 0,-1,1 -> 0,1,2");
        CHECK(zigzagEncode(-2) == 3 && zigzagEncode(2) == 4, "zigzag maps -2,2 -> 3,4");
        std::vector<std::uint8_t> b;
        appendVarintSigned(b, -1);
        CHECK(b.size() == 1, "-1 encodes in a single byte thanks to zigzag");

        const std::int64_t signedVals[] = {0, -1, 1, -2, 2, -64, 63, -12345, 12345,
                                           INT64_MIN, INT64_MAX, -1000000000};
        for (std::int64_t v : signedVals) {
            std::vector<std::uint8_t> buf;
            appendVarintSigned(buf, v);
            std::size_t off = 0;
            std::int64_t got = 0;
            CHECK(readVarintSigned(buf, off, got) && got == v && off == buf.size(), "signed round-trips exactly");
        }
    }

    // --- 4. Several values pack into one buffer and decode sequentially. ---
    {
        std::vector<std::uint8_t> buf;
        appendVarint(buf, 1);
        appendVarint(buf, 300);
        appendVarintSigned(buf, -7);
        appendVarint(buf, UINT64_MAX);
        std::size_t off = 0;
        std::uint64_t a = 0, b = 0, d = 0;
        std::int64_t c = 0;
        CHECK(readVarint(buf, off, a) && a == 1, "first value");
        CHECK(readVarint(buf, off, b) && b == 300, "second value");
        CHECK(readVarintSigned(buf, off, c) && c == -7, "third (signed) value");
        CHECK(readVarint(buf, off, d) && d == UINT64_MAX, "fourth value");
        CHECK(off == buf.size(), "offset lands exactly at the end after all four");
    }

    // --- 5. Truncated and over-long streams are rejected safely. ---
    {
        // A single continuation byte with no terminator: truncated.
        const std::vector<std::uint8_t> truncated = {0x80};
        std::size_t off = 0;
        std::uint64_t v = 0;
        CHECK(!readVarint(truncated, off, v), "truncated stream rejected");

        // Empty buffer.
        off = 0;
        CHECK(!readVarint(nullptr, 0, off, v), "empty stream rejected");

        // Eleven continuation bytes then a byte: exceeds 64 bits -> overflow rejected.
        std::vector<std::uint8_t> overlong(10, 0xFF);
        overlong.push_back(0x7F);
        off = 0;
        CHECK(!readVarint(overlong, off, v), "over-long (>64-bit) stream rejected");
    }

    if (g_fail == 0) {
        std::printf("varint: OK — sizes, boundaries, unsigned/signed round-trip, packed stream, truncation/overflow.\n");
        return 0;
    }
    std::printf("varint: %d failure(s).\n", g_fail);
    return 1;
}
