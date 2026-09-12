#pragma once

#include <cstdarg>
#include <functional>

namespace maz::core {

enum class LogLevel { Trace, Info, Warn, Error };

// Minimum level that will actually be emitted. Defaults to Info (Trace in debug builds).
void setLogLevel(LogLevel level);
LogLevel logLevel();

// Optional sink that also receives every emitted line (after level filtering), formatted as
// "[TAG] file:line: message" with no colour codes or trailing newline — used by the editor to show a
// live Output/Log panel. Pass a default-constructed std::function to clear it.
using LogSink = std::function<void(LogLevel level, const char* message)>;
void setLogSink(LogSink sink);

// printf-style logging. Prefer the MAZ_LOG* macros below.
void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

void logMessageV(LogLevel level, const char* file, int line, const char* fmt, va_list args);

} // namespace maz::core

#define MAZ_LOG_TRACE(...)                                                                         \
    ::maz::core::logMessage(::maz::core::LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define MAZ_LOG_INFO(...)                                                                          \
    ::maz::core::logMessage(::maz::core::LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define MAZ_LOG_WARN(...)                                                                          \
    ::maz::core::logMessage(::maz::core::LogLevel::Warn, __FILE__, __LINE__, __VA_ARGS__)
#define MAZ_LOG_ERROR(...)                                                                         \
    ::maz::core::logMessage(::maz::core::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
