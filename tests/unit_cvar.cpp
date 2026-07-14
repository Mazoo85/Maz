// Unit tests for maz::core::CVarRegistry — the typed runtime console-variable /
// settings registry. Exercises register+defaults, type-checked get/set,
// wrong-type/missing safety, reset-to-default, setFromString parsing (incl.
// parse failures leaving the value unchanged), change callbacks firing only on
// an actual value change, typeOf, and clear. Pure C++, no GPU/display. Float
// asserts use only exactly-representable values.

#include "maz/core/CVar.hpp"

#include <cstdio>
#include <string>

using namespace maz::core;
using maz::core::operator""_sid;

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
    // --- REGISTER + DEFAULTS -------------------------------------------------
    {
        CVarRegistry r;
        r.registerBool("g.fullscreen"_sid, true);
        r.registerInt("g.width"_sid, 1280);
        r.registerFloat("g.gamma"_sid, 2.0);
        r.registerString("g.name"_sid, "maz");
        check(r.getBool("g.fullscreen"_sid) == true, "bool default");
        check(r.getInt("g.width"_sid) == 1280, "int default");
        check(r.getFloat("g.gamma"_sid) == 2.0, "float default");
        check(r.getString("g.name"_sid) == "maz", "string default");
        check(r.count() == 4, "count == 4 after four registrations");
        check(r.has("g.width"_sid) && !r.has("g.missing"_sid), "has present/absent");
    }

    // --- SET + GET -----------------------------------------------------------
    {
        CVarRegistry r;
        r.registerBool("b"_sid, true);
        r.registerInt("i"_sid, 1280);
        r.registerFloat("f"_sid, 2.0);
        r.registerString("s"_sid, "maz");
        check(r.setBool("b"_sid, false) && r.getBool("b"_sid) == false, "setBool false");
        check(r.setInt("i"_sid, 1920) && r.getInt("i"_sid) == 1920, "setInt 1920");
        check(r.setFloat("f"_sid, 0.5) && r.getFloat("f"_sid) == 0.5, "setFloat 0.5");
        check(r.setString("s"_sid, "engine") && r.getString("s"_sid) == "engine", "setString engine");
    }

    // --- TYPE SAFETY ---------------------------------------------------------
    {
        CVarRegistry r;
        r.registerBool("g.fullscreen"_sid, true);
        r.registerInt("i"_sid, 1920);
        check(r.getInt("g.fullscreen"_sid, -1) == -1, "getInt on bool cvar -> fallback");
        check(!r.setString("i"_sid, "x") && r.getInt("i"_sid) == 1920, "setString on int cvar -> false, unchanged");
        check(r.getBool("nope"_sid, true) == true, "getBool on missing -> fallback");
        check(!r.setInt("nope"_sid, 5), "setInt on missing -> false");
    }

    // --- RESET ---------------------------------------------------------------
    {
        CVarRegistry r;
        r.registerInt("i"_sid, 1280);
        check(r.setInt("i"_sid, 99) && r.getInt("i"_sid) == 99, "setInt 99");
        check(r.reset("i"_sid) && r.getInt("i"_sid) == 1280, "reset -> default 1280");
        check(!r.reset("nope"_sid), "reset on missing -> false");
    }

    // --- SET FROM STRING -----------------------------------------------------
    {
        CVarRegistry r;
        r.registerBool("b"_sid, false);
        r.registerInt("i"_sid, 0);
        r.registerFloat("f"_sid, 0.0);
        r.registerString("s"_sid, "");
        check(r.setFromString("b"_sid, "true") && r.getBool("b"_sid) == true, "setFromString bool true");
        check(r.setFromString("b"_sid, "0") && r.getBool("b"_sid) == false, "setFromString bool 0");
        check(r.setFromString("i"_sid, "1920") && r.getInt("i"_sid) == 1920, "setFromString int 1920");
        check(r.setFromString("i"_sid, "-7") && r.getInt("i"_sid) == -7, "setFromString int -7");
        check(r.setFromString("f"_sid, "3.5") && r.getFloat("f"_sid) == 3.5, "setFromString float 3.5");
        check(r.setFromString("s"_sid, "hello") && r.getString("s"_sid) == "hello", "setFromString string hello");

        // Failures: return false AND leave the value unchanged.
        check(!r.setFromString("i"_sid, "12x") && r.getInt("i"_sid) == -7, "setFromString int 12x -> false, unchanged");
        check(!r.setFromString("i"_sid, "") && r.getInt("i"_sid) == -7, "setFromString int empty -> false, unchanged");
        check(!r.setFromString("f"_sid, "abc") && r.getFloat("f"_sid) == 3.5, "setFromString float abc -> false, unchanged");

        // Float trailing garbage: strtod consumes "1.0" but not "junk" -> reject,
        // value (currently 3.5, exactly representable) left unchanged.
        check(!r.setFromString("f"_sid, "1.0junk") && r.getFloat("f"_sid) == 3.5, "setFromString float 1.0junk -> false, unchanged");

        // Leading whitespace rejected for BOTH int and float (Fix 1): the int
        // path (from_chars) and float path (strtod + explicit leading-space
        // guard) must behave identically. Values use exactly-representable ints.
        check(!r.setFromString("i"_sid, " 1") && r.getInt("i"_sid) == -7, "setFromString int leading-space -> false, unchanged");
        check(!r.setFromString("f"_sid, " 2.0") && r.getFloat("f"_sid) == 3.5, "setFromString float leading-space -> false, unchanged");

        // Non-finite rejected (Fix 2): strtod accepts "nan"/"inf" but a NaN cvar
        // would refire callbacks on every set, so reject them; value unchanged.
        check(!r.setFromString("f"_sid, "nan") && r.getFloat("f"_sid) == 3.5, "setFromString float nan -> false, unchanged");
        check(!r.setFromString("f"_sid, "inf") && r.getFloat("f"_sid) == 3.5, "setFromString float inf -> false, unchanged");
    }

    // --- CHANGE CALLBACKS ----------------------------------------------------
    {
        CVarRegistry r;
        r.registerInt("i"_sid, 0);
        int fires = 0;
        r.onChanged("i"_sid, [&]{ ++fires; });
        check(r.setInt("i"_sid, 10) && fires == 1, "callback fires on change");
        check(r.setInt("i"_sid, 10) && fires == 1, "callback does not re-fire on same value");
        check(r.setFromString("i"_sid, "20") && fires == 2, "callback fires on setFromString change");
        check(r.reset("i"_sid) && fires == 3, "callback fires on reset that changes value");
    }

    // --- TYPE OF -------------------------------------------------------------
    {
        CVarRegistry r;
        r.registerBool("b"_sid, true);
        r.registerInt("i"_sid, 1);
        r.registerFloat("f"_sid, 1.0);
        r.registerString("s"_sid, "x");
        check(r.typeOf("b"_sid) == CVarType::Bool, "typeOf bool");
        check(r.typeOf("i"_sid) == CVarType::Int, "typeOf int");
        check(r.typeOf("f"_sid) == CVarType::Float, "typeOf float");
        check(r.typeOf("s"_sid) == CVarType::String, "typeOf string");
        check(r.typeOf("nope"_sid, CVarType::String) == CVarType::String, "typeOf missing -> fallback");
    }

    // --- CLEAR ---------------------------------------------------------------
    {
        CVarRegistry r;
        r.registerInt("i"_sid, 1);
        r.clear();
        check(r.count() == 0 && !r.has("i"_sid), "clear empties the registry");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
