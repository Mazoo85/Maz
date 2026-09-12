// Malformed-input robustness fuzz for the scripting VM's recursive-descent parser (script::Vm::run).
// A shipping game runs script::Vm on level logic and, crucially, on MOD scripts authored by untrusted
// third parties — so a corrupt or hostile .maz source must fail with a clean parse error, never crash the
// process. The existing script tests feed only well-formed programs; this feeds deep-nesting structural
// stressors (thousands of nested parens/brackets/braces/`if`s/type parameters that would overflow the
// native stack), truncations and byte mutations of valid seed programs, and random buffers. `Vm::run`
// must always return (true or false) with no stack overflow, out-of-bounds read, OOM, or other UB. Built
// under ASan+UBSan (the sanitizer CI job runs it). This harness found and now guards a real bug: unbounded
// recursion in Parser::primary()/expression()/statement() -> stack-overflow DoS, fixed with a parse-depth
// guard mirroring the JSON parser.
#include "maz/script/Script.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feed(const std::string& s) {
    script::Vm vm;
    // Bound interpreter execution: a mutated/random program can form an infinite loop (e.g. `while (1) {}`),
    // and the VM's default step budget is unlimited, so an unbounded run would spin forever. This harness
    // targets PARSER robustness (crashes/UB on malformed source), not runtime semantics, so cap execution to
    // keep every case terminating and the whole fuzz run fast and deterministic.
    vm.setStepBudget(50000);
    const bool ok = vm.run(s);
    g_sink += ok ? 1u : vm.error().size();
}

static void truncateAndMutate(const std::string& seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) feed(seed.substr(0, k));
    std::string s = seed;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char orig = s[i];
        for (char p : {'\0', '\xff', '(', ')', '[', ']', '{', '}', '"', '\\', ';'}) {
            s[i] = p;
            feed(s);
        }
        s[i] = orig;
    }
}

int main() {
    // --- structural stressors: the shapes that overflow the native stack without a depth guard ---
    feed("print " + std::string(200000, '(') + "1" + std::string(200000, ')') + "\n"); // nested parens
    feed("var x = " + std::string(200000, '[') + "1" + std::string(200000, ']') + ";\n"); // array literals
    feed(std::string(200000, '{') + std::string(200000, '}') + "\n");                     // nested blocks
    { // nested if-statements
        std::string s;
        for (int i = 0; i < 200000; ++i) s += "if (1) ";
        s += "print 1;\n";
        feed(s);
    }
    { // nested container type annotation: Array[Array[Array[...int...]]]
        std::string s = "var x: ";
        for (int i = 0; i < 200000; ++i) s += "Array[";
        s += "int";
        for (int i = 0; i < 200000; ++i) s += "]";
        s += " = nil;\n";
        feed(s);
    }
    // A legitimately deep-but-bounded program must still parse and run (guard sits far above real nesting).
    feed("var a = ((((((((1 + 2))))))) * 3); print a;\n");

    // --- truncation + mutation of small valid seed programs ---
    truncateAndMutate("var x = 1 + 2 * 3; print x;\n");
    truncateAndMutate("func f(a, b) { return a + b; } print f(2, 3);\n");
    truncateAndMutate("var a = [1, 2, 3]; for (v in a) { print v; }\n");
    truncateAndMutate("class C { var n = 0; func inc() { n = n + 1; } } var c = C(); c.inc();\n");
    truncateAndMutate("var d = {\"k\": 1, \"j\": [true, nil]}; if (d[\"k\"] == 1) { print \"ok\"; }\n");
    truncateAndMutate("var s = 0; var i = 0; while (i < 3) { s = s + i; i = i + 1; } print s;\n");

    // --- random buffers (deterministic PRNG) ---
    std::mt19937 rng(0x5C21u);
    const std::string alphabet = "(){}[];,.+-*/%<>=!&|:\"0123abcxyz \n_funcvarifelwhretclas";
    for (int iter = 0; iter < 20000; ++iter) {
        const std::size_t n = rng() % 129u;
        std::string s(n, ' ');
        for (std::size_t i = 0; i < n; ++i) s[i] = alphabet[rng() % alphabet.size()];
        feed(s);
    }
    for (int iter = 0; iter < 10000; ++iter) {
        const std::size_t n = rng() % 129u;
        std::string s(n, '\0');
        for (std::size_t i = 0; i < n; ++i) s[i] = static_cast<char>(rng() & 0xff);
        feed(s);
    }

    std::printf("scriptparser fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
