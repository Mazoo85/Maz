#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// maz::io::MessagePack — the MessagePack binary serialization format (https://msgpack.org): a compact,
// self-describing interchange encoding that is like JSON but in bytes. The engine already has JSON (great
// for human-edited config, but verbose and slow) and its own tag-free binary Serialize (tiny, but the two
// ends must agree on the exact layout in advance). MessagePack fills the gap between them: it is small and
// fast like binary, yet SELF-DESCRIBING like JSON — a decoder recovers the full structure (nil/bool/int/
// float/string/bytes/array/map) with no schema. That makes it the natural wire format for network
// messages, replays, and save files that must stay readable across versions, and it interоperates with the
// MessagePack libraries shipped for essentially every language (handy for tools and servers). This encoder
// is CANONICAL — every value takes its smallest legal representation — so the output byte-for-byte matches
// the published spec vectors, which is exactly what the tests pin down. Header-only, std-only, deterministic.
// Godot only offers JSON + its own var_to_bytes; MessagePack is the portable standard.
namespace maz::io {

class MsgValue {
public:
    enum class Type { Nil, Bool, Int, UInt, Double, Str, Bin, Array, Map };

    MsgValue() = default;

    static MsgValue nil() { return MsgValue{}; }
    static MsgValue boolean(bool v) { MsgValue m; m.m_type = Type::Bool; m.m_b = v; return m; }
    static MsgValue integer(std::int64_t v) { MsgValue m; m.m_type = Type::Int; m.m_i = v; return m; }
    static MsgValue uinteger(std::uint64_t v) { MsgValue m; m.m_type = Type::UInt; m.m_u = v; return m; }
    static MsgValue number(double v) { MsgValue m; m.m_type = Type::Double; m.m_d = v; return m; }
    static MsgValue str(std::string v) { MsgValue m; m.m_type = Type::Str; m.m_s = std::move(v); return m; }
    static MsgValue binary(std::vector<std::uint8_t> v) {
        MsgValue m; m.m_type = Type::Bin; m.m_bin = std::move(v); return m;
    }
    static MsgValue array(std::vector<MsgValue> v) {
        MsgValue m; m.m_type = Type::Array; m.m_arr = std::move(v); return m;
    }
    static MsgValue map(std::vector<std::pair<MsgValue, MsgValue>> v) {
        MsgValue m; m.m_type = Type::Map; m.m_map = std::move(v); return m;
    }

    Type type() const { return m_type; }
    bool asBool() const { return m_b; }
    std::int64_t asInt() const { return m_i; }
    std::uint64_t asUInt() const { return m_u; }
    double asDouble() const { return m_d; }
    const std::string& asStr() const { return m_s; }
    const std::vector<std::uint8_t>& asBin() const { return m_bin; }
    const std::vector<MsgValue>& asArray() const { return m_arr; }
    const std::vector<std::pair<MsgValue, MsgValue>>& asMap() const { return m_map; }

    bool operator==(const MsgValue& o) const {
        if (m_type != o.m_type) {
            return false;
        }
        switch (m_type) {
        case Type::Nil: return true;
        case Type::Bool: return m_b == o.m_b;
        case Type::Int: return m_i == o.m_i;
        case Type::UInt: return m_u == o.m_u;
        case Type::Double: return m_d == o.m_d;
        case Type::Str: return m_s == o.m_s;
        case Type::Bin: return m_bin == o.m_bin;
        case Type::Array: return m_arr == o.m_arr;
        case Type::Map: return m_map == o.m_map;
        }
        return false;
    }
    bool operator!=(const MsgValue& o) const { return !(*this == o); }

private:
    Type m_type = Type::Nil;
    bool m_b = false;
    std::int64_t m_i = 0;
    std::uint64_t m_u = 0;
    double m_d = 0.0;
    std::string m_s;
    std::vector<std::uint8_t> m_bin;
    std::vector<MsgValue> m_arr;
    std::vector<std::pair<MsgValue, MsgValue>> m_map;
};

namespace detail {

inline void putBE(std::vector<std::uint8_t>& out, std::uint64_t v, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xffu));
    }
}

inline void encodeUnsigned(std::vector<std::uint8_t>& out, std::uint64_t u) {
    if (u <= 0x7fu) {
        out.push_back(static_cast<std::uint8_t>(u)); // positive fixint
    } else if (u <= 0xffu) {
        out.push_back(0xccu);
        putBE(out, u, 1);
    } else if (u <= 0xffffu) {
        out.push_back(0xcdu);
        putBE(out, u, 2);
    } else if (u <= 0xffffffffull) {
        out.push_back(0xceu);
        putBE(out, u, 4);
    } else {
        out.push_back(0xcfu);
        putBE(out, u, 8);
    }
}

inline void encodeSigned(std::vector<std::uint8_t>& out, std::int64_t v) {
    if (v >= 0) {
        encodeUnsigned(out, static_cast<std::uint64_t>(v));
        return;
    }
    if (v >= -32) {
        out.push_back(static_cast<std::uint8_t>(v)); // negative fixint (0xe0..0xff)
    } else if (v >= -128) {
        out.push_back(0xd0u);
        putBE(out, static_cast<std::uint64_t>(static_cast<std::uint8_t>(static_cast<std::int8_t>(v))), 1);
    } else if (v >= -32768) {
        out.push_back(0xd1u);
        putBE(out, static_cast<std::uint64_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(v))), 2);
    } else if (v >= -2147483648ll) {
        out.push_back(0xd2u);
        putBE(out, static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(v))), 4);
    } else {
        out.push_back(0xd3u);
        putBE(out, static_cast<std::uint64_t>(v), 8);
    }
}

inline void encodeValue(std::vector<std::uint8_t>& out, const MsgValue& v) {
    using Type = MsgValue::Type;
    switch (v.type()) {
    case Type::Nil:
        out.push_back(0xc0u);
        break;
    case Type::Bool:
        out.push_back(v.asBool() ? 0xc3u : 0xc2u);
        break;
    case Type::Int:
        encodeSigned(out, v.asInt());
        break;
    case Type::UInt:
        encodeUnsigned(out, v.asUInt());
        break;
    case Type::Double: {
        out.push_back(0xcbu);
        std::uint64_t bits = 0;
        const double d = v.asDouble();
        std::memcpy(&bits, &d, sizeof(bits));
        putBE(out, bits, 8);
        break;
    }
    case Type::Str: {
        const std::string& s = v.asStr();
        const std::size_t n = s.size();
        if (n <= 31) {
            out.push_back(static_cast<std::uint8_t>(0xa0u | n)); // fixstr
        } else if (n <= 0xffu) {
            out.push_back(0xd9u);
            putBE(out, n, 1);
        } else if (n <= 0xffffu) {
            out.push_back(0xdau);
            putBE(out, n, 2);
        } else {
            out.push_back(0xdbu);
            putBE(out, n, 4);
        }
        out.insert(out.end(), s.begin(), s.end());
        break;
    }
    case Type::Bin: {
        const std::vector<std::uint8_t>& b = v.asBin();
        const std::size_t n = b.size();
        if (n <= 0xffu) {
            out.push_back(0xc4u);
            putBE(out, n, 1);
        } else if (n <= 0xffffu) {
            out.push_back(0xc5u);
            putBE(out, n, 2);
        } else {
            out.push_back(0xc6u);
            putBE(out, n, 4);
        }
        out.insert(out.end(), b.begin(), b.end());
        break;
    }
    case Type::Array: {
        const std::vector<MsgValue>& a = v.asArray();
        const std::size_t n = a.size();
        if (n <= 15) {
            out.push_back(static_cast<std::uint8_t>(0x90u | n)); // fixarray
        } else if (n <= 0xffffu) {
            out.push_back(0xdcu);
            putBE(out, n, 2);
        } else {
            out.push_back(0xddu);
            putBE(out, n, 4);
        }
        for (const MsgValue& e : a) {
            encodeValue(out, e);
        }
        break;
    }
    case Type::Map: {
        const auto& m = v.asMap();
        const std::size_t n = m.size();
        if (n <= 15) {
            out.push_back(static_cast<std::uint8_t>(0x80u | n)); // fixmap
        } else if (n <= 0xffffu) {
            out.push_back(0xdeu);
            putBE(out, n, 2);
        } else {
            out.push_back(0xdfu);
            putBE(out, n, 4);
        }
        for (const auto& kv : m) {
            encodeValue(out, kv.first);
            encodeValue(out, kv.second);
        }
        break;
    }
    }
}

// A bounds-checked cursor over the input bytes.
struct Reader {
    const std::uint8_t* p;
    std::size_t n;
    std::size_t pos = 0;
    bool ok = true;

    bool need(std::size_t k) {
        if (pos + k > n) {
            ok = false;
            return false;
        }
        return true;
    }
    std::uint8_t byte() { return p[pos++]; }
    std::uint64_t be(int bytes) {
        std::uint64_t v = 0;
        for (int i = 0; i < bytes; ++i) {
            v = (v << 8) | static_cast<std::uint64_t>(p[pos++]);
        }
        return v;
    }
};

inline MsgValue decodeValue(Reader& r, int depth);
inline MsgValue decodeArray(Reader& r, std::size_t len, int depth);
inline MsgValue decodeMap(Reader& r, std::size_t len, int depth);

inline MsgValue decodeValue(Reader& r, int depth) {
    if (depth > 256 || !r.need(1)) {
        r.ok = false;
        return MsgValue::nil();
    }
    const std::uint8_t c = r.byte();

    if (c <= 0x7fu) {
        return MsgValue::uinteger(c); // positive fixint
    }
    if (c >= 0xe0u) {
        return MsgValue::integer(static_cast<std::int8_t>(c)); // negative fixint
    }
    if ((c & 0xe0u) == 0xa0u) { // fixstr
        const std::size_t len = c & 0x1fu;
        if (!r.need(len)) return MsgValue::nil();
        std::string s(reinterpret_cast<const char*>(r.p + r.pos), len);
        r.pos += len;
        return MsgValue::str(std::move(s));
    }
    if ((c & 0xf0u) == 0x90u) { // fixarray
        return decodeArray(r, c & 0x0fu, depth);
    }
    if ((c & 0xf0u) == 0x80u) { // fixmap
        return decodeMap(r, c & 0x0fu, depth);
    }

    switch (c) {
    case 0xc0u: return MsgValue::nil();
    case 0xc2u: return MsgValue::boolean(false);
    case 0xc3u: return MsgValue::boolean(true);
    case 0xccu: if (!r.need(1)) break; return MsgValue::uinteger(r.be(1));
    case 0xcdu: if (!r.need(2)) break; return MsgValue::uinteger(r.be(2));
    case 0xceu: if (!r.need(4)) break; return MsgValue::uinteger(r.be(4));
    case 0xcfu: if (!r.need(8)) break; return MsgValue::uinteger(r.be(8));
    case 0xd0u: if (!r.need(1)) break; return MsgValue::integer(static_cast<std::int8_t>(r.be(1)));
    case 0xd1u: if (!r.need(2)) break; return MsgValue::integer(static_cast<std::int16_t>(r.be(2)));
    case 0xd2u: if (!r.need(4)) break; return MsgValue::integer(static_cast<std::int32_t>(r.be(4)));
    case 0xd3u: if (!r.need(8)) break; return MsgValue::integer(static_cast<std::int64_t>(r.be(8)));
    case 0xcau: { // float32
        if (!r.need(4)) break;
        const std::uint32_t bits = static_cast<std::uint32_t>(r.be(4));
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof(f));
        return MsgValue::number(static_cast<double>(f));
    }
    case 0xcbu: { // float64
        if (!r.need(8)) break;
        const std::uint64_t bits = r.be(8);
        double d = 0.0;
        std::memcpy(&d, &bits, sizeof(d));
        return MsgValue::number(d);
    }
    case 0xd9u: case 0xdau: case 0xdbu: { // str8/16/32
        const int lb = c == 0xd9u ? 1 : (c == 0xdau ? 2 : 4);
        if (!r.need(static_cast<std::size_t>(lb))) break;
        const std::size_t len = static_cast<std::size_t>(r.be(lb));
        if (!r.need(len)) break;
        std::string s(reinterpret_cast<const char*>(r.p + r.pos), len);
        r.pos += len;
        return MsgValue::str(std::move(s));
    }
    case 0xc4u: case 0xc5u: case 0xc6u: { // bin8/16/32
        const int lb = c == 0xc4u ? 1 : (c == 0xc5u ? 2 : 4);
        if (!r.need(static_cast<std::size_t>(lb))) break;
        const std::size_t len = static_cast<std::size_t>(r.be(lb));
        if (!r.need(len)) break;
        std::vector<std::uint8_t> b(r.p + r.pos, r.p + r.pos + len);
        r.pos += len;
        return MsgValue::binary(std::move(b));
    }
    case 0xdcu: case 0xddu: { // array16/32
        const int lb = c == 0xdcu ? 2 : 4;
        if (!r.need(static_cast<std::size_t>(lb))) break;
        const std::size_t len = static_cast<std::size_t>(r.be(lb));
        return decodeArray(r, len, depth);
    }
    case 0xdeu: case 0xdfu: { // map16/32
        const int lb = c == 0xdeu ? 2 : 4;
        if (!r.need(static_cast<std::size_t>(lb))) break;
        const std::size_t len = static_cast<std::size_t>(r.be(lb));
        return decodeMap(r, len, depth);
    }
    default:
        break;
    }
    r.ok = false;
    return MsgValue::nil();
}

inline MsgValue decodeArray(Reader& r, std::size_t len, int depth) {
    std::vector<MsgValue> a;
    a.reserve(len < 4096 ? len : 0);
    for (std::size_t i = 0; i < len && r.ok; ++i) {
        a.push_back(decodeValue(r, depth + 1));
    }
    return MsgValue::array(std::move(a));
}

inline MsgValue decodeMap(Reader& r, std::size_t len, int depth) {
    std::vector<std::pair<MsgValue, MsgValue>> m;
    m.reserve(len < 4096 ? len : 0);
    for (std::size_t i = 0; i < len && r.ok; ++i) {
        MsgValue k = decodeValue(r, depth + 1);
        MsgValue val = decodeValue(r, depth + 1);
        m.emplace_back(std::move(k), std::move(val));
    }
    return MsgValue::map(std::move(m));
}

} // namespace detail

// Encode a value to canonical MessagePack bytes.
inline std::vector<std::uint8_t> msgpackEncode(const MsgValue& v) {
    std::vector<std::uint8_t> out;
    detail::encodeValue(out, v);
    return out;
}

// Decode MessagePack bytes. Returns true and fills `out` on success; false if the bytes are malformed,
// truncated, or contain trailing garbage after one top-level value.
inline bool msgpackDecode(const std::uint8_t* data, std::size_t size, MsgValue& out) {
    detail::Reader r{data, size};
    MsgValue v = detail::decodeValue(r, 0);
    if (!r.ok || r.pos != size) {
        return false;
    }
    out = std::move(v);
    return true;
}

inline bool msgpackDecode(const std::vector<std::uint8_t>& bytes, MsgValue& out) {
    return msgpackDecode(bytes.data(), bytes.size(), out);
}

} // namespace maz::io
