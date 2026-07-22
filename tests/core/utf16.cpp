// tests/core/utf16.cpp — verifies UTF-16 conversion (core Utf16.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * known encodings: ASCII, Latin-1, BMP (euro), and an astral emoji as a surrogate pair; U+10FFFF as
//     the maximum surrogate pair;
//   * every valid scalar value U+0000..U+10FFFF (excluding the surrogate block) round-trips through
//     utf16Encode -> utf16Decode unchanged;
//   * lone/stray surrogates and out-of-range code points map to U+FFFD;
//   * utf8ToUtf16 -> utf16ToUtf8 round-trips a mixed multilingual string;
//   * utf16Length matches the encoded unit count.
#include "maz/core/Utf16.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

using maz::core::kReplacementChar;
using maz::core::utf16Decode;
using maz::core::utf16Encode;
using maz::core::utf16Length;
using maz::core::utf16ToUtf8;
using maz::core::utf8ToUtf16;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    // --- 1. Known encodings. ---
    {
        CHECK(utf16Encode(U"A") == std::u16string({0x0041}), "ASCII 'A'");
        CHECK(utf16Encode(std::u32string(1, 0x00E9)) == std::u16string({0x00E9}), "Latin-1 e-acute");
        CHECK(utf16Encode(std::u32string(1, 0x20AC)) == std::u16string({0x20AC}), "BMP euro sign");
        // U+1F600 grinning face -> D83D DE00
        CHECK(utf16Encode(std::u32string(1, 0x1F600)) == std::u16string({0xD83D, 0xDE00}),
              "astral emoji is a surrogate pair");
        // U+10FFFF (max) -> DBFF DFFF
        CHECK(utf16Encode(std::u32string(1, 0x10FFFF)) == std::u16string({0xDBFF, 0xDFFF}),
              "max code point surrogate pair");
    }

    // --- 2. Exhaustive round-trip over all valid scalar values. ---
    {
        bool ok = true;
        long count = 0;
        for (char32_t cp = 0; cp <= 0x10FFFFu; ++cp) {
            if (cp >= 0xD800u && cp <= 0xDFFFu) continue; // surrogate block: not scalar values
            const std::u32string one(1, cp);
            const std::u16string enc = utf16Encode(one);
            if (utf16Length(one) != enc.size()) { ok = false; break; }
            if (utf16Decode(enc) != one) { ok = false; break; }
            ++count;
        }
        CHECK(ok, "every valid code point round-trips through UTF-16");
        CHECK(count > 1000000, "the sweep covered the whole Unicode range");
    }

    // --- 3. Invalid inputs map to U+FFFD. ---
    {
        // Encoding a surrogate code point or an out-of-range value yields the replacement char.
        CHECK(utf16Encode(std::u32string(1, 0xD800)) == std::u16string({static_cast<char16_t>(kReplacementChar)}),
              "encoding a lone surrogate scalar -> U+FFFD");
        CHECK(utf16Encode(std::u32string(1, 0x110000)) == std::u16string({static_cast<char16_t>(kReplacementChar)}),
              "encoding an above-range code point -> U+FFFD");
        // Decoding a lone high surrogate, a stray low surrogate, and a high not followed by a low.
        CHECK(utf16Decode(std::u16string({0xD83D})) == std::u32string(1, kReplacementChar),
              "lone high surrogate decodes to U+FFFD");
        CHECK(utf16Decode(std::u16string({0xDE00})) == std::u32string(1, kReplacementChar),
              "stray low surrogate decodes to U+FFFD");
        CHECK(utf16Decode(std::u16string({0xD83D, 0x0041})) == std::u32string({kReplacementChar, 0x0041}),
              "high surrogate + non-surrogate -> U+FFFD then the char");
    }

    // --- 4. UTF-8 <-> UTF-16 convenience round-trip on a mixed string. ---
    {
        // "Hi €馬😀" — ASCII, BMP euro, CJK 'horse', astral emoji.
        const std::u32string cps = U"Hi €馬\U0001F600";
        const std::string utf8 = maz::core::utf8Encode(cps);
        const std::u16string u16 = utf8ToUtf16(utf8);
        CHECK(utf16ToUtf8(u16) == utf8, "utf8 -> utf16 -> utf8 round-trips a multilingual string");
        // The astral emoji must have produced a surrogate pair, so unit count > code-point count.
        CHECK(u16.size() == utf16Length(cps) && u16.size() > cps.size(), "surrogate pair grows unit count");
    }

    if (g_fail == 0) {
        std::printf("utf16: OK — known encodings, full-range round-trip, replacement policy, utf8 bridge.\n");
        return 0;
    }
    std::printf("utf16: %d failure(s).\n", g_fail);
    return 1;
}
