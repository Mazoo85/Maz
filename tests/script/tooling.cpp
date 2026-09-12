// tests/script/tooling.cpp — verifies the script editor-tooling layer (script::tooling): document
// outline + context-aware autocomplete over the engine's GDScript-style language. Ground truths, all
// pure text analysis, deterministic:
//   * documentSymbols reports classes, functions (with parameter lists), signals and variables in
//     source order, each tagged with kind and enclosing class (container);
//   * function parameters are extracted and ": Type" annotations are skipped;
//   * findFunction recovers a function's signature (its parameter names) for a tooltip;
//   * prefixAt returns the partial identifier under the cursor; isMemberAccessAt detects `foo.`;
//   * completionsAt prefix-filters candidates, offers keywords + the print builtin + in-scope symbols,
//     only surfaces symbols declared BEFORE the cursor, and switches to class members after a `.`;
//   * the tolerant scanner survives half-typed code (a dangling identifier, an unterminated string).
#include "maz/script/Tooling.hpp"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::script::tooling::Completion;
using maz::script::tooling::DocSymbol;
using maz::script::tooling::SymbolKind;
using maz::script::tooling::completionsAt;
using maz::script::tooling::documentSymbols;
using maz::script::tooling::findFunction;
using maz::script::tooling::isMemberAccessAt;
using maz::script::tooling::prefixAt;

static const DocSymbol* find(const std::vector<DocSymbol>& s, const std::string& name) {
    for (const DocSymbol& d : s) {
        if (d.name == name) {
            return &d;
        }
    }
    return nullptr;
}
static bool hasLabel(const std::vector<Completion>& c, const std::string& label) {
    for (const Completion& x : c) {
        if (x.label == label) {
            return true;
        }
    }
    return false;
}

int main() {
    // --- 1. Document outline: kinds, containers, and function params. ---
    {
        const std::string src =
            "var speed = 5\n"
            "signal hit\n"
            "\n"
            "func greet(name, times: int) {\n"
            "    print(name)\n"
            "}\n"
            "\n"
            "class Enemy {\n"
            "    var hp = 10\n"
            "    func attack(target) {\n"
            "        return target\n"
            "    }\n"
            "}\n";
        const auto syms = documentSymbols(src);
        CHECK(syms.size() == 6, "six declared symbols");

        const DocSymbol* speed = find(syms, "speed");
        CHECK(speed && speed->kind == SymbolKind::Variable && speed->container.empty(),
              "speed is a top-level variable");
        const DocSymbol* hit = find(syms, "hit");
        CHECK(hit && hit->kind == SymbolKind::Signal, "hit is a signal");
        const DocSymbol* greet = find(syms, "greet");
        CHECK(greet && greet->kind == SymbolKind::Function && greet->container.empty(),
              "greet is a top-level function");
        CHECK(greet && greet->params.size() == 2 && greet->params[0] == "name" &&
                  greet->params[1] == "times",
              "greet params extracted, type annotation skipped");
        const DocSymbol* enemy = find(syms, "Enemy");
        CHECK(enemy && enemy->kind == SymbolKind::Class && enemy->container.empty(),
              "Enemy is a top-level class");
        const DocSymbol* hp = find(syms, "hp");
        CHECK(hp && hp->kind == SymbolKind::Variable && hp->container == "Enemy",
              "hp is a member variable of Enemy");
        const DocSymbol* attack = find(syms, "attack");
        CHECK(attack && attack->kind == SymbolKind::Function && attack->container == "Enemy" &&
                  attack->params.size() == 1 && attack->params[0] == "target",
              "attack is a member function of Enemy with one param");

        // 2. Signature lookup for a tooltip.
        const DocSymbol* f = findFunction(syms, "greet");
        CHECK(f && f->params.size() == 2, "findFunction recovers the signature");
        CHECK(findFunction(syms, "nope") == nullptr, "findFunction misses unknown names");
    }

    // --- 3. prefix + member-access detection. ---
    {
        const std::string s = "health.ma";
        CHECK(prefixAt(s, s.size()) == "ma", "prefix is the trailing partial identifier");
        CHECK(isMemberAccessAt(s, s.size()), "member access detected after a dot");
        const std::string s2 = "heal";
        CHECK(!isMemberAccessAt(s2, s2.size()), "no member access for a bare identifier");
        CHECK(prefixAt("x = 1 + ", 8).empty(), "empty prefix after an operator/space");
    }

    // --- 4. Completion offers in-scope symbols, prefix-filtered, only from before the cursor. ---
    {
        const std::string src =
            "var health = 100\n"
            "func hurt(amount) {\n"
            "    he\n"      // cursor completes "he" here
            "}\n"
            "var healthbar = 1\n"; // declared AFTER the cursor — must NOT be offered
        const size_t off = src.find("he\n") + 2; // just past "he"
        const auto comps = completionsAt(src, off);
        CHECK(hasLabel(comps, "health"), "in-scope 'health' offered for prefix 'he'");
        CHECK(!hasLabel(comps, "hurt"), "'hurt' excluded (prefix mismatch)");
        CHECK(!hasLabel(comps, "healthbar"), "symbol declared after the cursor is not offered");
        for (const Completion& c : comps) {
            CHECK(c.label.compare(0, 2, "he") == 0, "every candidate matches the prefix");
        }
    }

    // --- 5. Empty prefix offers keywords + builtins + symbols. ---
    {
        const std::string src = "var health = 1\nfunc hurt(amount) {\n    \n}\n";
        const size_t off = src.find("    \n") + 4; // at the blank indented line, no prefix
        const auto comps = completionsAt(src, off);
        CHECK(hasLabel(comps, "var") && hasLabel(comps, "func"), "keywords offered");
        CHECK(hasLabel(comps, "print"), "print builtin offered");
        CHECK(hasLabel(comps, "health") && hasLabel(comps, "hurt") && hasLabel(comps, "amount"),
              "in-scope symbols and params offered");
    }

    // --- 6. Member access offers class members only. ---
    {
        const std::string src =
            "class Player {\n"
            "    var hp = 5\n"
            "    func attack(x) { return x }\n"
            "}\n"
            "var p = Player()\n"
            "p.";
        const size_t off = src.size(); // right after "p."
        const auto comps = completionsAt(src, off);
        CHECK(isMemberAccessAt(src, off), "member access at end");
        CHECK(hasLabel(comps, "hp") && hasLabel(comps, "attack"), "class members offered on '.'");
        CHECK(!hasLabel(comps, "p") && !hasLabel(comps, "var"),
              "top-level names and keywords not offered on member access");

        // Partial member: "p.at" -> only attack.
        const std::string src2 = src + "at";
        const auto comps2 = completionsAt(src2, src2.size());
        CHECK(hasLabel(comps2, "attack") && !hasLabel(comps2, "hp"),
              "member prefix 'at' narrows to attack");
    }

    // --- 7. Tolerant scanner survives half-typed / invalid code. ---
    {
        const std::string src = "var ok = 1\nvar broken = \"unterminated\nfunc later(z) { }\n";
        const auto syms = documentSymbols(src);
        CHECK(find(syms, "ok") != nullptr, "symbol before the bad string still found");
        CHECK(find(syms, "later") != nullptr, "symbol after the bad string still found");
    }

    if (g_fail == 0) {
        std::printf("tooling: OK — outline, params, signatures, prefix/member detection, completion "
                    "scoping, member completion, tolerant scan.\n");
        return 0;
    }
    std::printf("tooling: %d failure(s).\n", g_fail);
    return 1;
}
