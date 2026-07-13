// Unit tests for maz::core::Delegate<R(Args...)> — the single-target zero-heap
// fast delegate. Exercises free-function, non-const member, and const member
// binds; rebind; the unbound/reset states; equality; multi-arg; void return
// with side effects; and the two-pointer size bound. Pure C++, no GPU/display.

#include "maz/core/Delegate.hpp"

#include <cstdio>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

int addFree(int a, int b) { return a + b; }
int combine3(int a, int b, int c) { return a * 100 + b * 10 + c; }

int g_sideEffect = 0;
void bumpFree(int n) { g_sideEffect += n; }

struct Counter {
    int value = 0;
    void add(int n) { value += n; }
    int get() const { return value; }
};

} // namespace

int main() {
    // --- FREE FUNCTION --------------------------------------------------------
    {
        auto d = Delegate<int(int, int)>::fromFunction<&addFree>();
        check(d.isBound(), "free-fn delegate is bound");
        check(bool(d), "free-fn delegate is truthy");
        check(d(3, 4) == 7, "free-fn delegate invokes and returns");
    }

    // --- NON-CONST MEMBER MUTATION -------------------------------------------
    {
        Counter c;
        auto d = Delegate<void(int)>::fromMethod<&Counter::add, Counter>(&c);
        d(5);
        check(c.value == 5, "member delegate mutates the instance");
        d(3);
        check(c.value == 8, "member delegate mutates cumulatively");
    }

    // --- CONST MEMBER ---------------------------------------------------------
    {
        Counter c;
        c.add(42);
        auto d = Delegate<int()>::fromConstMethod<&Counter::get, Counter>(&c);
        check(d() == 42, "const-member delegate reads the instance");
    }

    // --- REBIND ---------------------------------------------------------------
    {
        Counter a, b;
        auto d = Delegate<void(int)>::fromMethod<&Counter::add, Counter>(&a);
        d(1);
        check(a.value == 1 && b.value == 0, "delegate targets the first instance");
        d = Delegate<void(int)>::fromMethod<&Counter::add, Counter>(&b);
        d(2);
        check(b.value == 2 && a.value == 1, "rebind retargets to the second instance");
    }

    // --- UNBOUND + RESET (NEVER call unbound) --------------------------------
    {
        Delegate<int(int, int)> d;
        check(!d.isBound(), "default-constructed delegate is unbound");
        check(!bool(d), "unbound delegate is falsy");
        d = Delegate<int(int, int)>::fromFunction<&addFree>();
        check(d.isBound(), "delegate is bound after assignment");
        d.reset();
        check(!d.isBound(), "delegate is unbound after reset");
    }

    // --- EQUALITY -------------------------------------------------------------
    {
        auto d1 = Delegate<int(int, int)>::fromFunction<&addFree>();
        auto d2 = Delegate<int(int, int)>::fromFunction<&addFree>();
        check(d1 == d2, "same free-fn bind compares equal");
        Counter a, b;
        auto m1 = Delegate<void(int)>::fromMethod<&Counter::add, Counter>(&a);
        auto m2 = Delegate<void(int)>::fromMethod<&Counter::add, Counter>(&b);
        check(m1 != m2, "member binds on different instances compare unequal");
        Delegate<int(int, int)> u;
        check(u != d1, "unbound compares unequal to a bound delegate");
    }

    // --- MULTI-ARG ------------------------------------------------------------
    {
        auto d = Delegate<int(int, int, int)>::fromFunction<&combine3>();
        check(d(1, 2, 3) == 123, "three-arg free-fn delegate forwards all args");
    }

    // --- VOID + SIDE EFFECT ---------------------------------------------------
    {
        g_sideEffect = 0;
        auto d = Delegate<void(int)>::fromFunction<&bumpFree>();
        d(10);
        check(g_sideEffect == 10, "void free-fn delegate produces a side effect");
        d(5);
        check(g_sideEffect == 15, "void free-fn delegate accumulates side effects");
    }

    // --- SIZE -----------------------------------------------------------------
    {
        check(sizeof(Delegate<int(int, int)>) <= 2 * sizeof(void*), "delegate is at most two pointers");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
