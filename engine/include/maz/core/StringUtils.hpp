#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// maz::core string utilities — the everyday text helpers Godot's String type bundles (split, join,
// strip_edges, lpad/rpad, replace, begins_with/ends_with, to_lower/upper, repeat, count). Maz already
// has StringId/StringTable for INTERNING; this is the plain manipulation layer that data parsing, UI
// text, save formats, and command handling all reach for. Pure std::string, header-only, deterministic
// — unit-tests exactly. (Locale-aware casing and Godot's quirky capitalize() are out of scope; casing
// here is ASCII.)
namespace maz::core {

// Split `s` on the substring `delim`. Empty pieces are kept unless `allowEmpty` is false. An empty
// delimiter returns the whole string as one element.
inline std::vector<std::string> split(const std::string& s, const std::string& delim,
                                      bool allowEmpty = true) {
    std::vector<std::string> out;
    if (delim.empty()) {
        out.push_back(s);
        return out;
    }
    std::size_t start = 0;
    for (;;) {
        const std::size_t hit = s.find(delim, start);
        if (hit == std::string::npos) {
            std::string piece = s.substr(start);
            if (allowEmpty || !piece.empty()) {
                out.push_back(piece);
            }
            break;
        }
        std::string piece = s.substr(start, hit - start);
        if (allowEmpty || !piece.empty()) {
            out.push_back(piece);
        }
        start = hit + delim.size();
    }
    return out;
}

// Join `parts` with `sep` between them.
inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) {
            out += sep;
        }
        out += parts[i];
    }
    return out;
}

inline bool beginsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
inline bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
inline bool contains(const std::string& s, const std::string& needle) {
    return s.find(needle) != std::string::npos;
}

// Strip leading/trailing ASCII whitespace (Godot strip_edges).
inline std::string lstrip(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    return s.substr(b);
}
inline std::string rstrip(const std::string& s) {
    std::size_t e = s.size();
    while (e > 0 && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(0, e);
}
inline std::string strip(const std::string& s) { return rstrip(lstrip(s)); }

// Left/right pad to `width` with `fill` (Godot lpad/rpad). No-op if already at least `width` long.
inline std::string padLeft(const std::string& s, std::size_t width, char fill = ' ') {
    if (s.size() >= width) {
        return s;
    }
    return std::string(width - s.size(), fill) + s;
}
inline std::string padRight(const std::string& s, std::size_t width, char fill = ' ') {
    if (s.size() >= width) {
        return s;
    }
    return s + std::string(width - s.size(), fill);
}

// Replace every occurrence of `from` with `to` (Godot replace). Empty `from` returns `s` unchanged.
inline std::string replaceAll(const std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return s;
    }
    std::string out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t hit = s.find(from, start);
        if (hit == std::string::npos) {
            out += s.substr(start);
            break;
        }
        out += s.substr(start, hit - start);
        out += to;
        start = hit + from.size();
    }
    return out;
}

inline std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}
inline std::string toUpper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

// Repeat `s` `n` times (Godot repeat).
inline std::string repeat(const std::string& s, std::size_t n) {
    std::string out;
    out.reserve(s.size() * n);
    for (std::size_t i = 0; i < n; ++i) {
        out += s;
    }
    return out;
}

// Count non-overlapping occurrences of `needle` in `s` (Godot count). Empty needle -> 0.
inline std::size_t count(const std::string& s, const std::string& needle) {
    if (needle.empty()) {
        return 0;
    }
    std::size_t n = 0, start = 0;
    for (;;) {
        const std::size_t hit = s.find(needle, start);
        if (hit == std::string::npos) {
            break;
        }
        ++n;
        start = hit + needle.size();
    }
    return n;
}

// ---- number parsing (Godot String's to_int / to_float / is_valid_* / hex_to_int) (M274) --------

// Parse a leading base-10 integer: optional leading whitespace, an optional +/- sign, then digits;
// scanning stops at the first non-digit and trailing junk is ignored (Godot's String.to_int).
// Returns 0 when no digits are present.
inline std::int64_t toInt(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    std::int64_t sign = 1;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        if (s[i] == '-') {
            sign = -1;
        }
        ++i;
    }
    std::int64_t v = 0;
    for (; i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])); ++i) {
        v = v * 10 + (s[i] - '0');
    }
    return sign * v;
}

// True when the ENTIRE string is a base-10 integer: an optional sign followed by one or more digits,
// nothing else (Godot's String.is_valid_int).
inline bool isValidInt(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    std::size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    if (i >= s.size()) {
        return false;
    }
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

// Parse a leading floating-point number (Godot's String.to_float); returns 0.0 when none is present.
inline double toFloat(const std::string& s) {
    const char* p = s.c_str();
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    return end == p ? 0.0 : v;
}

// True when the ENTIRE string (bar trailing whitespace) is a valid float (Godot's is_valid_float).
inline bool isValidFloat(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    const char* p = s.c_str();
    char* end = nullptr;
    std::strtod(p, &end);
    if (end == p) {
        return false;
    }
    while (*end) {
        if (!std::isspace(static_cast<unsigned char>(*end))) {
            return false;
        }
        ++end;
    }
    return true;
}

// Parse a hexadecimal integer with an optional sign and optional "0x"/"0X" prefix; scanning stops at
// the first non-hex-digit (Godot's String.hex_to_int). Returns 0 when no hex digits follow.
inline std::int64_t hexToInt(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    std::int64_t sign = 1;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        if (s[i] == '-') {
            sign = -1;
        }
        ++i;
    }
    if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        i += 2;
    }
    std::int64_t v = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        int d;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else {
            break;
        }
        v = v * 16 + d;
    }
    return sign * v;
}

// ---- file-path helpers (Godot String's get_extension / get_basename / get_file / get_base_dir /
// path_join / simplify_path) (M285) ----------------------------------------------------------------

// The file extension without the dot (Godot's String.get_extension); empty when the final path
// component has no '.'. A leading-dot name like ".gitignore" is treated as all-extension, matching Godot.
inline std::string getExtension(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "";
    }
    const std::size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && dot < slash) {
        return ""; // the '.' belongs to a directory, not the file
    }
    return path.substr(dot + 1);
}

// The path with its extension removed (Godot's String.get_basename): "a/b.txt" -> "a/b".
inline std::string getBasename(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return path;
    }
    const std::size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && dot < slash) {
        return path;
    }
    return path.substr(0, dot);
}

// The final path component (Godot's String.get_file): "a/b/c.txt" -> "c.txt".
inline std::string getFile(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// The directory containing the file, without a trailing slash (Godot's String.get_base_dir):
// "a/b/c.txt" -> "a/b", "/a/b" -> "/a", "c.txt" -> "".
inline std::string getBaseDir(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return "";
    }
    if (slash == 0) {
        return path.substr(0, 1); // keep the root "/"
    }
    return path.substr(0, slash);
}

// Join two path fragments with a single '/' (Godot's String.path_join).
inline std::string pathJoin(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (b.empty()) {
        return a;
    }
    const bool aSlash = a.back() == '/' || a.back() == '\\';
    const bool bSlash = b.front() == '/' || b.front() == '\\';
    if (aSlash && bSlash) {
        return a + b.substr(1);
    }
    if (aSlash || bSlash) {
        return a + b;
    }
    return a + "/" + b;
}

// Collapse redundant separators and resolve "." / ".." segments (Godot's String.simplify_path).
inline std::string simplifyPath(const std::string& path) {
    const bool absolute = !path.empty() && (path.front() == '/' || path.front() == '\\');
    std::vector<std::string> stack;
    std::string seg;
    auto flush = [&]() {
        if (seg.empty() || seg == ".") {
            seg.clear();
            return;
        }
        if (seg == "..") {
            if (!stack.empty() && stack.back() != "..") {
                stack.pop_back();
            } else if (!absolute) {
                stack.push_back("..");
            }
        } else {
            stack.push_back(seg);
        }
        seg.clear();
    };
    for (char c : path) {
        if (c == '/' || c == '\\') {
            flush();
        } else {
            seg.push_back(c);
        }
    }
    flush();
    std::string out;
    for (std::size_t i = 0; i < stack.size(); ++i) {
        if (i) {
            out.push_back('/');
        }
        out += stack[i];
    }
    if (absolute) {
        out = "/" + out;
    }
    if (out.empty()) {
        return absolute ? "/" : ".";
    }
    return out;
}

// ---- case conversion (Godot String's capitalize / to_snake_case / to_camel_case /
// to_pascal_case) (M287) ---------------------------------------------------------------------------

namespace detail {
inline bool isUpperAscii(char c) { return c >= 'A' && c <= 'Z'; }
inline bool isLowerAscii(char c) { return c >= 'a' && c <= 'z'; }
inline bool isDigitAscii(char c) { return c >= '0' && c <= '9'; }
inline bool isAlnumAscii(char c) {
    return isUpperAscii(c) || isLowerAscii(c) || isDigitAscii(c);
}

// Split into words the way Godot's case converters do: runs of non-alphanumeric characters are
// separators, and inside an alphanumeric run a boundary is inserted before an uppercase letter that
// follows a lowercase/digit (aB -> a|B) or that ends an acronym run before a lowercase (ABc -> A|Bc).
// Acronyms are thus treated as a single word ("HTTPServer" -> "HTTP","Server").
inline std::vector<std::string> splitCaseWords(const std::string& s) {
    std::vector<std::string> words;
    std::string cur;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (!isAlnumAscii(c)) {
            if (!cur.empty()) {
                words.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (!cur.empty()) {
            const char prev = cur.back();
            bool boundary = false;
            if (isUpperAscii(c) && (isLowerAscii(prev) || isDigitAscii(prev))) {
                boundary = true;
            } else if (isUpperAscii(c) && isUpperAscii(prev) && i + 1 < s.size() &&
                       isLowerAscii(s[i + 1])) {
                boundary = true;
            }
            if (boundary) {
                words.push_back(cur);
                cur.clear();
            }
        }
        cur.push_back(c);
    }
    if (!cur.empty()) {
        words.push_back(cur);
    }
    return words;
}

inline std::string firstUpperRestLower(const std::string& w) {
    std::string out = toLower(w);
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}
} // namespace detail

// "move_local_x" / "camelCase" -> "Move Local X" / "Camel Case" (Godot's String.capitalize):
// splits into words, lowercases them, then capitalizes each and joins with single spaces.
inline std::string capitalize(const std::string& s) {
    const auto words = detail::splitCaseWords(s);
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i) {
            out += ' ';
        }
        out += detail::firstUpperRestLower(words[i]);
    }
    return out;
}

// "MoveLocalX" / "camelCase" -> "move_local_x" / "camel_case" (Godot's String.to_snake_case):
// lowercase words joined by underscores.
inline std::string toSnakeCase(const std::string& s) {
    const auto words = detail::splitCaseWords(s);
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i) {
            out += '_';
        }
        out += toLower(words[i]);
    }
    return out;
}

// "move_local_x" -> "MoveLocalX" (Godot's String.to_pascal_case): each word capitalized, no
// separators. Acronyms are normalized ("HTTPServer" -> "HttpServer").
inline std::string toPascalCase(const std::string& s) {
    const auto words = detail::splitCaseWords(s);
    std::string out;
    for (const auto& w : words) {
        out += detail::firstUpperRestLower(w);
    }
    return out;
}

// "move_local_x" -> "moveLocalX" (Godot's String.to_camel_case): PascalCase with a lowercase first
// letter.
inline std::string toCamelCase(const std::string& s) {
    std::string out = toPascalCase(s);
    if (!out.empty()) {
        out[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[0])));
    }
    return out;
}

} // namespace maz::core
