#include "maz/core/Log.hpp"

#include <cstdio>
#include <cstring>

namespace maz::core {
namespace {

#if defined(MAZ_DEBUG)
LogLevel g_minLevel = LogLevel::Trace;
#else
LogLevel g_minLevel = LogLevel::Info;
#endif

const char* levelTag(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

// ANSI colors; harmless if the terminal ignores them.
const char* levelColor(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "\033[90m"; // grey
    case LogLevel::Info:  return "\033[0m";  // default
    case LogLevel::Warn:  return "\033[33m"; // yellow
    case LogLevel::Error: return "\033[31m"; // red
    }
    return "\033[0m";
}

// Strip directory so log lines stay short: /a/b/Window.cpp -> Window.cpp
const char* baseName(const char* path) {
    const char* slash = std::strrchr(path, '/');
    return slash ? slash + 1 : path;
}

} // namespace

void setLogLevel(LogLevel level) { g_minLevel = level; }
LogLevel logLevel() { return g_minLevel; }

void logMessageV(LogLevel level, const char* file, int line, const char* fmt, va_list args) {
    if (static_cast<int>(level) < static_cast<int>(g_minLevel)) {
        return;
    }
    std::FILE* out = (level == LogLevel::Error || level == LogLevel::Warn) ? stderr : stdout;
    std::fprintf(out, "%s[%s] %s:%d: ", levelColor(level), levelTag(level), baseName(file), line);
    std::vfprintf(out, fmt, args);
    std::fprintf(out, "\033[0m\n");
    std::fflush(out);
}

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessageV(level, file, line, fmt, args);
    va_end(args);
}

} // namespace maz::core
