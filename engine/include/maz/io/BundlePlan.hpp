#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "maz/io/ExportConfig.hpp"

// maz::io desktop export bundle planner — the per-OS "brain" behind tools/package.sh and Godot's
// "Export Project". Given a built app (its executable, the runtime libraries it links, its shaders and
// assets) and a target OS, it computes the COMPLETE bundle layout deterministically: the platform
// executable name (game.exe on Windows, game on Linux/macOS), where each file lands inside the bundle,
// a launcher script that makes the game find its own libraries and run from anywhere, a MANIFEST, and
// the total size. This is the decision-making package.sh currently does inline in shell — lifting it
// into a tested C++ core is what turns the packager into a real, verifiable per-OS bundler. Pure
// string/size logic, no filesystem calls, so the whole plan unit-tests headlessly; the shell (or an
// in-editor "Export" button) just executes the plan the planner returns.
namespace maz::io {

enum class TargetOs { Windows, Linux, MacOS };

inline std::string osName(TargetOs os) {
    switch (os) {
        case TargetOs::Windows: return "windows";
        case TargetOs::MacOS: return "macos";
        case TargetOs::Linux: break;
    }
    return "linux";
}

// Per-OS conventions the bundle must respect.
struct PlatformSpec {
    std::string exeSuffix;    // ".exe" on Windows, empty elsewhere
    std::string libSuffix;    // ".dll" / ".so" / ".dylib" (informational; libs ship as-built)
    std::string launcherName; // the script a player double-clicks
    bool windows = false;     // affects the launcher dialect and library-path mechanism
};

inline PlatformSpec platformSpec(TargetOs os) {
    switch (os) {
        case TargetOs::Windows: return PlatformSpec{".exe", ".dll", "run.bat", true};
        case TargetOs::MacOS: return PlatformSpec{"", ".dylib", "run.command", false};
        case TargetOs::Linux: break;
    }
    return PlatformSpec{"", ".so", "run.sh", false};
}

// What kind of input file this is — decides where it lands and whether it is marked executable.
enum class FileKind { Executable, Library, Shader, Asset };

// One input file to package, with its res-relative path and byte size.
struct SourceFile {
    std::string path;
    uint64_t size = 0;
    FileKind kind = FileKind::Asset;
};

// One planned output entry: copy `source` to `dest` (relative to the bundle root).
struct BundleFile {
    std::string source;
    std::string dest;
    uint64_t size = 0;
    bool executable = false;
};

struct BundlePlan {
    std::string appName;
    std::string version;
    TargetOs os = TargetOs::Linux;
    std::string exeName;        // appName + platform suffix
    std::string launcherName;   // run.sh / run.bat / run.command
    std::string launcherScript; // generated launcher contents
    std::vector<BundleFile> files; // exe, libraries, shaders, assets, and the launcher
    std::string manifest;       // "<size>\t<dest>" lines, sorted by dest
    uint64_t totalSize = 0;     // sum of all planned file sizes (launcher included)
};

namespace detail {
inline std::string baseName(const std::string& p) {
    const std::size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}
} // namespace detail

// The redistributable directory name package.sh tars up: "<app>-<version>-<os>-<arch>".
inline std::string bundleDirName(const std::string& app, const std::string& version, TargetOs os,
                                 const std::string& arch) {
    return app + "-" + version + "-" + osName(os) + "-" + arch;
}

// Build the launcher script for a target OS: it changes to its own directory (so relative paths work
// from anywhere), makes the bundled libraries discoverable, and runs the game.
inline std::string makeLauncher(TargetOs os, const std::string& exeName) {
    if (os == TargetOs::Windows) {
        // On Windows the DLL search path already includes the executable's directory.
        return "@echo off\r\n"
               "cd /d \"%~dp0\"\r\n"
               "start \"\" \"" + exeName + "\" %*\r\n";
    }
    const std::string var = os == TargetOs::MacOS ? "DYLD_LIBRARY_PATH" : "LD_LIBRARY_PATH";
    return "#!/usr/bin/env bash\n"
           "cd \"$(dirname \"$0\")\"\n"
           "export " + var + "=\"$PWD:${" + var + ":-}\"\n"
           "exec \"./" + exeName + "\" \"$@\"\n";
}

// Compute the full bundle plan. Executables and libraries always ship (they are the runtime); shaders
// and assets are subject to the export preset's include/exclude filters when a preset is supplied.
inline BundlePlan planBundle(const std::string& appName, const std::string& version, TargetOs os,
                             const std::vector<SourceFile>& sources,
                             const ExportPreset* preset = nullptr) {
    const PlatformSpec spec = platformSpec(os);
    BundlePlan plan;
    plan.appName = appName;
    plan.version = version;
    plan.os = os;
    plan.exeName = appName + spec.exeSuffix;
    plan.launcherName = spec.launcherName;
    plan.launcherScript = makeLauncher(os, plan.exeName);

    auto shipsResource = [&](const std::string& path) {
        return preset == nullptr || preset->includes(path);
    };

    for (const SourceFile& s : sources) {
        BundleFile bf;
        bf.source = s.path;
        bf.size = s.size;
        switch (s.kind) {
            case FileKind::Executable:
                bf.dest = plan.exeName; // rename to the platform executable name
                bf.executable = true;
                break;
            case FileKind::Library:
                bf.dest = detail::baseName(s.path); // libraries sit next to the executable
                bf.executable = false;
                break;
            case FileKind::Shader:
                if (!shipsResource(s.path)) {
                    continue;
                }
                bf.dest = s.path; // preserve the res-relative layout
                bf.executable = false;
                break;
            case FileKind::Asset:
                if (!shipsResource(s.path)) {
                    continue;
                }
                bf.dest = s.path;
                bf.executable = false;
                break;
        }
        plan.files.push_back(bf);
    }

    // The launcher itself is a bundle file (executable on POSIX).
    BundleFile launcher;
    launcher.source = "<generated>";
    launcher.dest = plan.launcherName;
    launcher.size = static_cast<uint64_t>(plan.launcherScript.size());
    launcher.executable = !spec.windows;
    plan.files.push_back(launcher);

    // Manifest: deterministic, sorted by destination path.
    std::vector<const BundleFile*> ordered;
    ordered.reserve(plan.files.size());
    for (const BundleFile& f : plan.files) {
        ordered.push_back(&f);
        plan.totalSize += f.size;
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const BundleFile* a, const BundleFile* b) { return a->dest < b->dest; });
    for (const BundleFile* f : ordered) {
        plan.manifest += std::to_string(f->size) + "\t" + f->dest + "\n";
    }
    return plan;
}

// Convenience: does the plan place a file at this destination? (for verification / tests)
inline const BundleFile* findDest(const BundlePlan& plan, const std::string& dest) {
    for (const BundleFile& f : plan.files) {
        if (f.dest == dest) {
            return &f;
        }
    }
    return nullptr;
}

} // namespace maz::io
