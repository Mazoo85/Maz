// tests/io/mobilebundleplan.cpp — verifies the mobile export bundle planner (io::planMobileBundle et al.),
// the Android/iOS analogue of the desktop planner. All pure string/size logic, deterministic:
//   * Android lays the game as lib/<abi>/lib<app>.so, other libs beside it under lib/<abi>/, and assets
//     under assets/; the manifest is AndroidManifest.xml at the apk root;
//   * iOS puts everything inside <App>.app: the binary at the root (named after the app), dylibs under
//     Frameworks/, assets at the .app root, and Info.plist at <App>.app/Info.plist;
//   * export-preset include/exclude filters drop excluded shaders/assets from the plan;
//   * the generated AndroidManifest.xml / Info.plist carry the app-specific package/label/version and the
//     Vulkan/Metal + landscape requirements;
//   * the listing lists every planned file (size + dest), sorted by dest, and totalSize is the sum.
#include "maz/io/MobileBundlePlan.hpp"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::io::ExportPreset;
using maz::io::FileKind;
using maz::io::MobileOs;
using maz::io::SourceFile;
using maz::io::appIdSegment;
using maz::io::bundleId;
using maz::io::findMobileDest;
using maz::io::planMobileBundle;

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

static std::vector<SourceFile> sampleSources() {
    return {
        SourceFile{"zomboid", 2000, FileKind::Executable},
        SourceFile{"lib/libSDL3.so", 500000, FileKind::Library},
        SourceFile{"shaders/sprite.spv", 800, FileKind::Shader},
        SourceFile{"assets/font.ttf", 30000, FileKind::Asset},
        SourceFile{"levels/level1.json", 1200, FileKind::Asset},
    };
}

int main() {
    // --- 1. App-id derivation: lowercase, non-alphanumerics dropped, digit-leading guarded. ---
    {
        CHECK(appIdSegment("ZOMBOID") == "zomboid", "app id lowercases");
        CHECK(appIdSegment("Orb Run!") == "orbrun", "app id drops space + punctuation");
        CHECK(appIdSegment("3D Demo") == "a3ddemo", "app id guards a leading digit");
        CHECK(appIdSegment("") == "app", "empty app id falls back to 'app'");
        CHECK(bundleId("ZOMBOID") == "com.mazengine.zomboid", "bundle id is reverse-DNS");
    }

    // --- 2. Android: shared-lib layout, assets/ prefix, AndroidManifest.xml at the root. ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.1.0", MobileOs::Android, sampleSources(),
                                           "arm64-v8a");
        CHECK(plan.os == MobileOs::Android && plan.abi == "arm64-v8a", "android abi recorded");
        CHECK(plan.bundleRoot.empty(), "android has no .app root");

        const auto* exe = findMobileDest(plan, "lib/arm64-v8a/libZOMBOID.so");
        CHECK(exe != nullptr && exe->source == "zomboid", "executable -> lib/<abi>/lib<app>.so");
        CHECK(exe && !exe->executable, "android .so is not marked chmod+x");
        CHECK(findMobileDest(plan, "lib/arm64-v8a/libSDL3.so") != nullptr,
              "runtime libs sit under lib/<abi>/");
        CHECK(findMobileDest(plan, "assets/shaders/sprite.spv") != nullptr, "shaders under assets/");
        CHECK(findMobileDest(plan, "assets/levels/level1.json") != nullptr, "assets under assets/");

        CHECK(plan.manifestDest == "AndroidManifest.xml", "android manifest at apk root");
        CHECK(contains(plan.manifest, "package=\"com.mazengine.zomboid\""), "manifest carries the package");
        CHECK(contains(plan.manifest, "android:versionName=\"1.1.0\""), "manifest carries the version");
        CHECK(contains(plan.manifest, "android.hardware.vulkan"), "manifest requires Vulkan");
        CHECK(contains(plan.manifest, "SDLActivity"), "manifest hosts the SDL activity");
        CHECK(findMobileDest(plan, "AndroidManifest.xml") != nullptr, "manifest is a planned file");
    }

    // --- 3. iOS: everything under <App>.app, binary at the root, Frameworks/, Info.plist. ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.1.0", MobileOs::iOS, sampleSources());
        CHECK(plan.os == MobileOs::iOS && plan.abi.empty(), "ios has no abi");
        CHECK(plan.bundleRoot == "ZOMBOID.app", "ios bundle root is <App>.app");

        const auto* exe = findMobileDest(plan, "ZOMBOID.app/ZOMBOID");
        CHECK(exe != nullptr && exe->source == "zomboid", "executable at the .app root, named after app");
        CHECK(findMobileDest(plan, "ZOMBOID.app/Frameworks/libSDL3.so") != nullptr,
              "libraries under <App>.app/Frameworks/");
        CHECK(findMobileDest(plan, "ZOMBOID.app/shaders/sprite.spv") != nullptr,
              "shaders as bundle resources at the .app root");
        CHECK(findMobileDest(plan, "ZOMBOID.app/assets/font.ttf") != nullptr, "assets at the .app root");

        CHECK(plan.manifestDest == "ZOMBOID.app/Info.plist", "Info.plist inside the .app");
        CHECK(contains(plan.manifest, "<key>CFBundleExecutable</key><string>ZOMBOID</string>"),
              "plist executable matches the app");
        CHECK(contains(plan.manifest, "com.mazengine.zomboid"), "plist carries the bundle id");
        CHECK(contains(plan.manifest, "metal"), "plist requires Metal");
        CHECK(contains(plan.manifest, "1.1.0"), "plist carries the version");
    }

    // --- 4. Export preset filters drop excluded assets from the mobile plan too. ---
    {
        ExportPreset preset;
        preset.excludeFilters.push_back("levels/*"); // ship everything except levels/
        const auto plan = planMobileBundle("ZOMBOID", "1.1.0", MobileOs::Android, sampleSources(),
                                           "arm64-v8a", &preset);
        CHECK(findMobileDest(plan, "assets/levels/level1.json") == nullptr, "excluded level dropped");
        CHECK(findMobileDest(plan, "assets/shaders/sprite.spv") != nullptr, "included shader kept");
        CHECK(findMobileDest(plan, "lib/arm64-v8a/libZOMBOID.so") != nullptr,
              "executable always ships (never filtered)");
    }

    // --- 5. Listing is deterministic (sorted by dest) and totalSize sums every planned file. ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.1.0", MobileOs::Android, sampleSources(),
                                           "arm64-v8a");
        uint64_t sum = 0;
        for (const auto& f : plan.files) sum += f.size;
        CHECK(sum == plan.totalSize, "totalSize == sum of planned file sizes");
        // Sorted: each listing line's dest is >= the previous one.
        std::string prevDest;
        bool sorted = true;
        size_t pos = 0;
        while (pos < plan.listing.size()) {
            const size_t tab = plan.listing.find('\t', pos);
            const size_t nl = plan.listing.find('\n', pos);
            if (tab == std::string::npos || nl == std::string::npos) break;
            const std::string dest = plan.listing.substr(tab + 1, nl - tab - 1);
            if (!prevDest.empty() && dest < prevDest) sorted = false;
            prevDest = dest;
            pos = nl + 1;
        }
        CHECK(sorted, "listing is sorted by dest");
        CHECK(contains(plan.listing, "AndroidManifest.xml"), "listing includes the manifest");
    }

    if (g_fail == 0) {
        std::printf("mobile_bundleplan: OK — android .so/assets layout, ios .app layout, manifest/plist, "
                    "preset filters, deterministic listing.\n");
        return 0;
    }
    std::printf("mobile_bundleplan: %d failure(s).\n", g_fail);
    return 1;
}
