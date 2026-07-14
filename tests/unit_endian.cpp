// Unit tests for maz::core byte-order helpers (Endian.hpp). Exercises exact
// byteSwap16/32/64 values + their involution, the exact byte layout produced by
// writeU32LE/BE (LSB-first vs MSB-first), write/read round-trips across 16/32/64
// and both endiannesses, reads from known byte arrays, host<->little/big
// conversion round-trips with a hostIsLittleEndian()-computed native-identity
// expectation (host-independent), and cross-endianness consistency (reading
// LE-written bytes as BE == byteSwap). Pure C++, no GPU/display.

#include "maz/core/Endian.hpp"

#include <cstdio>
#include <cstdint>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- byteSwap exact ------------------------------------------------------
    {
        check(byteSwap16(0x1234) == 0x3412, "byteSwap16(0x1234) == 0x3412");
        check(byteSwap32(0x12345678u) == 0x78563412u, "byteSwap32(0x12345678) == 0x78563412");
        check(byteSwap64(0x0123456789ABCDEFull) == 0xEFCDAB8967452301ull,
              "byteSwap64(0x0123456789ABCDEF) == 0xEFCDAB8967452301");
    }

    // --- byteSwap involution -------------------------------------------------
    {
        const std::uint16_t v16[] = {0x0000, 0x00FF, 0xFF00, 0x1234, 0xABCD, 0xFFFF};
        for (std::uint16_t x : v16) { check(byteSwap16(byteSwap16(x)) == x, "byteSwap16 involution"); }
        const std::uint32_t v32[] = {0x00000000u, 0xFFFFFFFFu, 0x12345678u, 0xDEADBEEFu};
        for (std::uint32_t x : v32) { check(byteSwap32(byteSwap32(x)) == x, "byteSwap32 involution"); }
        const std::uint64_t v64[] = {0ull, 0xFFFFFFFFFFFFFFFFull, 0x0123456789ABCDEFull};
        for (std::uint64_t x : v64) { check(byteSwap64(byteSwap64(x)) == x, "byteSwap64 involution"); }
    }

    // --- writeU32LE exact bytes (LSB first) ----------------------------------
    {
        std::uint8_t b[4];
        writeU32LE(b, 0x12345678u);
        check(b[0] == 0x78 && b[1] == 0x56 && b[2] == 0x34 && b[3] == 0x12,
              "writeU32LE(0x12345678) -> {78,56,34,12}");
    }

    // --- writeU32BE exact bytes (MSB first) ----------------------------------
    {
        std::uint8_t b[4];
        writeU32BE(b, 0x12345678u);
        check(b[0] == 0x12 && b[1] == 0x34 && b[2] == 0x56 && b[3] == 0x78,
              "writeU32BE(0x12345678) -> {12,34,56,78}");
    }

    // --- write/read round-trip (16/32/64, LE and BE) -------------------------
    {
        const std::uint16_t v16[] = {0x0000, 0xFFFF, 0x1234};
        for (std::uint16_t x : v16) {
            std::uint8_t b[2];
            writeU16LE(b, x); check(readU16LE(b) == x, "U16 LE round-trip");
            writeU16BE(b, x); check(readU16BE(b) == x, "U16 BE round-trip");
        }
        const std::uint32_t v32[] = {0x00000000u, 0xFFFFFFFFu, 0x12345678u};
        for (std::uint32_t x : v32) {
            std::uint8_t b[4];
            writeU32LE(b, x); check(readU32LE(b) == x, "U32 LE round-trip");
            writeU32BE(b, x); check(readU32BE(b) == x, "U32 BE round-trip");
        }
        const std::uint64_t v64[] = {0ull, 0xFFFFFFFFFFFFFFFFull, 0x0123456789ABCDEFull};
        for (std::uint64_t x : v64) {
            std::uint8_t b[8];
            writeU64LE(b, x); check(readU64LE(b) == x, "U64 LE round-trip");
            writeU64BE(b, x); check(readU64BE(b) == x, "U64 BE round-trip");
        }
    }

    // --- read from known bytes -----------------------------------------------
    {
        std::uint8_t le[4] = {0x78, 0x56, 0x34, 0x12};
        check(readU32LE(le) == 0x12345678u, "readU32LE({78,56,34,12}) == 0x12345678");
        std::uint8_t be[4] = {0x12, 0x34, 0x56, 0x78};
        check(readU32BE(be) == 0x12345678u, "readU32BE({12,34,56,78}) == 0x12345678");
    }

    // --- 16 and 64 bit write/read + exact bytes ------------------------------
    {
        std::uint8_t b16[2];
        writeU16LE(b16, 0xABCD);
        check(b16[0] == 0xCD && b16[1] == 0xAB, "writeU16LE(0xABCD) -> {CD,AB}");
        check(readU16LE(b16) == 0xABCD, "readU16LE back -> 0xABCD");

        std::uint8_t b64[8];
        writeU64LE(b64, 0x0123456789ABCDEFull);
        check(b64[0] == 0xEF && b64[7] == 0x01, "writeU64LE(0x0123...EF) first=EF last=01");
        check(readU64LE(b64) == 0x0123456789ABCDEFull, "readU64LE back -> 0x0123456789ABCDEF");
    }

    // --- host conversions round-trip + native identity -----------------------
    {
        const std::uint32_t x = 0x12345678u;
        check(littleToHost32(hostToLittle32(x)) == x, "hostToLittle32 then littleToHost32 == x");
        check(bigToHost32(hostToBig32(x)) == x, "hostToBig32 then bigToHost32 == x");
        if (hostIsLittleEndian()) {
            check(hostToLittle32(x) == x, "little host: hostToLittle32 identity");
            check(hostToBig32(x) == byteSwap32(x), "little host: hostToBig32 == byteSwap32");
        } else {
            check(hostToBig32(x) == x, "big host: hostToBig32 identity");
            check(hostToLittle32(x) == byteSwap32(x), "big host: hostToLittle32 == byteSwap32");
        }
    }

    // --- cross-consistency ---------------------------------------------------
    {
        const std::uint32_t x = 0x12345678u;
        std::uint8_t b[4];
        writeU32LE(b, x);
        check(readU32BE(b) == byteSwap32(x), "readU32BE(writeU32LE(x)) == byteSwap32(x)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
