// tests/io/messagepack.cpp — verifies the MessagePack codec (io MessagePack.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * canonical encoding matches published spec byte vectors (fixint, uint8/16, negative fixint, int8,
//     float64, fixstr, fixarray, fixmap, nil/bool, bin8);
//   * decoding those exact bytes recovers the original values;
//   * encode->decode round-trips a deeply nested mixed structure;
//   * a randomized round-trip over thousands of generated values is identity;
//   * truncated / trailing-garbage byte streams are rejected.
#include "maz/io/MessagePack.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using maz::io::MsgValue;
using maz::io::msgpackDecode;
using maz::io::msgpackEncode;
using Bytes = std::vector<std::uint8_t>;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

static bool eqBytes(const Bytes& a, const Bytes& b) { return a == b; }

// Build a random value tree (bounded depth) for round-trip fuzzing.
static MsgValue randValue(Lcg& rng, int depth) {
    const std::uint32_t k = rng.next() % (depth <= 0 ? 6u : 9u);
    switch (k) {
    case 0: return MsgValue::nil();
    case 1: return MsgValue::boolean((rng.next() & 1u) != 0u);
    // Strictly-negative: MessagePack keeps signed ints as Int only for negative values (a non-negative
    // Int encodes byte-identically to a UInt and canonically decodes back as UInt — pinned in test 6).
    case 2: return MsgValue::integer(-1 - static_cast<std::int64_t>(rng.next() % 2000000000u));
    case 3: return MsgValue::uinteger(static_cast<std::uint64_t>(rng.next()) * 1000003ull);
    case 4: {
        std::string s;
        const std::uint32_t n = rng.next() % 40u;
        for (std::uint32_t i = 0; i < n; ++i) s.push_back(static_cast<char>('a' + rng.next() % 26u));
        return MsgValue::str(std::move(s));
    }
    case 5: {
        Bytes b;
        const std::uint32_t n = rng.next() % 20u;
        for (std::uint32_t i = 0; i < n; ++i) b.push_back(static_cast<std::uint8_t>(rng.next() & 0xffu));
        return MsgValue::binary(std::move(b));
    }
    case 6: {
        std::vector<MsgValue> a;
        const std::uint32_t n = rng.next() % 5u;
        for (std::uint32_t i = 0; i < n; ++i) a.push_back(randValue(rng, depth - 1));
        return MsgValue::array(std::move(a));
    }
    case 7: {
        std::vector<std::pair<MsgValue, MsgValue>> m;
        const std::uint32_t n = rng.next() % 5u;
        for (std::uint32_t i = 0; i < n; ++i)
            m.emplace_back(MsgValue::uinteger(static_cast<std::uint64_t>(i)), randValue(rng, depth - 1));
        return MsgValue::map(std::move(m));
    }
    default: {
        // A double that survives an exact round-trip.
        const double d = static_cast<double>(static_cast<std::int32_t>(rng.next())) * 0.5;
        return MsgValue::number(d);
    }
    }
}

int main() {
    // --- 1. Canonical encoding matches the spec byte vectors. ---
    {
        CHECK(eqBytes(msgpackEncode(MsgValue::nil()), Bytes{0xc0}), "nil");
        CHECK(eqBytes(msgpackEncode(MsgValue::boolean(true)), Bytes{0xc3}), "true");
        CHECK(eqBytes(msgpackEncode(MsgValue::boolean(false)), Bytes{0xc2}), "false");
        CHECK(eqBytes(msgpackEncode(MsgValue::uinteger(0)), Bytes{0x00}), "0 fixint");
        CHECK(eqBytes(msgpackEncode(MsgValue::uinteger(127)), Bytes{0x7f}), "127 fixint");
        CHECK(eqBytes(msgpackEncode(MsgValue::uinteger(128)), Bytes{0xcc, 0x80}), "128 uint8");
        CHECK(eqBytes(msgpackEncode(MsgValue::uinteger(256)), Bytes{0xcd, 0x01, 0x00}), "256 uint16");
        CHECK(eqBytes(msgpackEncode(MsgValue::integer(-1)), Bytes{0xff}), "-1 neg fixint");
        CHECK(eqBytes(msgpackEncode(MsgValue::integer(-32)), Bytes{0xe0}), "-32 neg fixint");
        CHECK(eqBytes(msgpackEncode(MsgValue::integer(-33)), Bytes{0xd0, 0xdf}), "-33 int8");
        CHECK(eqBytes(msgpackEncode(MsgValue::number(1.5)),
                      Bytes{0xcb, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}), "1.5 float64");
        CHECK(eqBytes(msgpackEncode(MsgValue::str("")), Bytes{0xa0}), "empty fixstr");
        CHECK(eqBytes(msgpackEncode(MsgValue::str("a")), Bytes{0xa1, 0x61}), "'a' fixstr");
        CHECK(eqBytes(msgpackEncode(MsgValue::str("hello")),
                      Bytes{0xa5, 'h', 'e', 'l', 'l', 'o'}), "'hello' fixstr");
        CHECK(eqBytes(msgpackEncode(MsgValue::array({MsgValue::uinteger(1), MsgValue::uinteger(2),
                                                     MsgValue::uinteger(3)})),
                      Bytes{0x93, 0x01, 0x02, 0x03}), "[1,2,3] fixarray");
        CHECK(eqBytes(msgpackEncode(MsgValue::map({{MsgValue::uinteger(1), MsgValue::uinteger(2)}})),
                      Bytes{0x81, 0x01, 0x02}), "{1:2} fixmap");
        CHECK(eqBytes(msgpackEncode(MsgValue::binary({0x01, 0x02})),
                      Bytes{0xc4, 0x02, 0x01, 0x02}), "bin8 [1,2]");
    }

    // --- 2. Decoding the spec bytes recovers the values. ---
    {
        MsgValue v;
        CHECK(msgpackDecode(Bytes{0xcc, 0x80}, v) && v.type() == MsgValue::Type::UInt && v.asUInt() == 128,
              "decode 128");
        CHECK(msgpackDecode(Bytes{0xd0, 0xdf}, v) && v.type() == MsgValue::Type::Int && v.asInt() == -33,
              "decode -33");
        CHECK(msgpackDecode(Bytes{0xa5, 'h', 'e', 'l', 'l', 'o'}, v) && v.asStr() == "hello",
              "decode 'hello'");
        CHECK(msgpackDecode(Bytes{0x93, 0x01, 0x02, 0x03}, v) && v.type() == MsgValue::Type::Array &&
                  v.asArray().size() == 3 && v.asArray()[2].asUInt() == 3, "decode [1,2,3]");
    }

    // --- 3. Round-trip a hand-built nested structure. ---
    {
        MsgValue doc = MsgValue::map({
            {MsgValue::str("name"), MsgValue::str("Maz")},
            {MsgValue::str("ver"), MsgValue::uinteger(715)},
            {MsgValue::str("pos"), MsgValue::array({MsgValue::number(1.25), MsgValue::number(-3.5)})},
            {MsgValue::str("flags"), MsgValue::array({MsgValue::boolean(true), MsgValue::nil()})},
            {MsgValue::str("blob"), MsgValue::binary({0xde, 0xad, 0xbe, 0xef})},
        });
        const Bytes enc = msgpackEncode(doc);
        MsgValue back;
        CHECK(msgpackDecode(enc, back) && back == doc, "nested structure round-trips");
    }

    // --- 4. Randomized round-trip identity. ---
    {
        Lcg rng{0x11515u};
        bool ok = true;
        long totalBytes = 0;
        for (int trial = 0; trial < 5000 && ok; ++trial) {
            const MsgValue v = randValue(rng, 3);
            const Bytes enc = msgpackEncode(v);
            totalBytes += static_cast<long>(enc.size());
            MsgValue back;
            if (!msgpackDecode(enc, back) || back != v) { ok = false; break; }
        }
        CHECK(ok, "random values encode/decode to themselves");
        CHECK(totalBytes > 5000, "the fuzz produced a substantial byte volume");
    }

    // --- 5. Malformed / truncated input is rejected. ---
    {
        MsgValue v;
        CHECK(!msgpackDecode(Bytes{0xcc}, v), "truncated uint8 rejected");
        CHECK(!msgpackDecode(Bytes{0xa5, 'h', 'i'}, v), "truncated fixstr rejected");
        CHECK(!msgpackDecode(Bytes{0x93, 0x01, 0x02}, v), "array missing an element rejected");
        CHECK(!msgpackDecode(Bytes{0x00, 0x00}, v), "trailing garbage after a value rejected");
        CHECK(!msgpackDecode(Bytes{0xc1}, v), "reserved byte 0xc1 rejected");
        CHECK(!msgpackDecode(Bytes{}, v), "empty input rejected");
    }

    // --- 6. Canonicalization: a non-negative Int decodes back as UInt (same value), by spec. ---
    {
        MsgValue v;
        CHECK(msgpackDecode(msgpackEncode(MsgValue::integer(5)), v) &&
                  v.type() == MsgValue::Type::UInt && v.asUInt() == 5,
              "non-negative Int canonically round-trips as UInt(5)");
        CHECK(msgpackDecode(msgpackEncode(MsgValue::integer(-5)), v) &&
                  v.type() == MsgValue::Type::Int && v.asInt() == -5,
              "negative Int round-trips as Int(-5)");
    }

    if (g_fail == 0) {
        std::printf("messagepack: OK — spec vectors, decode, nested + 5000-trial round-trip, error handling.\n");
        return 0;
    }
    std::printf("messagepack: %d failure(s).\n", g_fail);
    return 1;
}
