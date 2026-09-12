#include "maz/core/Log.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

namespace maz::core {
namespace {

#if defined(MAZ_DEBUG)
LogLevel g_minLevel = LogLevel::Trace;
#else
LogLevel g_minLevel = LogLevel::Info;
#endif

LogSink g_sink; // optional extra destination (e.g. an editor log panel)

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
void setLogSink(LogSink sink) { g_sink = std::move(sink); }

void logMessageV(LogLevel level, const char* file, int line, const char* fmt, va_list args) {
    if (static_cast<int>(level) < static_cast<int>(g_minLevel)) {
        return;
    }
    // Format the message body once; reused by the console line and the optional sink.
    va_list argsCopy;
    va_copy(argsCopy, args);
    char body[1024];
    std::vsnprintf(body, sizeof(body), fmt, argsCopy);
    va_end(argsCopy);

    std::FILE* out = (level == LogLevel::Error || level == LogLevel::Warn) ? stderr : stdout;
    std::fprintf(out, "%s[%s] %s:%d: %s\033[0m\n", levelColor(level), levelTag(level),
                 baseName(file), line, body);
    std::fflush(out);

    if (g_sink) {
        char formatted[1200];
        std::snprintf(formatted, sizeof(formatted), "[%s] %s:%d: %s", levelTag(level),
                      baseName(file), line, body);
        g_sink(level, formatted);
    }
}

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessageV(level, file, line, fmt, args);
    va_end(args);
}

} // namespace maz::core
