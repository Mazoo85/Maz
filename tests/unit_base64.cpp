// Unit tests for maz::core base64 + hex codecs (Base64.hpp). Exercises the exact
// RFC 4648 base64 test vectors, decode round-trips, invalid-input rejection
// (bad length / bad char / misplaced padding -> false with out left empty), a
// full 0..255 binary round-trip (no signed-char corruption on high bytes),
// hex encode/decode round-trips incl. case handling, and hex invalid rejection.
// Pure C++, no GPU/display.

#include "maz/core/Base64.hpp"

#include <cstdio>
#include <string>
#include <vector>
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

// Bytes from a string literal (its chars, not including a NUL terminator).
std::vector<std::uint8_t> bytes(std::string_view s) {
    std::vector<std::uint8_t> v;
    v.reserve(s.size());
    for (char c : s) {
        v.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(c)));
    }
    return v;
}

} // namespace

int main() {
    // --- base64 RFC 4648 vectors (EXACT) -------------------------------------
    {
        check(base64Encode(bytes("")) == "", "b64 encode '' -> ''");
        check(base64Encode(bytes("f")) == "Zg==", "b64 encode f -> Zg==");
        check(base64Encode(bytes("fo")) == "Zm8=", "b64 encode fo -> Zm8=");
        check(base64Encode(bytes("foo")) == "Zm9v", "b64 encode foo -> Zm9v");
        check(base64Encode(bytes("foob")) == "Zm9vYg==", "b64 encode foob -> Zm9vYg==");
        check(base64Encode(bytes("fooba")) == "Zm9vYmE=", "b64 encode fooba -> Zm9vYmE=");
        check(base64Encode(bytes("foobar")) == "Zm9vYmFy", "b64 encode foobar -> Zm9vYmFy");
    }

    // --- base64 decode round-trip --------------------------------------------
    {
        const char* vectors[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
        for (const char* s : vectors) {
            const std::vector<std::uint8_t> orig = bytes(s);
            const std::string enc = base64Encode(orig);
            std::vector<std::uint8_t> dec;
            const bool ok = base64Decode(enc, dec);
            std::string label = std::string("b64 round-trip '") + s + "'";
            check(ok && dec == orig, label.c_str());
        }
        std::vector<std::uint8_t> dec;
        check(base64Decode("Zm9vYmFy", dec) && dec == bytes("foobar"),
              "b64 decode literal Zm9vYmFy -> foobar");
    }

    // --- base64 decode invalid -----------------------------------------------
    {
        std::vector<std::uint8_t> out{0x11, 0x22};  // pre-fill: must be cleared
        check(base64Decode("Zm9v!", out) == false && out.empty(),
              "b64 decode 'Zm9v!' -> false, out empty");
        out = {0x11};
        check(base64Decode("Zg=", out) == false && out.empty(),
              "b64 decode 'Zg=' (len%4) -> false, out empty");
        out = {0x11};
        check(base64Decode("Z===", out) == false && out.empty(),
              "b64 decode 'Z===' (misplaced padding) -> false, out empty");
        out = {0x11};
        check(base64Decode("Zm=9", out) == false && out.empty(),
              "b64 decode 'Zm=9' ('=' not at end) -> false, out empty");
        // A high-bit (>127) byte must be rejected without an out-of-bounds lookup —
        // pins the unsigned-char indexing of the 256-entry reverse table.
        out = {0x11};
        {
            std::string hi = "Zm9";
            hi.push_back(static_cast<char>(0x80));
            check(base64Decode(hi, out) == false && out.empty(),
                  "b64 decode with a 0x80 byte -> false, out empty (no OOB)");
        }
    }

    // --- base64 binary round-trip (0..255) -----------------------------------
    {
        std::vector<std::uint8_t> v;
        for (int i = 0; i < 256; ++i) { v.push_back(static_cast<std::uint8_t>(i)); }
        const std::string enc = base64Encode(v);
        std::vector<std::uint8_t> dec;
        const bool ok = base64Decode(enc, dec);
        check(ok && dec == v, "b64 round-trip all bytes 0..255 (no high-bit corruption)");
    }

    // --- base64 empty --------------------------------------------------------
    {
        check(base64Encode({}) == "", "b64 encode {} -> ''");
        std::vector<std::uint8_t> out{0x11};
        check(base64Decode("", out) == true && out.empty(),
              "b64 decode '' -> true, out empty");
    }

    // --- hexEncode -----------------------------------------------------------
    {
        check(hexEncode({0x00, 0x0f, 0xff, 0xa5}) == "000fffa5", "hexEncode lower");
        check(hexEncode({0x00, 0x0f, 0xff, 0xa5}, true) == "000FFFA5", "hexEncode upper");
        check(hexEncode({}) == "", "hexEncode {} -> ''");
    }

    // --- hexDecode round-trip + case -----------------------------------------
    {
        std::vector<std::uint8_t> dec;
        check(hexDecode("616263", dec) && dec == bytes("abc"), "hexDecode 616263 -> abc");
        check(hexDecode("414243", dec) && dec == bytes("ABC"), "hexDecode 414243 -> ABC");
        check(hexDecode("aAbB", dec) && dec == std::vector<std::uint8_t>({0xAA, 0xBB}),
              "hexDecode mixed case aAbB");

        std::vector<std::uint8_t> v;
        for (int i = 0; i < 256; ++i) { v.push_back(static_cast<std::uint8_t>(i)); }
        std::vector<std::uint8_t> rt;
        check(hexDecode(hexEncode(v), rt) && rt == v, "hex round-trip all bytes 0..255");
    }

    // --- hexDecode invalid ---------------------------------------------------
    {
        std::vector<std::uint8_t> out{0x11};
        check(hexDecode("abc", out) == false && out.empty(),
              "hexDecode 'abc' (odd length) -> false, out empty");
        out = {0x11};
        check(hexDecode("6z", out) == false && out.empty(),
              "hexDecode '6z' (invalid char) -> false, out empty");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
