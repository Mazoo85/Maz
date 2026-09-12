#pragma once

#include <csignal>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if defined(__GLIBC__) || defined(__APPLE__)
#define MAZ_HAVE_EXECINFO 1
#include <cxxabi.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#else
#define MAZ_HAVE_EXECINFO 0
#endif

// maz::platform::CrashHandler — a last-resort crash reporter, Maz's answer to Godot's
// CrashHandler. When the process hits a fatal signal (SIGSEGV / SIGABRT / SIGFPE / SIGILL / SIGBUS)
// it prints a labelled banner and a symbolized backtrace to stderr AND to a crash-log file, then
// restores the default handler and re-raises so the OS can still produce a core dump. That
// backtrace is often the only clue for a bug that only reproduces on a player's machine — the exact
// role Godot's handler plays.
//
// Signal handlers may only call async-signal-safe functions, so the crash path deliberately uses
// raw write() + backtrace_symbols_fd() (both safe) rather than std::string formatting. The rich,
// std::string-based pieces — demangling a mangled frame into a readable C++ name, naming a signal,
// capturing the current stack — live as separate free functions used off the crash path (startup
// diagnostics, tests), so the whole module is verifiable without actually crashing the test runner.
//
// POSIX (Linux/macOS) is fully supported via <execinfo.h>; on other platforms install() is a safe
// no-op and the helpers degrade gracefully, so engine code can call them unconditionally.
namespace maz::platform {

// Human-readable name for a fatal signal number ("SIGSEGV", "SIGABRT", ... or "SIG<n>").
inline const char* signalName(int sig) {
    switch (sig) {
    case SIGSEGV:
        return "SIGSEGV (segmentation fault)";
    case SIGABRT:
        return "SIGABRT (abort)";
    case SIGFPE:
        return "SIGFPE (floating-point exception)";
    case SIGILL:
        return "SIGILL (illegal instruction)";
#ifdef SIGBUS
    case SIGBUS:
        return "SIGBUS (bus error)";
#endif
    default:
        return "unknown signal";
    }
}

// Demangle a single frame from a backtrace_symbols()-style line into a readable C++ name.
// Handles both the Linux format `binary(_ZN3maz3fooEv+0x1a) [0x...]` and the macOS format
// `3  binary  0x...  _ZN3maz3fooEv + 26` by locating the mangled `_Z...` token and running it
// through the Itanium ABI demangler. If there is no mangled token (a C symbol, or an unresolved
// address), the original line is returned unchanged so nothing is ever lost.
inline std::string demangleSymbol(const std::string& line) {
#if MAZ_HAVE_EXECINFO
    const size_t z = line.find("_Z");
    if (z == std::string::npos) {
        return line;
    }
    // The mangled name runs until a character that can't appear in one.
    size_t end = z;
    while (end < line.size()) {
        const char c = line[end];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '$';
        if (!ok) {
            break;
        }
        ++end;
    }
    const std::string mangled = line.substr(z, end - z);
    int status = 0;
    char* dem = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if (status != 0 || !dem) {
        if (dem) {
            free(dem);
        }
        return line;
    }
    std::string out = line.substr(0, z) + dem + line.substr(end);
    free(dem);
    return out;
#else
    return line;
#endif
}

// Capture the current call stack as a vector of demangled frame strings (most-recent first),
// skipping `skip` innermost frames (this function itself, typically). Empty on unsupported
// platforms. Off the crash path — for startup diagnostics and tests.
inline std::vector<std::string> captureBacktrace(int maxFrames = 64, int skip = 1) {
    std::vector<std::string> frames;
#if MAZ_HAVE_EXECINFO
    if (maxFrames < 1) {
        return frames;
    }
    std::vector<void*> buffer(static_cast<size_t>(maxFrames));
    const int n = backtrace(buffer.data(), maxFrames);
    char** symbols = backtrace_symbols(buffer.data(), n);
    if (!symbols) {
        return frames;
    }
    for (int i = skip; i < n; ++i) {
        frames.push_back(demangleSymbol(symbols[i]));
    }
    free(symbols);
#else
    (void)maxFrames;
    (void)skip;
#endif
    return frames;
}

// Assemble the crash-banner text (used at the top of both the stderr dump and the log file).
// Pure string work — tested directly.
inline std::string formatCrashBanner(const std::string& appName, const std::string& version,
                                     int sig) {
    std::string s;
    s += "================ MAZ ENGINE CRASH ================\n";
    s += "app:     " + (appName.empty() ? std::string("maz") : appName) + "\n";
    s += "version: " + (version.empty() ? std::string("0.0.0") : version) + "\n";
    s += "signal:  " + std::string(signalName(sig)) + "\n";
    s += "-------------------- backtrace -------------------\n";
    return s;
}

// Configuration for install(). logPath, if set, receives a copy of the crash dump (append-safe,
// truncated at install); leave empty to dump to stderr only.
struct CrashConfig {
    std::string appName = "maz";
    std::string version = "0.0.0";
    std::string logPath; // file to also write the crash report to (optional)
};

class CrashHandler {
  public:
    // Install fatal-signal handlers. Idempotent; safe (no-op) on unsupported platforms. Returns
    // true if handlers were installed.
    static bool install(const CrashConfig& cfg = {}) {
#if MAZ_HAVE_EXECINFO
        config() = cfg;
        if (!cfg.logPath.empty()) {
            logFd() = ::open(cfg.logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        for (int sig : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS}) {
            std::signal(sig, &CrashHandler::onSignal);
        }
        installed() = true;
        return true;
#else
        (void)cfg;
        return false;
#endif
    }

    // Restore default handlers and close the log file. For clean shutdown / tests.
    static void uninstall() {
#if MAZ_HAVE_EXECINFO
        for (int sig : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS}) {
            std::signal(sig, SIG_DFL);
        }
        if (logFd() >= 0) {
            ::close(logFd());
            logFd() = -1;
        }
        installed() = false;
#endif
    }

    static bool isInstalled() { return installed(); }

  private:
    static CrashConfig& config() {
        static CrashConfig c;
        return c;
    }
    static int& logFd() {
        static int fd = -1;
        return fd;
    }
    static bool& installed() {
        static bool b = false;
        return b;
    }

#if MAZ_HAVE_EXECINFO
    // Async-signal-safe: raw write() of a C string to a descriptor.
    static void writeStr(int fd, const char* s) {
        if (fd >= 0 && s) {
            const ssize_t r = ::write(fd, s, std::strlen(s));
            (void)r;
        }
    }

    // The actual signal handler. Uses only async-signal-safe calls (write, backtrace,
    // backtrace_symbols_fd), then re-raises the default handler for a core dump.
    static void onSignal(int sig) {
        // A pre-built banner (std::string built at install-safe time would be nicer, but this is
        // constant text plus safe pieces). Keep it minimal and fixed.
        const char* banner = "\n================ MAZ ENGINE CRASH ================\n";
        const char* mid = "\nsignal:  ";
        const char* tail = "\n-------------------- backtrace -------------------\n";
        for (int fd : {2, logFd()}) {
            if (fd < 0) {
                continue;
            }
            writeStr(fd, banner);
            writeStr(fd, "app:     ");
            writeStr(fd, config().appName.c_str());
            writeStr(fd, "\nversion: ");
            writeStr(fd, config().version.c_str());
            writeStr(fd, mid);
            writeStr(fd, signalName(sig));
            writeStr(fd, tail);
        }
        void* bt[64];
        const int n = backtrace(bt, 64);
        backtrace_symbols_fd(bt, n, 2);
        if (logFd() >= 0) {
            backtrace_symbols_fd(bt, n, logFd());
        }
        // Restore the default handler and re-raise so the OS still produces a core dump.
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
#endif
};

} // namespace maz::platform
