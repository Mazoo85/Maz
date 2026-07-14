#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
#include <cctype>
#include <charconv>

#include "maz/core/Assert.hpp"

// Common ASCII, locale-INDEPENDENT string helpers in maz::core::str — split/join,
// trim (borrowing views), startsWith/endsWith/contains, toLower/toUpper/equalsIgnoreCase
// (ASCII case only), replaceAll, and parseInt/parseFloat (std::from_chars,
// whole-string-must-parse, no whitespace leniency). trim* return string_views INTO the
// argument (they borrow — the returned view is valid only while the source's underlying
// storage lives); everything else returns owning strings. NOT Unicode-aware (byte/ASCII
// operations only). A nested namespace so generic names like `contains`/`trim` don't
// pollute maz::core.
// Future: Unicode-aware case/trim, printf-style format.

namespace maz::core::str {

// --- PREDICATES ----------------------------------------------------------------

inline bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

inline bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

// Empty sub -> find returns 0 -> true (the std convention).
inline bool contains(std::string_view s, std::string_view sub) {
    return s.find(sub) != std::string_view::npos;
}

// --- TRIM (borrowing views) ----------------------------------------------------

// True for the 6 ASCII whitespace chars (space \t \n \r \f \v). Casts to unsigned char
// first so bytes >= 0x80 are well-defined for std::isspace.
inline bool isSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

// The returned view points INTO `s` — valid only while s's storage lives.
inline std::string_view trimLeft(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && isSpace(s[i])) { ++i; }
    return s.substr(i);
}

// The returned view points INTO `s` — valid only while s's storage lives.
inline std::string_view trimRight(std::string_view s) {
    std::size_t n = s.size();
    while (n > 0 && isSpace(s[n - 1])) { --n; }
    return s.substr(0, n);
}

// All-whitespace -> empty view; empty input -> empty. Borrows from `s`.
inline std::string_view trim(std::string_view s) {
    return trimLeft(trimRight(s));
}

// --- SPLIT / JOIN --------------------------------------------------------------

// Split on each delim occurrence into owning strings. N delimiters -> N+1 pieces before
// the keepEmpty filter (the final piece after the last delim is always emitted). If
// keepEmpty is false, empty pieces (consecutive/leading/trailing delimiters) are skipped.
// An empty input with keepEmpty yields a single empty string (zero delimiters -> one
// piece); with keepEmpty=false it yields an empty vector.
inline std::vector<std::string> split(std::string_view s, char delim, bool keepEmpty = true) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = s.find(delim, start);
        std::string_view piece = (pos == std::string_view::npos)
                                     ? s.substr(start)
                                     : s.substr(start, pos - start);
        if (keepEmpty || !piece.empty()) {
            out.emplace_back(piece);
        }
        if (pos == std::string_view::npos) { break; }
        start = pos + 1;
    }
    return out;
}

// Concatenate parts with sep between them (no trailing sep). Empty vector -> "".
inline std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    std::string out;
    if (parts.empty()) { return out; }
    std::size_t total = 0;
    for (const std::string& p : parts) { total += p.size(); }
    total += sep.size() * (parts.size() - 1);
    out.reserve(total);
    out += parts.front();
    for (std::size_t i = 1; i < parts.size(); ++i) {
        out += sep;
        out += parts[i];
    }
    return out;
}

// --- CASE (ASCII only) ---------------------------------------------------------

inline std::string toLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        out.push_back((uc >= 'A' && uc <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

inline std::string toUpper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        out.push_back((uc >= 'a' && uc <= 'z') ? static_cast<char>(c - 'a' + 'A') : c);
    }
    return out;
}

// Sizes equal AND each char equal under ASCII lower-fold. Does not allocate.
inline bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if (ca >= 'A' && ca <= 'Z') { ca = static_cast<unsigned char>(ca - 'A' + 'a'); }
        if (cb >= 'A' && cb <= 'Z') { cb = static_cast<unsigned char>(cb - 'A' + 'a'); }
        if (ca != cb) { return false; }
    }
    return true;
}

// --- REPLACE -------------------------------------------------------------------

// Replace every non-overlapping occurrence of `from` with `to`. Advances past each
// replacement by from.size() so replacements are never re-scanned. No occurrence ->
// copy of s. `from` must be non-empty (an empty `from` would loop forever).
inline std::string replaceAll(std::string_view s, std::string_view from, std::string_view to) {
    MAZ_ASSERT(!from.empty(), "str::replaceAll: empty 'from'");
    std::string out;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = s.find(from, start);
        if (pos == std::string_view::npos) {
            out += s.substr(start);
            break;
        }
        out += s.substr(start, pos - start);
        out += to;
        start = pos + from.size();
    }
    return out;
}

// --- PARSE (std::from_chars — locale-independent, whole string must parse) ------

// The WHOLE string must be consumed, so trailing garbage fails. No whitespace/sign-prefix
// leniency (from_chars does not skip leading whitespace) — trim first if you want that.
// "42"->true/42; "-7"->true/-7; "12x"->false; "abc"->false; ""->false; " 5"->false.
// `out` is left untouched on failure.
inline bool parseInt(std::string_view s, int& out) {
    if (s.empty()) { return false; }
    int v;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && ptr == s.data() + s.size()) {
        out = v;
        return true;
    }
    return false;
}

// Same whole-string-must-parse contract as parseInt, via the floating-point from_chars
// overload. "3.5"->true/3.5; "1e3"->true/1000; "3.5x"->false; ""->false. `out` untouched
// on failure.
inline bool parseFloat(std::string_view s, float& out) {
    if (s.empty()) { return false; }
    float v;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && ptr == s.data() + s.size()) {
        out = v;
        return true;
    }
    return false;
}

} // namespace maz::core::str
