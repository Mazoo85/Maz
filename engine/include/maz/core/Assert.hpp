#pragma once

#include "maz/core/Log.hpp"

#include <cstdlib>

// MAZ_ASSERT(cond, fmt, ...) — active in debug builds, compiled out in release.
// MAZ_VERIFY(cond, fmt, ...) — always active; use for genuinely fatal runtime failures.

#define MAZ_ABORT_MSG(tag, cond, ...)                                                              \
    do {                                                                                          \
        ::maz::core::logMessage(::maz::core::LogLevel::Error, __FILE__, __LINE__,                 \
                                tag " '%s' failed", cond);                                         \
        ::maz::core::logMessage(::maz::core::LogLevel::Error, __FILE__, __LINE__, "  " __VA_ARGS__);\
        std::abort();                                                                             \
    } while (0)

#define MAZ_VERIFY(cond, ...)                                                                      \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            MAZ_ABORT_MSG("VERIFY", #cond, __VA_ARGS__);                                          \
        }                                                                                         \
    } while (0)

#if defined(MAZ_DEBUG)
#define MAZ_ASSERT(cond, ...)                                                                      \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            MAZ_ABORT_MSG("ASSERT", #cond, __VA_ARGS__);                                          \
        }                                                                                         \
    } while (0)
#else
#define MAZ_ASSERT(cond, ...) ((void)0)
#endif
