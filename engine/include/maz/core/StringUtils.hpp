#pragma once

#include <cctype>
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

} // namespace maz::core
