#pragma once

// A tiny 5x7 bitmap font for the HUD. The Maz Engine has no text renderer, so DEAD SECTOR bakes
// this font into a single-row RGBA atlas at startup and draws each glyph as a sprite sub-rect.
// Header-only and dependency-free (just the standard library) so it can also be rendered to a PPM
// by a local tool for eyeballing legibility.
//
// Each glyph is 5 wide x 7 tall, expressed as seven 5-character rows ('#' = on). Glyphs are packed
// left-to-right in atlas order; index 0 is space. Unsupported characters fall back to space.

#include <cstddef>
#include <cstdint>

namespace shooter::font {

constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;

struct Glyph {
    char c;
    const char* rows[kGlyphH];
};

// Atlas order. Keep space first so index 0 is a blank cell.
inline const Glyph* glyphs(std::size_t& count) {
    static const Glyph table[] = {
        {' ', {"     ", "     ", "     ", "     ", "     ", "     ", "     "}},
        {'A', {" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
        {'B', {"#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "}},
        {'C', {" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "}},
        {'D', {"#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### "}},
        {'E', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"}},
        {'F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "}},
        {'G', {" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### "}},
        {'H', {"#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
        {'I', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####"}},
        {'J', {"  ###", "   # ", "   # ", "   # ", "#  # ", "#  # ", " ##  "}},
        {'K', {"#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"}},
        {'L', {"#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"}},
        {'M', {"#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #"}},
        {'N', {"#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"}},
        {'O', {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
        {'P', {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "}},
        {'Q', {" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"}},
        {'R', {"#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"}},
        {'S', {" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "}},
        {'T', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "}},
        {'U', {"#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
        {'V', {"#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "}},
        {'W', {"#   #", "#   #", "#   #", "#   #", "# # #", "## ##", "#   #"}},
        {'X', {"#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"}},
        {'Y', {"#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "}},
        {'Z', {"#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"}},
        {'0', {" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "}},
        {'1', {"  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "}},
        {'2', {" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"}},
        {'3', {" ### ", "#   #", "    #", "  ## ", "    #", "#   #", " ### "}},
        {'4', {"   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "}},
        {'5', {"#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "}},
        {'6', {" ### ", "#   #", "#    ", "#### ", "#   #", "#   #", " ### "}},
        {'7', {"#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "}},
        {'8', {" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "}},
        {'9', {" ### ", "#   #", "#   #", " ####", "    #", "#   #", " ### "}},
        {':', {"     ", "  #  ", "  #  ", "     ", "  #  ", "  #  ", "     "}},
        {'-', {"     ", "     ", "     ", "#####", "     ", "     ", "     "}},
        {'+', {"     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     "}},
        {'!', {"  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "     ", "  #  "}},
        {'.', {"     ", "     ", "     ", "     ", "     ", "     ", "  #  "}},
        {'/', {"    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    "}},
    };
    count = sizeof(table) / sizeof(table[0]);
    return table;
}

// Atlas index for a character (uppercased). Returns 0 (space) for anything unsupported.
inline std::size_t indexOf(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    std::size_t count = 0;
    const Glyph* g = glyphs(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (g[i].c == ch) return i;
    }
    return 0;
}

} // namespace shooter::font
