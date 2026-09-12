// tests/core/numberformat_ordinal.cpp — verifies the ordinal + Roman-numeral helpers added to core::NumberFormat
// (ordinalSuffix / ordinal / toRoman). Ground truths: ordinal suffixes follow English rules including the 11/12/13
// "th" exception; ordinal() prepends the number and keeps the sign; toRoman renders 1..3999 with correct additive/
// subtractive form and returns "" outside that range. Values checked against known references. Pure CPU.
#include "maz/core/NumberFormat.hpp"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::core;

int main() {
    // --- 1. Ordinal suffixes: base cases. ---
    {
        CHECK(ordinalSuffix(1) == "st" && ordinalSuffix(2) == "nd" && ordinalSuffix(3) == "rd" &&
              ordinalSuffix(4) == "th", "1st/2nd/3rd/4th base suffixes");
        CHECK(ordinalSuffix(21) == "st" && ordinalSuffix(22) == "nd" && ordinalSuffix(23) == "rd" &&
              ordinalSuffix(24) == "th", "21st/22nd/23rd/24th follow the last digit");
        CHECK(ordinalSuffix(101) == "st" && ordinalSuffix(102) == "nd", "101st/102nd (past a hundred)");
    }

    // --- 2. The teen exception: 11/12/13 (and 111/112/113) are always "th". ---
    {
        CHECK(ordinalSuffix(11) == "th" && ordinalSuffix(12) == "th" && ordinalSuffix(13) == "th",
              "11th/12th/13th are the teen exception");
        CHECK(ordinalSuffix(111) == "th" && ordinalSuffix(112) == "th" && ordinalSuffix(113) == "th",
              "111th/112th/113th also the exception");
        CHECK(ordinalSuffix(0) == "th", "0th");
    }

    // --- 3. ordinal() prepends the value and preserves sign. ---
    {
        CHECK(ordinal(1) == "1st" && ordinal(22) == "22nd" && ordinal(113) == "113th", "ordinal joins number+suffix");
        CHECK(ordinal(-1) == "-1st", "negative keeps its sign, suffix by magnitude");
    }

    // --- 4. Roman numerals: canonical values. ---
    {
        CHECK(toRoman(1) == "I" && toRoman(4) == "IV" && toRoman(9) == "IX", "I / IV / IX");
        CHECK(toRoman(14) == "XIV" && toRoman(40) == "XL" && toRoman(90) == "XC", "XIV / XL / XC");
        CHECK(toRoman(400) == "CD" && toRoman(900) == "CM", "CD / CM subtractive hundreds");
        CHECK(toRoman(2024) == "MMXXIV", "2024 = MMXXIV");
        CHECK(toRoman(1984) == "MCMLXXXIV", "1984 = MCMLXXXIV");
        CHECK(toRoman(3999) == "MMMCMXCIX", "3999 = MMMCMXCIX (maximum)");
    }

    // --- 5. Out-of-range Roman -> empty. ---
    {
        CHECK(toRoman(0).empty() && toRoman(-5).empty() && toRoman(4000).empty(),
              "0, negative, and >3999 return empty");
    }

    if (g_fail == 0) {
        std::printf("numberformat_ordinal: OK — ordinal suffixes incl. teens, signed ordinal, Roman 1..3999, ranges.\n");
        return 0;
    }
    std::printf("numberformat_ordinal: %d failure(s).\n", g_fail);
    return 1;
}
