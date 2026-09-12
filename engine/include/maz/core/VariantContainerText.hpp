#pragma once

#include "maz/core/VariantContainers.hpp"
#include "maz/core/VariantText.hpp"

#include <optional>
#include <string>
#include <vector>

// maz::core Array/Dictionary text serialization — the container half of Godot's var_to_str /
// str_to_var. Arrays serialize as `[a, b, c]` and Dictionaries as `{ "key": value, ... }` (keys in
// insertion order, matching Godot's ordered dictionaries). Elements are the scalar Variant set
// (M293); nested containers are out of scope (the containers hold scalar Variants). The parser is
// quote- and paren-aware so commas inside quoted strings and inside `Vector2(x, y)` don't split
// elements. Deterministic, header-only; unit-tested by round-trip. (Whitespace may differ from
// Godot's exact bytes; the guarantee is strToArray(arrayToStr(a)) == a and likewise for dictionaries.)
namespace maz::core {

// ---- Array ------------------------------------------------------------------------------------
inline std::string arrayToStr(const Array& a) {
    std::string out = "[";
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (i) {
            out += ", ";
        }
        out += varToStr(a[i]);
    }
    out += "]";
    return out;
}

// ---- Dictionary -------------------------------------------------------------------------------
inline std::string dictToStr(const Dictionary& d) {
    const std::vector<std::string> keys = d.keys();
    if (keys.empty()) {
        return "{}";
    }
    std::string out = "{";
    for (std::size_t i = 0; i < keys.size(); ++i) {
        out += i ? "," : "";
        out += " " + varToStr(Variant(keys[i])) + ": " + varToStr(d.get(keys[i]));
    }
    out += " }";
    return out;
}

namespace detail {
// Split `s` on top-level commas — commas inside quoted strings or inside (), [], {} are ignored.
inline std::vector<std::string> splitTopLevelCommas(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    bool inStr = false;
    int depth = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (inStr) {
            cur.push_back(c);
            if (c == '\\' && i + 1 < s.size()) {
                cur.push_back(s[++i]);
            } else if (c == '"') {
                inStr = false;
            }
            continue;
        }
        if (c == '"') {
            inStr = true;
            cur.push_back(c);
        } else if (c == '(' || c == '[' || c == '{') {
            ++depth;
            cur.push_back(c);
        } else if (c == ')' || c == ']' || c == '}') {
            --depth;
            cur.push_back(c);
        } else if (c == ',' && depth == 0) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty() || !parts.empty()) {
        parts.push_back(cur);
    }
    return parts;
}

// Position of the first top-level ':' (outside quotes / brackets), or npos.
inline std::size_t findTopLevelColon(const std::string& s) {
    bool inStr = false;
    int depth = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (inStr) {
            if (c == '\\') {
                ++i;
            } else if (c == '"') {
                inStr = false;
            }
        } else if (c == '"') {
            inStr = true;
        } else if (c == '(' || c == '[' || c == '{') {
            ++depth;
        } else if (c == ')' || c == ']' || c == '}') {
            --depth;
        } else if (c == ':' && depth == 0) {
            return i;
        }
    }
    return std::string::npos;
}
} // namespace detail

inline std::optional<Array> strToArray(const std::string& text) {
    const std::string s = strip(text);
    if (s.size() < 2 || s.front() != '[' || s.back() != ']') {
        return std::nullopt;
    }
    const std::string inner = strip(s.substr(1, s.size() - 2));
    Array a;
    if (inner.empty()) {
        return a;
    }
    for (const std::string& tok : detail::splitTopLevelCommas(inner)) {
        const std::optional<Variant> v = strToVar(strip(tok));
        if (!v) {
            return std::nullopt;
        }
        a.append(*v);
    }
    return a;
}

inline std::optional<Dictionary> strToDict(const std::string& text) {
    const std::string s = strip(text);
    if (s.size() < 2 || s.front() != '{' || s.back() != '}') {
        return std::nullopt;
    }
    const std::string inner = strip(s.substr(1, s.size() - 2));
    Dictionary d;
    if (inner.empty()) {
        return d;
    }
    for (const std::string& entry : detail::splitTopLevelCommas(inner)) {
        const std::size_t colon = detail::findTopLevelColon(entry);
        if (colon == std::string::npos) {
            return std::nullopt;
        }
        const std::optional<Variant> key = strToVar(strip(entry.substr(0, colon)));
        const std::optional<Variant> val = strToVar(strip(entry.substr(colon + 1)));
        if (!key || !val || key->type() != VariantType::String) {
            return std::nullopt;
        }
        d.set(key->stringify(), *val); // stringify() of a String returns the raw string
    }
    return d;
}

} // namespace maz::core
