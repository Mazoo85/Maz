#pragma once

#include "maz/core/Utf8.hpp" // utf8Decode / utf8Encode, kReplacementChar

#include <cstdint>
#include <string>

// maz::core UTF-16 <-> code point <-> UTF-8 conversion, the companion to Utf8.hpp. UTF-16 is the native
// text encoding of the Windows API (wide-char file paths, the clipboard, native file dialogs and message
// boxes) and of Java/JavaScript/.NET strings, so an engine that talks to those platforms or imports data
// from them needs to move between UTF-8 (its internal/on-disk form) and UTF-16. The subtlety UTF-16 adds
// over UTF-8 is SURROGATE PAIRS: code points above U+FFFF (emoji, many CJK extensions, historic scripts)
// are stored as TWO 16-bit units — a high surrogate (0xD800..0xDBFF) followed by a low surrogate
// (0xDC00..0xDFFF) — and getting the split/join arithmetic right is exactly where naive code breaks. This
// handles it, and mirrors Utf8.hpp's forgiving policy: any invalid scalar value (a lone surrogate, a value
// above U+10FFFF) becomes U+FFFD, the replacement character, rather than corrupting the stream. Header-only,
// std-only, deterministic. Godot exposes wide-char conversion only through its opaque String; this is the
// standalone codec.
namespace maz::core {

// Encode a code-point string to UTF-16 (surrogate pairs for astral code points). Invalid scalar values
// (surrogate range, > U+10FFFF) are emitted as U+FFFD.
inline std::u16string utf16Encode(const std::u32string& cps) {
    std::u16string out;
    out.reserve(cps.size());
    for (char32_t cp : cps) {
        if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            cp = kReplacementChar;
        }
        if (cp <= 0xFFFFu) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            const char32_t v = cp - 0x10000u;
            out.push_back(static_cast<char16_t>(0xD800u + (v >> 10)));       // high surrogate
            out.push_back(static_cast<char16_t>(0xDC00u + (v & 0x3FFu)));    // low surrogate
        }
    }
    return out;
}

// Decode a UTF-16 unit string to code points. An unpaired or malformed surrogate becomes U+FFFD.
inline std::u32string utf16Decode(const std::u16string& units) {
    std::u32string out;
    out.reserve(units.size());
    const std::size_t n = units.size();
    for (std::size_t i = 0; i < n; ++i) {
        const char16_t u = units[i];
        if (u >= 0xD800u && u <= 0xDBFFu) { // high surrogate: expect a low surrogate next
            if (i + 1 < n) {
                const char16_t lo = units[i + 1];
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    const char32_t cp = 0x10000u
                        + ((static_cast<char32_t>(u) - 0xD800u) << 10)
                        + (static_cast<char32_t>(lo) - 0xDC00u);
                    out.push_back(cp);
                    ++i; // consume the low surrogate
                    continue;
                }
            }
            out.push_back(kReplacementChar); // lone high surrogate
        } else if (u >= 0xDC00u && u <= 0xDFFFu) {
            out.push_back(kReplacementChar); // stray low surrogate
        } else {
            out.push_back(static_cast<char32_t>(u));
        }
    }
    return out;
}

// Convenience: UTF-8 bytes -> UTF-16 units, and back (composed with the Utf8.hpp codec).
inline std::u16string utf8ToUtf16(const std::string& utf8) { return utf16Encode(utf8Decode(utf8)); }
inline std::string utf16ToUtf8(const std::u16string& utf16) { return utf8Encode(utf16Decode(utf16)); }

// Number of UTF-16 code units a code-point string needs (2 for astral code points, 1 otherwise).
inline std::size_t utf16Length(const std::u32string& cps) {
    std::size_t n = 0;
    for (char32_t cp : cps) {
        n += (cp > 0xFFFFu && cp <= 0x10FFFFu) ? 2u : 1u;
    }
    return n;
}

} // namespace maz::core
