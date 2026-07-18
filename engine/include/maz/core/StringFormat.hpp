#pragma once

#include "maz/core/Variant.hpp"
#include "maz/core/VariantContainers.hpp"

#include <cstdint>
#include <string>

// maz::core string formatting — Godot's String.format: substitute {placeholder} tokens in a template
// with values drawn from an Array (positional {0},{1},...) or a Dictionary (named {key}). This is the
// data-driven text layer for localized strings, debug readouts, and templated UI: "Hi {name}, you have
// {0} gold". Values are stringified via Variant (so numbers/vectors/bools format consistently). Unknown
// or malformed placeholders are left verbatim, matching Godot. Header-only, pure, unit-tested.
namespace maz::core {

namespace detail {
// Replace each "{token}" in `fmt` using `resolve(token) -> (matched, replacement)`. A '{' with no
// matching '}' emits the rest literally; an unresolved token is left as "{token}".
template <typename Resolver>
inline std::string formatImpl(const std::string& fmt, Resolver&& resolve) {
    std::string out;
    out.reserve(fmt.size());
    std::size_t i = 0;
    while (i < fmt.size()) {
        if (fmt[i] != '{') {
            out.push_back(fmt[i++]);
            continue;
        }
        const std::size_t close = fmt.find('}', i);
        if (close == std::string::npos) {
            out.append(fmt, i, std::string::npos); // no closer -> rest is literal
            break;
        }
        const std::string token = fmt.substr(i + 1, close - i - 1);
        std::string replacement;
        if (resolve(token, replacement)) {
            out += replacement;
        } else {
            out += "{" + token + "}"; // leave unresolved verbatim
        }
        i = close + 1;
    }
    return out;
}
} // namespace detail

// Fill named placeholders {key} from a Dictionary — Godot's String.format(Dictionary).
inline std::string formatWith(const std::string& fmt, const Dictionary& args) {
    return detail::formatImpl(fmt, [&](const std::string& key, std::string& out) {
        if (!args.has(key)) {
            return false;
        }
        out = args.get(key).stringify();
        return true;
    });
}

// Fill positional placeholders {0},{1},... from an Array — Godot's String.format(Array).
inline std::string formatWith(const std::string& fmt, const Array& args) {
    return detail::formatImpl(fmt, [&](const std::string& key, std::string& out) {
        if (key.empty()) {
            return false;
        }
        for (char c : key) {
            if (c < '0' || c > '9') {
                return false; // not a positional index
            }
        }
        const std::int64_t idx = std::stoll(key);
        if (idx < 0 || static_cast<std::size_t>(idx) >= args.size()) {
            return false;
        }
        out = args[static_cast<std::size_t>(idx)].stringify();
        return true;
    });
}

} // namespace maz::core
