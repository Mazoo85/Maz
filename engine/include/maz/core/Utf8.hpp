#pragma once

#include <cstdint>
#include <string>

// maz::core UTF-8 <-> code-point conversion. Godot's String is fundamentally a sequence of Unicode
// CODE POINTS (stored as UTF-32), with length() counting code points and to_utf8_buffer/parse_utf8
// converting to/from UTF-8 bytes. This is the core layer that gives Maz the same code-point view of
// text: encode a code point (or a whole u32string) to UTF-8, decode UTF-8 to code points, and count
// code points in a UTF-8 string (String.length semantics — NOT the raw byte count). Malformed bytes
// and out-of-range/surrogate code points decode/encode as U+FFFD (the replacement character), so the
// functions never throw and always make progress. Header-only, pure, deterministic; unit-tested by
// round-trip and against known byte sequences.
namespace maz::core {

inline constexpr char32_t kReplacementChar = 0xFFFDu;

// Encode a single code point to 1-4 UTF-8 bytes. Surrogates (U+D800..U+DFFF) and values above
// U+10FFFF are replaced with U+FFFD.
inline std::string utf8EncodeChar(char32_t cp) {
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        cp = kReplacementChar;
    }
    std::string out;
    if (cp < 0x80u) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800u) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000u) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
    return out;
}

// Encode a whole code-point string to UTF-8 (Godot's String -> to_utf8_buffer).
inline std::string utf8Encode(const std::u32string& cps) {
    std::string out;
    out.reserve(cps.size());
    for (char32_t cp : cps) {
        out += utf8EncodeChar(cp);
    }
    return out;
}

// Decode a UTF-8 byte string to code points (Godot's parse_utf8). Malformed lead/continuation bytes
// become a single U+FFFD and decoding resumes at the next byte.
inline std::u32string utf8Decode(const std::string& s) {
    std::u32string out;
    std::size_t i = 0;
    const std::size_t n = s.size();
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        std::size_t extra;
        char32_t minv; // smallest legal value for this length (rejects overlong encodings)
        if (c < 0x80u) {
            out.push_back(static_cast<char32_t>(c));
            ++i;
            continue;
        } else if ((c & 0xE0u) == 0xC0u) {
            cp = c & 0x1Fu;
            extra = 1;
            minv = 0x80u;
        } else if ((c & 0xF0u) == 0xE0u) {
            cp = c & 0x0Fu;
            extra = 2;
            minv = 0x800u;
        } else if ((c & 0xF8u) == 0xF0u) {
            cp = c & 0x07u;
            extra = 3;
            minv = 0x10000u;
        } else {
            out.push_back(kReplacementChar); // stray continuation or invalid lead byte
            ++i;
            continue;
        }
        if (i + extra >= n) {
            out.push_back(kReplacementChar); // truncated sequence: consume the partial tail as one
            i = n;
            continue;
        }
        bool ok = true;
        for (std::size_t k = 1; k <= extra; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0u) != 0x80u) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (!ok || cp < minv || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            out.push_back(kReplacementChar);
            ++i; // resync one byte at a time
            continue;
        }
        out.push_back(cp);
        i += extra + 1;
    }
    return out;
}

// Number of code points in a UTF-8 string (Godot's String.length, which is NOT s.size() in bytes).
// Defined via utf8Decode so it counts identically to the decoded code-point string, including how
// malformed bytes resolve to U+FFFD.
inline std::size_t utf8Length(const std::string& s) { return utf8Decode(s).size(); }

} // namespace maz::core
