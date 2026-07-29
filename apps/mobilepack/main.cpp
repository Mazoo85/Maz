// Maz Engine — mobilepack: the command-line mobile exporter. It takes a desktop build tree (the game
// executable + its SDL3 libraries + compiled shaders + assets, exactly what tools/package.sh reads from
// build/bin) and stages a complete Android or iOS bundle layout on disk: the game as lib/<abi>/lib<App>.so
// (Android) or the Mach-O at the <App>.app root (iOS), assets under assets/ (Android) or the .app root
// (iOS), and the generated AndroidManifest.xml / Info.plist — every destination decided by the single
// source of truth, io::planMobileBundle. On Android it can stage MULTIPLE ABIs into one "fat" bundle
// (a lib/<abi>/ tree per ABI, with the shared assets + manifest written once). It then re-checks that
// every planned file actually landed at its planned size. The concrete APK/IPA assembly still needs the
// owner's Gradle/Xcode toolchain (see docs/MOBILE_BUILD.md); this makes the "lay out the files + drop in
// the manifest" half a one-command run.
//
//   mobilepack --app ZOMBOID --version 1.0.0 --os android --abi arm64-v8a,armeabi-v7a,x86_64 --from build/bin
//   mobilepack --selftest        # synthesize a tiny build tree in a temp dir, stage + verify it, exit 0 (CI)

#include "maz/io/MobileBundlePlan.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using namespace maz::io;

namespace {

FileKind classify(const fs::path& p) {
    return p.extension() == ".spv" ? FileKind::Shader : FileKind::Asset;
}

// Split a comma-separated list ("arm64-v8a,armeabi-v7a") into its non-empty parts.
std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Scan a desktop build tree (`from`) into the planner's source list: the game exe, any libSDL3.so*, every
// shader under shaders/, and every file under assets/. `abs` tracks the real path for each logical entry so
// staging can copy it. Returns false if the game executable is missing.
bool collectSources(const fs::path& from, const std::string& app, std::vector<SourceFile>& sources,
                    std::unordered_map<std::string, std::string>& absByLogical) {
    const fs::path exe = from / app;
    if (!fs::exists(exe)) {
        std::fprintf(stderr, "error: game executable not found: %s\n", exe.string().c_str());
        std::fprintf(stderr, "       build it first (cmake --build build --target %s)\n", app.c_str());
        return false;
    }
    auto add = [&](const fs::path& abs, const std::string& logical, FileKind kind) {
        SourceFile s;
        s.path = logical;
        s.size = static_cast<uint64_t>(fs::file_size(abs));
        s.kind = kind;
        sources.push_back(s);
        absByLogical[logical] = abs.string();
    };

    add(exe, app, FileKind::Executable);

    std::error_code ec;
    for (const auto& e : fs::directory_iterator(from, ec)) {
        const std::string name = e.path().filename().string();
        if (name.rfind("libSDL3.so", 0) == 0 && fs::is_regular_file(e.path())) {
            add(e.path(), name, FileKind::Library);
        }
    }
    // Shaders keep a `shaders/` prefix (a sibling of assets that must live under the same on-device mount);
    // assets are recorded relative to the assets/ dir itself (no prefix), because the planner already nests
    // them under the container root (`assets/` on Android, the `.app` root on iOS). This yields, on Android,
    // `assets/shaders/x.spv` and `assets/fonts/x.ttf` — the res:// tree the game already references.
    struct Root { const char* sub; const char* prefix; };
    for (const Root& r : {Root{"shaders", "shaders/"}, Root{"assets", ""}}) {
        const fs::path root = from / r.sub;
        if (!fs::is_directory(root)) continue;
        for (const auto& e : fs::recursive_directory_iterator(root, ec)) {
            if (!fs::is_regular_file(e.path())) continue;
            const std::string logical = r.prefix + fs::relative(e.path(), root, ec).generic_string();
            add(e.path(), logical, classify(e.path()));
        }
    }
    return true;
}

// A merged plan across one or more ABIs. On Android each ABI contributes its own lib/<abi>/ files; the
// shared assets and the manifest have identical destinations across ABIs and are deduped to one copy. On
// iOS there is a single (ABI-less) plan. Files are keyed by destination so the listing is deterministic.
struct MergedPlan {
    std::vector<BundleFile> files;
    std::string manifest, manifestDest, bundleId, listing;
    uint64_t totalSize = 0;
};

MergedPlan planBundleMulti(const std::string& app, const std::string& version, MobileOs os,
                           const std::vector<SourceFile>& sources, const std::vector<std::string>& abis) {
    std::map<std::string, BundleFile> byDest; // dedupe + sorted-by-dest order
    MergedPlan mp;
    const std::vector<std::string> abiList = (os == MobileOs::iOS) ? std::vector<std::string>{""} : abis;
    for (const std::string& abi : abiList) {
        const MobileBundlePlan p =
            planMobileBundle(app, version, os, sources, abi.empty() ? "arm64-v8a" : abi);
        if (mp.manifest.empty()) {
            mp.manifest = p.manifest;
            mp.manifestDest = p.manifestDest;
            mp.bundleId = p.bundleId;
        }
        for (const BundleFile& f : p.files) byDest[f.dest] = f;
    }
    for (const auto& kv : byDest) {
        mp.files.push_back(kv.second);
        mp.totalSize += kv.second.size;
        mp.listing += std::to_string(kv.second.size) + "\t" + kv.second.dest + "\n";
    }
    return mp;
}

// Stage a merged plan into `outDir`: copy each real source to its planned destination, and write the
// generated manifest/plist. Then verify every planned file exists at its planned size. Returns mismatches.
int stageAndVerify(const MergedPlan& plan, const std::unordered_map<std::string, std::string>& absByLogical,
                   const fs::path& outDir) {
    std::error_code ec;
    fs::remove_all(outDir, ec);
    fs::create_directories(outDir, ec);

    for (const BundleFile& f : plan.files) {
        const fs::path dest = outDir / f.dest;
        fs::create_directories(dest.parent_path(), ec);
        if (f.source == "<generated>") {
            std::ofstream out(dest, std::ios::binary | std::ios::trunc);
            out << plan.manifest;
        } else {
            const auto it = absByLogical.find(f.source);
            if (it == absByLogical.end()) {
                std::fprintf(stderr, "error: no source on disk for %s\n", f.source.c_str());
                return 1;
            }
            fs::copy_file(it->second, dest, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                std::fprintf(stderr, "error: copy %s -> %s: %s\n", it->second.c_str(), dest.string().c_str(),
                             ec.message().c_str());
                return 1;
            }
        }
    }

    int mismatches = 0;
    for (const BundleFile& f : plan.files) {
        const fs::path dest = outDir / f.dest;
        std::error_code sizeEc;
        const uint64_t got = fs::exists(dest) ? static_cast<uint64_t>(fs::file_size(dest, sizeEc)) : ~uint64_t(0);
        if (!fs::exists(dest) || got != f.size) {
            std::fprintf(stderr, "  MISMATCH %s: planned %llu bytes, staged %s\n", f.dest.c_str(),
                         static_cast<unsigned long long>(f.size),
                         fs::exists(dest) ? std::to_string(got).c_str() : "MISSING");
            ++mismatches;
        }
    }
    return mismatches;
}

int runSelfTest() {
    const fs::path tmp = fs::temp_directory_path() / "maz_mobilepack_selftest";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    const fs::path from = tmp / "bin";
    fs::create_directories(from / "shaders", ec);
    fs::create_directories(from / "assets" / "fonts", ec);
    auto write = [](const fs::path& p, const std::string& content) {
        std::ofstream o(p, std::ios::binary | std::ios::trunc);
        o << content;
    };
    write(from / "TestGame", "ELF-fake-binary");
    write(from / "libSDL3.so.0", "fake-sdl");
    write(from / "shaders" / "sky.vert.spv", "spv-bytes");
    write(from / "assets" / "fonts" / "font.ttf", "ttf-bytes");

    std::vector<SourceFile> sources;
    std::unordered_map<std::string, std::string> absByLogical;
    if (!collectSources(from, "TestGame", sources, absByLogical)) return 1;

    int fails = 0;

    // iOS single bundle.
    {
        const MergedPlan plan = planBundleMulti("TestGame", "1.2.3", MobileOs::iOS, sources, {});
        const fs::path out = tmp / "stage-ios";
        if (stageAndVerify(plan, absByLogical, out) != 0) ++fails;
        if (!fs::exists(out / "TestGame.app/TestGame")) { std::fprintf(stderr, "selftest: missing ios exe\n"); ++fails; }
        if (!fs::exists(out / "TestGame.app/Info.plist")) { std::fprintf(stderr, "selftest: missing ios plist\n"); ++fails; }
    }

    // Android FAT bundle across three ABIs: each gets its own lib/<abi>/libTestGame.so, but the shared
    // assets and the single AndroidManifest.xml are written once (deduped).
    {
        const std::vector<std::string> abis = {"arm64-v8a", "armeabi-v7a", "x86_64"};
        const MergedPlan plan = planBundleMulti("TestGame", "1.2.3", MobileOs::Android, sources, abis);
        const fs::path out = tmp / "stage-android-fat";
        if (stageAndVerify(plan, absByLogical, out) != 0) ++fails;
        for (const std::string& abi : abis) {
            if (!fs::exists(out / ("lib/" + abi + "/libTestGame.so"))) {
                std::fprintf(stderr, "selftest: missing lib for abi %s\n", abi.c_str());
                ++fails;
            }
        }
        // The manifest and a shared asset must appear exactly once regardless of ABI count.
        if (!fs::exists(out / "AndroidManifest.xml")) { std::fprintf(stderr, "selftest: missing manifest\n"); ++fails; }
        if (!fs::exists(out / "assets/fonts/font.ttf")) { std::fprintf(stderr, "selftest: missing asset\n"); ++fails; }
        // Exactly one <uses-feature vulkan> manifest, and no per-ABI asset duplication: the plan lists the
        // font once even though three ABIs were planned.
        int fontCount = 0;
        for (const BundleFile& f : plan.files) if (f.dest == "assets/fonts/font.ttf") ++fontCount;
        if (fontCount != 1) { std::fprintf(stderr, "selftest: shared asset duplicated %d times\n", fontCount); ++fails; }
        int libCount = 0;
        for (const BundleFile& f : plan.files) if (f.dest.rfind("lib/", 0) == 0 && f.dest.find("libTestGame.so") != std::string::npos) ++libCount;
        if (libCount != 3) { std::fprintf(stderr, "selftest: expected 3 per-ABI libs, got %d\n", libCount); ++fails; }
    }

    fs::remove_all(tmp, ec);
    if (fails == 0) {
        std::printf("mobilepack selftest: OK — ios + android fat (multi-ABI) bundles staged and verified.\n");
        return 0;
    }
    return 1;
}

void usage() {
    std::printf(
        "mobilepack — stage a mobile (Android/iOS) bundle from a desktop build tree.\n"
        "usage: mobilepack --app NAME [--version V] [--os android|ios]\n"
        "                  [--abi ABI[,ABI...]] [--from DIR] [--out DIR]\n"
        "       mobilepack --selftest\n"
        "  --abi accepts a comma-separated list on Android for a fat bundle, e.g.\n"
        "        --abi arm64-v8a,armeabi-v7a,x86_64  (ignored on iOS)\n");
}

} // namespace

int main(int argc, char** argv) {
    std::string app, version = "0.1.0", osArg = "android", abiArg = "arm64-v8a", from = "build/bin", out;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto val = [&](const char* def) -> std::string { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(def); };
        if (a == "--selftest") return runSelfTest();
        else if (a == "--app") app = val("");
        else if (a == "--version") version = val("0.1.0");
        else if (a == "--os") osArg = val("android");
        else if (a == "--abi") abiArg = val("arm64-v8a");
        else if (a == "--from") from = val("build/bin");
        else if (a == "--out") out = val("");
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); usage(); return 2; }
    }
    if (app.empty()) { usage(); return 2; }
    if (osArg != "android" && osArg != "ios") {
        std::fprintf(stderr, "error: --os must be 'android' or 'ios' (got '%s')\n", osArg.c_str());
        return 2;
    }
    const MobileOs os = (osArg == "ios") ? MobileOs::iOS : MobileOs::Android;

    std::vector<std::string> abis = splitCsv(abiArg);
    if (abis.empty()) abis = {"arm64-v8a"};

    std::vector<SourceFile> sources;
    std::unordered_map<std::string, std::string> absByLogical;
    if (!collectSources(fs::path(from), app, sources, absByLogical)) return 1;

    const MergedPlan plan = planBundleMulti(app, version, os, sources, abis);

    // Default output dir names the ABI when there is exactly one, or "fat" for a multi-ABI Android bundle.
    std::string suffix = mobileOsName(os);
    if (os == MobileOs::Android) suffix += (abis.size() == 1) ? ("-" + abis[0]) : "-fat";
    fs::path outDir = out.empty() ? fs::path("dist") / (app + "-" + version + "-" + suffix) : fs::path(out);

    std::string abiLabel;
    for (size_t i = 0; i < abis.size(); ++i) abiLabel += (i ? "+" : "") + abis[i];
    std::printf("==> mobilepack '%s' v%s for %s%s\n", app.c_str(), version.c_str(), mobileOsName(os).c_str(),
                os == MobileOs::Android ? (" [" + abiLabel + "]").c_str() : "");
    std::printf("    bundle id: %s\n", plan.bundleId.c_str());
    std::printf("    manifest:  %s\n", plan.manifestDest.c_str());
    std::printf("    files:     %zu, total %llu bytes\n", plan.files.size(),
                static_cast<unsigned long long>(plan.totalSize));

    const int mismatches = stageAndVerify(plan, absByLogical, outDir);

    { std::ofstream m(outDir / "MANIFEST.txt", std::ios::binary | std::ios::trunc); m << plan.listing; }

    if (mismatches != 0) {
        std::fprintf(stderr, "==> FAILED: %d file(s) did not stage as planned.\n", mismatches);
        return 1;
    }
    std::printf("==> OK: staged + verified at %s\n", outDir.string().c_str());
    return 0;
}
