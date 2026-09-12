// tests/io/androidgradle.cpp — verifies the generated Android Gradle build files
// (io::androidBuildGradle / io::androidSettingsGradle) that turn mobilepack's staged bundle into a
// buildable Gradle project without hand-authoring one. Pure string generation, deterministic:
//   * build.gradle carries the reverse-DNS applicationId/namespace, the app version, and min/target SDK;
//   * ndk abiFilters lists exactly the staged ABIs (single or fat), never empty;
//   * it points jniLibs at lib/ and assets at assets/ (mobilepack's staged layout) and does NOT compile
//     native code (packages the prebuilt .so per ABI);
//   * settings.gradle names the project after the app id.
#include "maz/io/MobileBundlePlan.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::io::androidBuildGradle;
using maz::io::androidSettingsGradle;

static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

int main() {
    // --- 1. build.gradle: identity, version, SDK levels, and the fat abiFilters list. ---
    {
        const std::string g = androidBuildGradle("ZOMBOID", "1.2.0", {"arm64-v8a", "armeabi-v7a"}, 26, 34);
        CHECK(has(g, "id 'com.android.application'"), "applies the android application plugin");
        CHECK(has(g, "namespace 'com.mazengine.zomboid'"), "namespace is the reverse-DNS bundle id");
        CHECK(has(g, "applicationId \"com.mazengine.zomboid\""), "applicationId is the reverse-DNS bundle id");
        CHECK(has(g, "versionName \"1.2.0\""), "versionName carries the app version");
        CHECK(has(g, "minSdk 26") && has(g, "targetSdk 34") && has(g, "compileSdk 34"), "SDK levels set");
        CHECK(has(g, "abiFilters 'arm64-v8a', 'armeabi-v7a'"), "fat abiFilters lists both staged ABIs");
    }

    // --- 2. Packages PRE-STAGED artifacts (no native build): jniLibs -> lib/, assets -> assets/. ---
    {
        const std::string g = androidBuildGradle("ZOMBOID", "1.0.0", {"arm64-v8a"});
        CHECK(has(g, "jniLibs.srcDirs = ['lib']"), "jniLibs point at the staged lib/ tree");
        CHECK(has(g, "assets.srcDirs = ['assets']"), "assets point at the staged assets/ tree");
        CHECK(has(g, "manifest.srcFile 'AndroidManifest.xml'"), "uses the generated manifest");
        CHECK(!has(g, "externalNativeBuild") && !has(g, "CMakeLists"), "does not compile native code");
        CHECK(has(g, "lib" "ZOMBOID" ".so"), "comment names the prebuilt game .so");
    }

    // --- 3. A single-ABI build still produces a valid (non-empty) abiFilters. ---
    {
        const std::string g = androidBuildGradle("Game", "0.1.0", {"x86_64"});
        CHECK(has(g, "abiFilters 'x86_64'"), "single ABI filter");
        // Empty ABI list falls back to arm64-v8a rather than an invalid empty filter.
        const std::string e = androidBuildGradle("Game", "0.1.0", {});
        CHECK(has(e, "abiFilters 'arm64-v8a'"), "empty ABI list falls back to arm64-v8a");
    }

    // --- 4. settings.gradle names the project after the sanitized app id. ---
    {
        const std::string s = androidSettingsGradle("Orb Run!");
        CHECK(has(s, "rootProject.name = 'orbrun'"), "settings names the project after the app id");
        CHECK(has(s, "SDL3"), "settings hints where to add the SDL3 module");
    }

    if (g_fail == 0) {
        std::printf("androidgradle: OK — build.gradle identity/SDK/abiFilters + prebuilt packaging + settings.\n");
        return 0;
    }
    std::printf("androidgradle: %d failure(s).\n", g_fail);
    return 1;
}
