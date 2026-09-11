#pragma once

#include "maz/core/Log.hpp"

#include <cstdio>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// maz::core log sinks — structured destinations for the log stream beyond the built-in coloured
// console. The engine's Log always prints to the console; setLogSink() adds ONE extra consumer.
// These helpers give that consumer real structure:
//
//   FileLogSink — append every line to a log file ("[LEVEL] message"), optionally timestamped and
//                 flushed each write, thread-safe. The persistent record a shipped game / server
//                 leaves behind for bug reports (Godot writes user://logs/godot.log; this is the
//                 same idea, engine-native).
//   MultiSink   — fan one log stream out to several sinks at once, so a file sink and the editor's
//                 Output panel can both receive it (setLogSink only holds one).
//
// levelName() maps LogLevel to a stable string. Install with:
//   auto file = std::make_shared<FileLogSink>("game.log");
//   core::setLogSink(core::makeSink(file));            // file only, or
//   core::setLogSink(MultiSink{ core::makeSink(file), editorPanelSink });  // file + panel
//
// Header-only; no GPU. The console output is unaffected — these are additive.
namespace maz::core {

inline const char* levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "?";
}

// A thread-safe file sink. Lines are written as "[LEVEL] message\n" (with an optional
// "YYYY-MM-DD HH:MM:SS " prefix). Open state is queryable; write() is a no-op if the file failed to
// open, so logging never crashes on a bad path.
class FileLogSink {
  public:
    struct Options {
        bool append = false;    // false truncates the file on open
        bool timestamp = false; // prefix each line with local wall-clock time
        bool flushEachLine = true;
    };

    FileLogSink() = default;
    explicit FileLogSink(const std::string& path) { open(path, Options{}); }
    FileLogSink(const std::string& path, Options opts) { open(path, opts); }
    ~FileLogSink() { close(); }

    FileLogSink(const FileLogSink&) = delete;
    FileLogSink& operator=(const FileLogSink&) = delete;

    bool open(const std::string& path) { return open(path, Options{}); }
    bool open(const std::string& path, Options opts) {
        std::lock_guard<std::mutex> lock(m_mutex);
        close_locked();
        m_opts = opts;
#if defined(_MSC_VER)
#pragma warning(push)
// fopen: the fopen_s variant is MSVC-only. The null return is checked by the caller below, which
// is the same error handling the portable call already relies on.
#pragma warning(disable : 4996)
#endif
        m_file = std::fopen(path.c_str(), opts.append ? "ab" : "wb");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        m_lineCount = 0;
        return m_file != nullptr;
    }
    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        close_locked();
    }
    bool isOpen() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_file != nullptr;
    }
    // Lines written so far (handy for tests / rotation heuristics).
    size_t lineCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lineCount;
    }

    // The LogSink entry point.
    void write(LogLevel level, const char* message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file) {
            return;
        }
        if (m_opts.timestamp) {
            char ts[32];
            const std::time_t t = std::time(nullptr);
            std::tm tmv{};
#if defined(_WIN32)
            localtime_s(&tmv, &t);
#else
            localtime_r(&t, &tmv);
#endif
            std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S ", &tmv);
            std::fputs(ts, m_file);
        }
        std::fprintf(m_file, "[%s] %s\n", levelName(level), message ? message : "");
        if (m_opts.flushEachLine) {
            std::fflush(m_file);
        }
        ++m_lineCount;
    }

    void operator()(LogLevel level, const char* message) { write(level, message); }

  private:
    void close_locked() {
        if (m_file) {
            std::fclose(m_file);
            m_file = nullptr;
        }
    }

    mutable std::mutex m_mutex;
    std::FILE* m_file = nullptr;
    Options m_opts{};
    size_t m_lineCount = 0;
};

// Bind a shared FileLogSink (or anything with write()) into a std::function LogSink that keeps it
// alive for the duration of the sink.
template <class T> LogSink makeSink(std::shared_ptr<T> target) {
    return [target](LogLevel level, const char* message) { target->write(level, message); };
}

// Fan a single log stream out to many sinks. Construct from a list and install via setLogSink;
// every registered sink receives every line, in registration order.
class MultiSink {
  public:
    MultiSink() = default;
    MultiSink(std::initializer_list<LogSink> sinks) : m_sinks(sinks) {}

    void add(LogSink sink) { m_sinks.push_back(std::move(sink)); }
    size_t size() const { return m_sinks.size(); }

    void operator()(LogLevel level, const char* message) const {
        for (const LogSink& s : m_sinks) {
            if (s) {
                s(level, message);
            }
        }
    }

  private:
    std::vector<LogSink> m_sinks;
};

} // namespace maz::core
