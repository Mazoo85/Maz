// Unit tests for maz::core::str — common ASCII, locale-independent string helpers.
// Exercises startsWith/endsWith/contains, the borrowing trim family, split (keepEmpty
// and skipEmpty semantics incl. empty input), join (incl. split round-trip), ASCII
// toLower/toUpper/equalsIgnoreCase, non-overlapping replaceAll, and the whole-string
// parseInt/parseFloat (from_chars, no whitespace leniency, out untouched on failure).
// Pure C++, no GPU/display.

#include "maz/core/StringUtil.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace str = maz::core::str;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- startsWith / endsWith / contains ------------------------------------
    {
        check(str::startsWith("hello", "he") == true, "startsWith(hello,he)");
        check(str::startsWith("he", "hello") == false, "startsWith(he,hello) too-short");
        check(str::startsWith("x", "") == true, "startsWith(x,'') empty prefix");
        check(str::endsWith("hello", "lo") == true, "endsWith(hello,lo)");
        check(str::endsWith("lo", "hello") == false, "endsWith(lo,hello) too-short");
        check(str::contains("hello", "ell") == true, "contains(hello,ell)");
        check(str::contains("hello", "z") == false, "contains(hello,z)");
    }

    // --- trim family (borrowing views) ---------------------------------------
    {
        check(str::trim("  hi  ") == std::string_view("hi"), "trim both sides");
        check(str::trimLeft("  hi") == std::string_view("hi"), "trimLeft");
        check(str::trimRight("hi  ") == std::string_view("hi"), "trimRight");
        check(str::trim("   ") == std::string_view(""), "trim all-whitespace -> empty");
        check(str::trim("") == std::string_view(""), "trim empty -> empty");
        check(str::trim("no-ws") == std::string_view("no-ws"), "trim no-whitespace unchanged");
    }

    // --- split keepEmpty (default) -------------------------------------------
    {
        check(str::split("a,b,c", ',') == std::vector<std::string>({"a", "b", "c"}), "split a,b,c");
        check(str::split("a,,c", ',') == std::vector<std::string>({"a", "", "c"}), "split a,,c keeps empty");
        check(str::split(",a,", ',') == std::vector<std::string>({"", "a", ""}), "split ,a, keeps edges");
        check(str::split("", ',') == std::vector<std::string>({""}), "split '' -> one empty piece");
        check(str::split("abc", ',') == std::vector<std::string>({"abc"}), "split no-delim -> whole");
    }

    // --- split skipEmpty (keepEmpty=false) -----------------------------------
    {
        check(str::split("a,,c", ',', false) == std::vector<std::string>({"a", "c"}), "split a,,c skips empty");
        check(str::split(",a,", ',', false) == std::vector<std::string>({"a"}), "split ,a, skips edges");
        check(str::split("", ',', false) == std::vector<std::string>({}), "split '' skipEmpty -> empty vector");
        check(str::split(",,,", ',', false) == std::vector<std::string>({}), "split ',,,' skipEmpty -> empty vector");
    }

    // --- join ----------------------------------------------------------------
    {
        check(str::join({"a", "b", "c"}, ",") == "a,b,c", "join a,b,c");
        check(str::join({}, ",") == "", "join empty -> ''");
        check(str::join({"solo"}, ",") == "solo", "join single -> element");
        check(str::join({"a", "b"}, " -> ") == "a -> b", "join multi-char sep");
        check(str::join(str::split("a,b,c", ','), ",") == "a,b,c", "join(split) round-trip");
    }

    // --- toLower / toUpper / equalsIgnoreCase --------------------------------
    {
        check(str::toLower("AbC123") == "abc123", "toLower AbC123");
        check(str::toUpper("AbC123") == "ABC123", "toUpper AbC123");
        check(str::toLower("!@# 09") == "!@# 09", "toLower leaves non-alpha");
        check(str::equalsIgnoreCase("Hello", "hELLO") == true, "equalsIgnoreCase Hello/hELLO");
        check(str::equalsIgnoreCase("a", "ab") == false, "equalsIgnoreCase differing sizes");
        check(str::equalsIgnoreCase("abc", "abd") == false, "equalsIgnoreCase differing chars");
        check(str::equalsIgnoreCase("", "") == true, "equalsIgnoreCase empty/empty");
    }

    // --- replaceAll ----------------------------------------------------------
    {
        check(str::replaceAll("aXbXc", "X", "YY") == "aYYbYYc", "replaceAll X->YY");
        check(str::replaceAll("aaa", "aa", "b") == "ba", "replaceAll non-overlapping aa->b");
        check(str::replaceAll("abc", "z", "Q") == "abc", "replaceAll no occurrence");
        check(str::replaceAll("hello", "l", "") == "heo", "replaceAll with empty 'to'");
    }

    // --- parseInt ------------------------------------------------------------
    {
        int v = 0;
        check(str::parseInt("42", v) == true && v == 42, "parseInt 42");
        check(str::parseInt("-7", v) == true && v == -7, "parseInt -7");
        check(str::parseInt("0", v) == true && v == 0, "parseInt 0");

        int sentinel = -999;
        check(str::parseInt("12x", sentinel) == false && sentinel == -999, "parseInt 12x -> false, untouched");
        check(str::parseInt("abc", sentinel) == false && sentinel == -999, "parseInt abc -> false, untouched");
        check(str::parseInt("", sentinel) == false && sentinel == -999, "parseInt '' -> false, untouched");
        check(str::parseInt(" 5", sentinel) == false && sentinel == -999, "parseInt ' 5' -> false, no ws leniency");
    }

    // --- parseFloat ----------------------------------------------------------
    {
        float f = 0.0f;
        check(str::parseFloat("3.5", f) == true && f == 3.5f, "parseFloat 3.5");
        check(str::parseFloat("1e3", f) == true && f == 1000.0f, "parseFloat 1e3");
        check(str::parseFloat("-0.25", f) == true && f == -0.25f, "parseFloat -0.25");

        float sentinel = -1.0f;
        check(str::parseFloat("3.5x", sentinel) == false && sentinel == -1.0f, "parseFloat 3.5x -> false, untouched");
        check(str::parseFloat("", sentinel) == false && sentinel == -1.0f, "parseFloat '' -> false, untouched");
        check(str::parseFloat("abc", sentinel) == false && sentinel == -1.0f, "parseFloat abc -> false, untouched");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
