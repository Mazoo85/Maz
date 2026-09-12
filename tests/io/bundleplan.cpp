// tests/io/bundleplan.cpp — verifies the desktop export bundle planner (io::planBundle et al.).
// Ground truths, all pure string/size logic, deterministic:
//   * the executable is renamed to the platform convention (game.exe on Windows, game on Linux/macOS);
//   * libraries land next to the executable (basename at the bundle root);
//   * shaders/assets preserve their res-relative paths and are subject to the export preset filters
//     (an excluded asset is dropped from the plan);
//   * the launcher is per-OS: a .bat that runs the .exe on Windows, a .sh/.command that sets the
//     library path and runs the binary on POSIX;
//   * the MANIFEST lists every planned file (size + dest), sorted by dest, and totalSize is the sum;
//   * bundleDirName follows the <app>-<version>-<os>-<arch> convention package.sh tars up.
#include "maz/io/BundlePlan.hpp"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::io::BundlePlan;
using maz::io::ExportPreset;
using maz::io::FileKind;
using maz::io::SourceFile;
using maz::io::TargetOs;
using maz::io::bundleDirName;
using maz::io::findDest;
using maz::io::planBundle;

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

static std::vector<SourceFile> sampleSources() {
    return {
        SourceFile{"zomboid", 2000, FileKind::Executable},
        SourceFile{"lib/libSDL3.so.0", 500000, FileKind::Library},
        SourceFile{"shaders/sprite.spv", 800, FileKind::Shader},
        SourceFile{"assets/font.ttf", 30000, FileKind::Asset},
        SourceFile{"levels/level1.json", 1200, FileKind::Asset},
    };
}

int main() {
    // --- 1. Linux plan: no exe suffix, run.sh launcher, LD_LIBRARY_PATH. ---
    {
        const BundlePlan p = planBundle("zomboid", "1.0.0", TargetOs::Linux, sampleSources());
        CHECK(p.exeName == "zomboid", "linux exe keeps bare name");
        CHECK(p.launcherName == "run.sh", "linux launcher is run.sh");
        const auto* exe = findDest(p, "zomboid");
        CHECK(exe && exe->executable && exe->source == "zomboid", "exe placed and marked executable");
        const auto* lib = findDest(p, "libSDL3.so.0");
        CHECK(lib && !lib->executable, "library placed at root by basename");
        CHECK(findDest(p, "shaders/sprite.spv") != nullptr, "shader keeps res path");
        CHECK(findDest(p, "assets/font.ttf") != nullptr, "asset keeps res path");
        CHECK(contains(p.launcherScript, "LD_LIBRARY_PATH") && contains(p.launcherScript, "./zomboid"),
              "linux launcher sets lib path and runs the binary");
        CHECK(contains(p.launcherScript, "#!/usr/bin/env bash"), "linux launcher is a bash script");
    }

    // --- 2. Windows plan: .exe suffix, run.bat launcher. ---
    {
        const BundlePlan p = planBundle("zomboid", "1.0.0", TargetOs::Windows, sampleSources());
        CHECK(p.exeName == "zomboid.exe", "windows exe gets .exe suffix");
        CHECK(p.launcherName == "run.bat", "windows launcher is run.bat");
        CHECK(findDest(p, "zomboid.exe") != nullptr, "exe placed under .exe name");
        CHECK(findDest(p, "zomboid") == nullptr, "no bare-name exe on windows");
        CHECK(contains(p.launcherScript, "@echo off") && contains(p.launcherScript, "zomboid.exe"),
              "windows launcher is a batch file that runs the exe");
        const auto* launcher = findDest(p, "run.bat");
        CHECK(launcher && !launcher->executable, "windows launcher not marked +x");
    }

    // --- 3. macOS plan: run.command, DYLD_LIBRARY_PATH. ---
    {
        const BundlePlan p = planBundle("zomboid", "1.0.0", TargetOs::MacOS, sampleSources());
        CHECK(p.exeName == "zomboid" && p.launcherName == "run.command", "macos naming");
        CHECK(contains(p.launcherScript, "DYLD_LIBRARY_PATH"), "macos launcher sets DYLD path");
    }

    // --- 4. Export preset filters drop excluded assets (exe/libs always ship). ---
    {
        ExportPreset preset;
        preset.name = "release";
        preset.platform = "linux";
        preset.excludeFilters = {"levels/*"}; // ship everything except levels
        const BundlePlan p = planBundle("zomboid", "1.0.0", TargetOs::Linux, sampleSources(), &preset);
        CHECK(findDest(p, "levels/level1.json") == nullptr, "excluded asset dropped");
        CHECK(findDest(p, "assets/font.ttf") != nullptr, "non-excluded asset kept");
        CHECK(findDest(p, "zomboid") != nullptr && findDest(p, "libSDL3.so.0") != nullptr,
              "executable and library ship regardless of filters");
    }

    // --- 5. Manifest is sorted by dest and totalSize sums all files (launcher included). ---
    {
        const BundlePlan p = planBundle("zomboid", "1.0.0", TargetOs::Linux, sampleSources());
        // Sorted-by-dest: check the manifest lines are in nondecreasing dest order.
        std::string prevDest;
        size_t pos = 0;
        int lines = 0;
        bool sorted = true;
        while (pos < p.manifest.size()) {
            const size_t nl = p.manifest.find('\n', pos);
            const std::string line = p.manifest.substr(pos, nl - pos);
            const size_t tab = line.find('\t');
            const std::string dest = tab == std::string::npos ? line : line.substr(tab + 1);
            if (!prevDest.empty() && dest < prevDest) {
                sorted = false;
            }
            prevDest = dest;
            ++lines;
            pos = nl == std::string::npos ? p.manifest.size() : nl + 1;
        }
        CHECK(sorted, "manifest sorted by dest");
        CHECK(lines == 6, "manifest lists all 6 files (exe, lib, shader, 2 assets, launcher)");

        uint64_t expected = 2000 + 500000 + 800 + 30000 + 1200 + p.launcherScript.size();
        CHECK(p.totalSize == expected, "totalSize sums every planned file including the launcher");
    }

    // --- 6. bundleDirName convention. ---
    {
        CHECK(bundleDirName("zomboid", "1.0.0", TargetOs::Windows, "x86_64") ==
                  "zomboid-1.0.0-windows-x86_64",
              "bundle dir name follows app-version-os-arch");
    }

    if (g_fail == 0) {
        std::printf("bundleplan: OK — per-OS exe/launcher, lib placement, preset filters, manifest, "
                    "totals, dir naming.\n");
        return 0;
    }
    std::printf("bundleplan: %d failure(s).\n", g_fail);
    return 1;
}
