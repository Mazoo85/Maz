#pragma once

#include "maz/render/ColorOps.hpp" // Color, fromHtml, color8
#include "maz/render/Renderer.hpp" // render::Color

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

// maz::render named colours — Godot's Color constants (Color.RED, Color.SKY_BLUE, ...) and
// Color.from_string(). Godot's constant set is the CSS3 / X11 named-colour palette; this reproduces
// it exactly (byte-for-byte sRGB), so a colour authored by name in Godot resolves identically here.
// Name lookup is forgiving the way Godot's is: case-insensitive, and underscores/spaces are ignored
// ("SKY_BLUE", "sky blue" and "skyblue" all match). Header-only, pure, unit-tested against the palette.
namespace maz::render {

namespace detail {
struct NamedColorEntry {
    const char* name; // lowercase, no separators
    std::uint32_t rgba; // 0xRRGGBBAA
};

// The CSS3/Godot named-colour table (names already normalized: lowercase, no separators). Alpha is
// 0xFF for every colour except TRANSPARENT, which Godot defines as white with zero alpha.
inline const NamedColorEntry* namedColorTable(std::size_t& count) {
    static const NamedColorEntry kTable[] = {
        {"aliceblue", 0xF0F8FFFF},      {"antiquewhite", 0xFAEBD7FF},
        {"aqua", 0x00FFFFFF},           {"aquamarine", 0x7FFFD4FF},
        {"azure", 0xF0FFFFFF},          {"beige", 0xF5F5DCFF},
        {"bisque", 0xFFE4C4FF},         {"black", 0x000000FF},
        {"blanchedalmond", 0xFFEBCDFF}, {"blue", 0x0000FFFF},
        {"blueviolet", 0x8A2BE2FF},     {"brown", 0xA52A2AFF},
        {"burlywood", 0xDEB887FF},      {"cadetblue", 0x5F9EA0FF},
        {"chartreuse", 0x7FFF00FF},     {"chocolate", 0xD2691EFF},
        {"coral", 0xFF7F50FF},          {"cornflowerblue", 0x6495EDFF},
        {"cornsilk", 0xFFF8DCFF},       {"crimson", 0xDC143CFF},
        {"cyan", 0x00FFFFFF},           {"darkblue", 0x00008BFF},
        {"darkcyan", 0x008B8BFF},       {"darkgoldenrod", 0xB8860BFF},
        {"darkgray", 0xA9A9A9FF},       {"darkgreen", 0x006400FF},
        {"darkkhaki", 0xBDB76BFF},      {"darkmagenta", 0x8B008BFF},
        {"darkolivegreen", 0x556B2FFF}, {"darkorange", 0xFF8C00FF},
        {"darkorchid", 0x9932CCFF},     {"darkred", 0x8B0000FF},
        {"darksalmon", 0xE9967AFF},     {"darkseagreen", 0x8FBC8FFF},
        {"darkslateblue", 0x483D8BFF},  {"darkslategray", 0x2F4F4FFF},
        {"darkturquoise", 0x00CED1FF},  {"darkviolet", 0x9400D3FF},
        {"deeppink", 0xFF1493FF},       {"deepskyblue", 0x00BFFFFF},
        {"dimgray", 0x696969FF},        {"dodgerblue", 0x1E90FFFF},
        {"firebrick", 0xB22222FF},      {"floralwhite", 0xFFFAF0FF},
        {"forestgreen", 0x228B22FF},    {"fuchsia", 0xFF00FFFF},
        {"gainsboro", 0xDCDCDCFF},      {"ghostwhite", 0xF8F8FFFF},
        {"gold", 0xFFD700FF},           {"goldenrod", 0xDAA520FF},
        {"gray", 0xBEBEBEFF},           {"green", 0x00FF00FF},
        {"greenyellow", 0xADFF2FFF},    {"honeydew", 0xF0FFF0FF},
        {"hotpink", 0xFF69B4FF},        {"indianred", 0xCD5C5CFF},
        {"indigo", 0x4B0082FF},         {"ivory", 0xFFFFF0FF},
        {"khaki", 0xF0E68CFF},          {"lavender", 0xE6E6FAFF},
        {"lavenderblush", 0xFFF0F5FF},  {"lawngreen", 0x7CFC00FF},
        {"lemonchiffon", 0xFFFACDFF},   {"lightblue", 0xADD8E6FF},
        {"lightcoral", 0xF08080FF},     {"lightcyan", 0xE0FFFFFF},
        {"lightgoldenrod", 0xFAFAD2FF}, {"lightgray", 0xD3D3D3FF},
        {"lightgreen", 0x90EE90FF},     {"lightpink", 0xFFB6C1FF},
        {"lightsalmon", 0xFFA07AFF},    {"lightseagreen", 0x20B2AAFF},
        {"lightskyblue", 0x87CEFAFF},   {"lightslategray", 0x778899FF},
        {"lightsteelblue", 0xB0C4DEFF}, {"lightyellow", 0xFFFFE0FF},
        {"lime", 0x00FF00FF},           {"limegreen", 0x32CD32FF},
        {"linen", 0xFAF0E6FF},          {"magenta", 0xFF00FFFF},
        {"maroon", 0xB03060FF},         {"mediumaquamarine", 0x66CDAAFF},
        {"mediumblue", 0x0000CDFF},     {"mediumorchid", 0xBA55D3FF},
        {"mediumpurple", 0x9370DBFF},   {"mediumseagreen", 0x3CB371FF},
        {"mediumslateblue", 0x7B68EEFF},{"mediumspringgreen", 0x00FA9AFF},
        {"mediumturquoise", 0x48D1CCFF},{"mediumvioletred", 0xC71585FF},
        {"midnightblue", 0x191970FF},   {"mintcream", 0xF5FFFAFF},
        {"mistyrose", 0xFFE4E1FF},      {"moccasin", 0xFFE4B5FF},
        {"navajowhite", 0xFFDEADFF},    {"navyblue", 0x000080FF},
        {"oldlace", 0xFDF5E6FF},        {"olive", 0x808000FF},
        {"olivedrab", 0x6B8E23FF},      {"orange", 0xFFA500FF},
        {"orangered", 0xFF4500FF},      {"orchid", 0xDA70D6FF},
        {"palegoldenrod", 0xEEE8AAFF},  {"palegreen", 0x98FB98FF},
        {"paleturquoise", 0xAFEEEEFF},  {"palevioletred", 0xDB7093FF},
        {"papayawhip", 0xFFEFD5FF},     {"peachpuff", 0xFFDAB9FF},
        {"peru", 0xCD853FFF},           {"pink", 0xFFC0CBFF},
        {"plum", 0xDDA0DDFF},           {"powderblue", 0xB0E0E6FF},
        {"purple", 0xA020F0FF},         {"rebeccapurple", 0x663399FF},
        {"red", 0xFF0000FF},            {"rosybrown", 0xBC8F8FFF},
        {"royalblue", 0x4169E1FF},      {"saddlebrown", 0x8B4513FF},
        {"salmon", 0xFA8072FF},         {"sandybrown", 0xF4A460FF},
        {"seagreen", 0x2E8B57FF},       {"seashell", 0xFFF5EEFF},
        {"sienna", 0xA0522DFF},         {"silver", 0xC0C0C0FF},
        {"skyblue", 0x87CEEBFF},        {"slateblue", 0x6A5ACDFF},
        {"slategray", 0x708090FF},      {"snow", 0xFFFAFAFF},
        {"springgreen", 0x00FF7FFF},    {"steelblue", 0x4682B4FF},
        {"tan", 0xD2B48CFF},            {"teal", 0x008080FF},
        {"thistle", 0xD8BFD8FF},        {"tomato", 0xFF6347FF},
        {"transparent", 0xFFFFFF00},    {"turquoise", 0x40E0D0FF},
        {"violet", 0xEE82EEFF},         {"webgray", 0x808080FF},
        {"webgreen", 0x008000FF},       {"webmaroon", 0x800000FF},
        {"webpurple", 0x800080FF},      {"wheat", 0xF5DEB3FF},
        {"white", 0xFFFFFFFF},          {"whitesmoke", 0xF5F5F5FF},
        {"yellow", 0xFFFF00FF},         {"yellowgreen", 0x9ACD32FF},
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

// Normalize a colour name the way Godot's lookup does: lowercase, drop spaces and underscores.
inline std::string normalizeColorName(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == ' ' || ch == '_') {
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

inline Color fromRgba8888(std::uint32_t v) {
    return color8(static_cast<int>((v >> 24) & 0xFF), static_cast<int>((v >> 16) & 0xFF),
                  static_cast<int>((v >> 8) & 0xFF), static_cast<int>(v & 0xFF));
}
} // namespace detail

// Resolve a Godot/CSS colour name (case-insensitive, separators ignored) to a Color. Returns
// std::nullopt when the name is unknown — Godot's ColorNames lookup.
inline std::optional<Color> namedColor(const std::string& name) {
    const std::string key = detail::normalizeColorName(name);
    std::size_t count = 0;
    const detail::NamedColorEntry* table = detail::namedColorTable(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (key == table[i].name) {
            return detail::fromRgba8888(table[i].rgba);
        }
    }
    return std::nullopt;
}

// Godot's Color.from_string(str, default): parse `str` as either a named colour or an HTML hex code
// ("#rrggbb", "rrggbbaa", etc.); return `fallback` if neither parses.
inline Color colorFromString(const std::string& str, const Color& fallback) {
    if (auto c = namedColor(str)) {
        return *c;
    }
    Color out;
    if (fromHtml(str, out)) {
        return out;
    }
    return fallback;
}

} // namespace maz::render
