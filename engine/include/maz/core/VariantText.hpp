#pragma once

#include "maz/core/StringUtils.hpp" // cEscape/cUnescape, split, strip, isValidInt/Float, toInt/toFloat
#include "maz/core/Variant.hpp"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

// maz::core Variant text serialization — Godot's var_to_str / str_to_var: a round-trippable text
// encoding of a Variant (the form Godot writes into .tres/.tscn and returns from var_to_str). Unlike
// Variant::stringify() (the human-facing str() form), this keeps enough type information to parse the
// value straight back: strings are quoted and escaped, floats always carry a decimal marker so they
// don't read back as ints, and vectors use Godot's `Vector2(x, y)` / `Vector3(x, y, z)` constructor
// syntax. Covers the Variant type set (Nil/Bool/Int/Float/String/Vector2/Vector3). Deterministic,
// header-only; unit-tested by round-trip. (Whitespace/precision may differ from Godot's exact bytes;
// the guarantee is that strToVar(varToStr(v)) == v.)
namespace maz::core {

namespace detail {
// Shortest %g representation that parses back to exactly `v` (so floats stay compact but exact).
inline std::string shortestG(double v) {
    char buf[64];
    for (int prec = 6; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (std::strtod(buf, nullptr) == v) {
            break;
        }
    }
    return std::string(buf);
}
// Float token: shortest form, guaranteed to carry a '.'/'e' marker so it never reads back as an int.
inline std::string floatToken(double v) {
    std::string s = shortestG(v);
    if (s.find_first_of(".eEnN") == std::string::npos) {
        s += ".0";
    }
    return s;
}
} // namespace detail

// Variant -> round-trippable text (Godot's var_to_str).
inline std::string varToStr(const Variant& v) {
    switch (v.type()) {
    case VariantType::Nil:
        return "null";
    case VariantType::Bool:
        return v.asBool() ? "true" : "false";
    case VariantType::Int:
        return std::to_string(v.asInt());
    case VariantType::Float:
        return detail::floatToken(v.asFloat());
    case VariantType::String:
        return "\"" + cEscape(v.stringify()) + "\""; // stringify() returns the raw string here
    case VariantType::Vector2: {
        const auto p = v.asVector2();
        return "Vector2(" + detail::shortestG(p.x) + ", " + detail::shortestG(p.y) + ")";
    }
    case VariantType::Vector3: {
        const auto p = v.asVector3();
        return "Vector3(" + detail::shortestG(p.x) + ", " + detail::shortestG(p.y) + ", " +
               detail::shortestG(p.z) + ")";
    }
    }
    return "null";
}

namespace detail {
// Parse exactly `n` comma-separated float components from a "Name(...)" body; false if malformed.
inline bool parseComponents(const std::string& s, std::size_t prefixLen, int n, float* out) {
    if (s.back() != ')') {
        return false;
    }
    const std::string body = s.substr(prefixLen, s.size() - prefixLen - 1);
    const std::vector<std::string> parts = split(body, ",");
    if (static_cast<int>(parts.size()) != n) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        const std::string tok = strip(parts[static_cast<std::size_t>(i)]);
        if (!isValidFloat(tok) && !isValidInt(tok)) {
            return false;
        }
        out[i] = static_cast<float>(toFloat(tok));
    }
    return true;
}
} // namespace detail

// Text -> Variant (Godot's str_to_var). Returns nullopt on malformed input.
inline std::optional<Variant> strToVar(const std::string& text) {
    const std::string s = strip(text);
    if (s.empty()) {
        return std::nullopt;
    }
    if (s == "null") {
        return Variant();
    }
    if (s == "true") {
        return Variant(true);
    }
    if (s == "false") {
        return Variant(false);
    }
    if (s.front() == '"') {
        if (s.size() < 2 || s.back() != '"') {
            return std::nullopt;
        }
        const std::string inner = s.substr(1, s.size() - 2);
        // Reject an unescaped quote inside (would mean the string ended early / is malformed).
        for (std::size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '\\') {
                ++i;
                continue;
            }
            if (inner[i] == '"') {
                return std::nullopt;
            }
        }
        return Variant(cUnescape(inner));
    }
    if (s.rfind("Vector2(", 0) == 0) {
        float c[2];
        if (detail::parseComponents(s, 8, 2, c)) {
            return Variant(math::vec2(c[0], c[1]));
        }
        return std::nullopt;
    }
    if (s.rfind("Vector3(", 0) == 0) {
        float c[3];
        if (detail::parseComponents(s, 8, 3, c)) {
            return Variant(math::vec3(c[0], c[1], c[2]));
        }
        return std::nullopt;
    }
    if (isValidInt(s)) {
        return Variant(toInt(s));
    }
    if (isValidFloat(s)) {
        return Variant(toFloat(s));
    }
    return std::nullopt;
}

} // namespace maz::core
