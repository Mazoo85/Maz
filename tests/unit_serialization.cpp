// Unit tests for maz::core::ByteWriter/ByteReader. Exercises the little-endian
// length-prefixed binary format: exact on-disk byte order, scalar/string/StringId
// round-trips, and the fail-safe reader (bounds- and overflow-checked reads that
// never throw, never read OOB, and reject a corrupt length without a huge alloc).
// Pure C++, no GPU/display.

#include "maz/core/Serialization.hpp"
#include "maz/core/StringId.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

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
    // --- UNSIGNED ROUND-TRIP --------------------------------------------------
    {
        ByteWriter w;
        w.writeU8(0xABu);
        w.writeU16(0xBEEFu);
        w.writeU32(0xDEADBEEFu);
        w.writeU64(0x0123456789ABCDEFull);  // high bits set: catches getLE truncation

        ByteReader r(w.buffer());
        std::uint8_t u8 = 0;
        std::uint16_t u16 = 0;
        std::uint32_t u32 = 0;
        std::uint64_t u64 = 0;
        check(r.readU8(u8) && u8 == 0xABu, "u8 round-trips");
        check(r.readU16(u16) && u16 == 0xBEEFu, "u16 round-trips");
        check(r.readU32(u32) && u32 == 0xDEADBEEFu, "u32 round-trips");
        check(r.readU64(u64) && u64 == 0x0123456789ABCDEFull, "u64 (high bits) round-trips");
        check(r.ok(), "reader ok after unsigned round-trip");
        check(r.remaining() == 0, "no bytes remaining after unsigned round-trip");
    }

    // --- EXACT LITTLE-ENDIAN LAYOUT -------------------------------------------
    {
        ByteWriter w;
        w.writeU32(0x11223344u);
        check(w.size() == 4, "writeU32 appends exactly 4 bytes");
        check(w.data()[0] == 0x44 && w.data()[1] == 0x33 && w.data()[2] == 0x22 && w.data()[3] == 0x11,
              "u32 stored little-endian");

        ByteWriter w2;
        w2.writeU16(0xAABBu);
        check(w2.data()[0] == 0xBB && w2.data()[1] == 0xAA, "u16 stored little-endian");
    }

    // --- SIGNED (INCLUDING NEGATIVES) -----------------------------------------
    {
        ByteWriter w;
        w.writeI32(-1);
        w.writeI32(INT32_MIN);
        w.writeI16(-12345);
        w.writeI64(-1234567890123LL);
        w.writeI8(-128);

        ByteReader r(w.buffer());
        std::int32_t i32a = 0;
        std::int32_t i32b = 0;
        std::int16_t i16 = 0;
        std::int64_t i64 = 0;
        std::int8_t i8 = 0;
        check(r.readI32(i32a) && i32a == -1, "i32 -1 round-trips");
        check(r.readI32(i32b) && i32b == INT32_MIN, "i32 INT32_MIN round-trips");
        check(r.readI16(i16) && i16 == -12345, "i16 -12345 round-trips");
        check(r.readI64(i64) && i64 == -1234567890123LL, "i64 negative round-trips");
        check(r.readI8(i8) && i8 == -128, "i8 -128 round-trips");
        check(r.remaining() == 0, "no bytes remaining after signed round-trip");
    }

    // --- BOOL -----------------------------------------------------------------
    {
        ByteWriter w;
        w.writeBool(true);
        w.writeBool(false);
        ByteReader r(w.buffer());
        bool bt = false;
        bool bf = true;
        check(r.readBool(bt) && bt == true, "bool true round-trips");
        check(r.readBool(bf) && bf == false, "bool false round-trips");
    }

    // --- FLOAT / DOUBLE (BIT-EXACT: use ==, NOT epsilon) ----------------------
    // These are bit-exact round-trips of the stored representation, not
    // approximate math — do NOT change these to an epsilon comparison.
    {
        ByteWriter w;
        w.writeFloat(3.14159f);
        w.writeDouble(2.718281828459045);
        w.writeFloat(-1.5f);
        w.writeFloat(0.0f);

        ByteReader r(w.buffer());
        float f0 = 0.0f;
        double d0 = 0.0;
        float f1 = 0.0f;
        float f2 = 0.0f;
        check(r.readFloat(f0) && f0 == 3.14159f, "float 3.14159 bit-exact round-trips");
        check(r.readDouble(d0) && d0 == 2.718281828459045, "double e bit-exact round-trips");
        check(r.readFloat(f1) && f1 == -1.5f, "float -1.5 bit-exact round-trips");
        check(r.readFloat(f2) && f2 == 0.0f, "float 0.0 bit-exact round-trips");
        check(r.remaining() == 0, "no bytes remaining after float/double round-trip");
    }

    // --- STRING ---------------------------------------------------------------
    {
        ByteWriter w;
        w.writeString("hello world");
        w.writeString("");
        w.writeString("second");

        ByteReader r(w.buffer());
        std::string s0;
        std::string s1;
        std::string s2;
        check(r.readString(s0) && s0 == "hello world", "non-empty string round-trips");
        check(r.readString(s1) && s1.empty(), "empty string round-trips to empty");
        check(r.readString(s2) && s2 == "second", "second string round-trips");
        check(r.remaining() == 0, "no bytes remaining after string round-trip");
    }

    // --- STRINGID -------------------------------------------------------------
    {
        ByteWriter w;
        w.writeStringId("foobar"_sid);
        ByteReader r(w.buffer());
        StringId id;
        check(r.readStringId(id) && id == "foobar"_sid && id.value == StringId("foobar"_sid).value,
              "StringId round-trips");
    }

    // --- MIXED SEQUENCE -------------------------------------------------------
    {
        ByteWriter w;
        w.writeU32(0xCAFEBABEu);
        w.writeString("mix");
        w.writeFloat(1.25f);
        w.writeI64(-42);
        w.writeBool(true);

        ByteReader r(w.buffer());
        std::uint32_t u32 = 0;
        std::string s;
        float f = 0.0f;
        std::int64_t i64 = 0;
        bool b = false;
        check(r.readU32(u32) && u32 == 0xCAFEBABEu, "mixed: u32");
        check(r.readString(s) && s == "mix", "mixed: string");
        check(r.readFloat(f) && f == 1.25f, "mixed: float");
        check(r.readI64(i64) && i64 == -42, "mixed: i64");
        check(r.readBool(b) && b == true, "mixed: bool");
        check(r.ok() && r.remaining() == 0, "mixed: reader ok and drained");
    }

    // --- TRUNCATION SAFETY (KEY gate) -----------------------------------------
    {
        // a. read past the end of a short buffer fails and sets failed().
        std::uint8_t buf2[2] = {1, 2};
        ByteReader r(buf2, 2);
        std::uint32_t out = 0;
        check(!r.readU32(out), "readU32 on a 2-byte buffer returns false");
        check(r.failed(), "reader failed after short readU32");

        // b. sticky: a subsequent read still fails even though 1 byte remains.
        std::uint8_t out8 = 0;
        check(!r.readU8(out8), "sticky: readU8 fails after an earlier failure");
        check(r.failed(), "reader stays failed (sticky)");
    }
    {
        // c. corrupt length: a 0xFFFF... length must fail without a huge alloc.
        ByteWriter w;
        w.writeU64(0xFFFFFFFFFFFFFFFFull);
        ByteReader r2(w.buffer());
        std::string s;
        check(!r2.readString(s), "corrupt huge length: readString returns false");
        check(r2.failed(), "corrupt huge length: reader failed (no alloc, no crash)");

        // milder: a plausible-but-too-large length also fails.
        ByteWriter w3;
        w3.writeU64(100);
        ByteReader r3(w3.buffer());
        std::string s3;
        check(!r3.readString(s3), "length beyond buffer: readString returns false");
    }
    {
        // d. boundary: an exact read drains the buffer, then the next read fails.
        ByteWriter w4;
        w4.writeU32(0x01020304u);
        ByteReader r4(w4.buffer());
        std::uint32_t v = 0;
        check(r4.readU32(v) && v == 0x01020304u, "boundary: exact readU32 succeeds");
        check(r4.remaining() == 0, "boundary: buffer drained");
        std::uint8_t x = 0;
        check(!r4.readU8(x) && r4.failed(), "boundary: read past end fails");
    }

    // --- CLEAR ----------------------------------------------------------------
    {
        ByteWriter w;
        w.writeU32(5);
        w.clear();
        check(w.size() == 0, "size() == 0 after clear()");
        w.writeU16(7);
        ByteReader r(w.buffer());
        std::uint16_t v = 0;
        check(r.readU16(v) && v == 7, "writer reusable after clear()");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
