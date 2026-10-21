#pragma once
// Shared mini test harness for the Iron Front logic tests (same shape as tests/zomboid/sim.cpp).
#include <cstdio>

static int g_fail = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FAIL: %s (%s:%d)\n", #c, __FILE__, __LINE__); \
            ++g_fail;                                                  \
        }                                                              \
    } while (0)

inline int finish(const char* name) {
    if (g_fail == 0) std::printf("%s: all checks passed\n", name);
    return g_fail;
}
