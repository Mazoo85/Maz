#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "maz/io/BundlePlan.hpp"   // SourceFile / BundleFile / FileKind / detail::baseName
#include "maz/io/ExportConfig.hpp" // ExportPreset

// maz::io mobile export bundle planner — the Android/iOS analogue of io::planBundle. A phone build is NOT a
// flat folder with a launcher script: Android wants the game as a shared library under lib/<abi>/, its assets
// under assets/, and an AndroidManifest.xml; iOS wants everything inside a <App>.app bundle with the binary
// at the root, dylibs under Frameworks/, and an Info.plist. This planner computes that COMPLETE layout
// deterministically — where every file lands and the exact manifest/plist text — so the on-device build
// (Gradle / Xcode, see docs/MOBILE_BUILD.md) is a mechanical "copy the planned files + drop in the generated
// manifest" step. Pure string/size logic, no filesystem, so it unit-tests headlessly exactly like the desktop
// planner. The concrete APK/IPA assembly still needs the owner's SDK/toolchain; this is the decision layer
// that makes it turnkey.
namespace maz::io {

enum class MobileOs { Android, iOS };

inline std::string mobileOsName(MobileOs os) {
    return os == MobileOs::Android ? "android" : "ios";
}

// A reverse-DNS-safe application id segment from an app name: lowercase, non-alphanumerics dropped. Used for
// the Android package / iOS CFBundleIdentifier (e.g. "ZOMBOID" -> "com.mazengine.zomboid").
inline std::string appIdSegment(const std::string& appName) {
    std::string out;
    for (char c : appName) {
        if (c >= 'A' && c <= 'Z') {
            out += static_cast<char>(c - 'A' + 'a');
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out += c;
        }
        // everything else (spaces, punctuation) is dropped
    }
    if (out.empty()) {
        out = "app";
    }
    // A leading digit is illegal in a Java/bundle identifier segment.
    if (out[0] >= '0' && out[0] <= '9') {
        out = "a" + out;
    }
    return out;
}

inline std::string bundleId(const std::string& appName) {
    return "com.mazengine." + appIdSegment(appName);
}

struct MobileBundlePlan {
    std::string appName;
    std::string version;
    MobileOs os = MobileOs::Android;
    std::string abi;               // Android only, e.g. "arm64-v8a"; empty on iOS
    std::string bundleId;          // reverse-DNS application id
    std::string bundleRoot;        // "" on Android (apk root), "<App>.app" on iOS
    std::string manifestDest;      // "AndroidManifest.xml" / "<App>.app/Info.plist"
    std::string manifest;          // the generated manifest / plist text
    std::vector<BundleFile> files; // exe (as .so on Android), libraries, shaders, assets, and the manifest
    std::string listing;           // "<size>\t<dest>" lines, sorted by dest (the deterministic file list)
    uint64_t totalSize = 0;        // sum of all planned file sizes (manifest included)
};

// The generated AndroidManifest.xml: an SDLActivity-hosted, Vulkan-required, landscape game. This is the
// exact manifest the Gradle build drops in; only the package/label/version are app-specific.
inline std::string androidManifest(const std::string& appName, const std::string& version) {
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
           "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
           "          package=\"" + bundleId(appName) + "\"\n"
           "          android:versionCode=\"1\" android:versionName=\"" + version + "\">\n"
           "    <uses-feature android:name=\"android.hardware.vulkan.version\" android:required=\"true\" />\n"
           "    <uses-sdk android:minSdkVersion=\"26\" android:targetSdkVersion=\"34\" />\n"
           "    <application android:label=\"" + appName + "\" android:hasCode=\"true\">\n"
           "        <activity android:name=\"org.libsdl.app.SDLActivity\"\n"
           "                  android:configChanges=\"orientation|screenSize|keyboardHidden\"\n"
           "                  android:screenOrientation=\"sensorLandscape\" android:exported=\"true\">\n"
           "            <intent-filter>\n"
           "                <action android:name=\"android.intent.action.MAIN\" />\n"
           "                <category android:name=\"android.intent.category.LAUNCHER\" />\n"
           "            </intent-filter>\n"
           "        </activity>\n"
           "    </application>\n"
           "</manifest>\n";
}

// The generated iOS Info.plist: a Metal-required, landscape game whose executable matches the app name.
inline std::string iosInfoPlist(const std::string& appName, const std::string& version) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
           "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
           "<plist version=\"1.0\">\n"
           "<dict>\n"
           "    <key>CFBundleName</key><string>" + appName + "</string>\n"
           "    <key>CFBundleExecutable</key><string>" + appName + "</string>\n"
           "    <key>CFBundleIdentifier</key><string>" + bundleId(appName) + "</string>\n"
           "    <key>CFBundleShortVersionString</key><string>" + version + "</string>\n"
           "    <key>CFBundleVersion</key><string>1</string>\n"
           "    <key>UIRequiredDeviceCapabilities</key><array><string>metal</string></array>\n"
           "    <key>UISupportedInterfaceOrientations</key>\n"
           "    <array>\n"
           "        <string>UIInterfaceOrientationLandscapeLeft</string>\n"
           "        <string>UIInterfaceOrientationLandscapeRight</string>\n"
           "    </array>\n"
           "</dict>\n"
           "</plist>\n";
}

// Compute the full mobile bundle plan. Executables and libraries always ship (they are the runtime); shaders
// and assets honor the export preset's include/exclude filters when a preset is supplied — same policy as the
// desktop planner. `abi` is the Android ABI directory (ignored on iOS).
inline MobileBundlePlan planMobileBundle(const std::string& appName, const std::string& version, MobileOs os,
                                         const std::vector<SourceFile>& sources,
                                         const std::string& abi = "arm64-v8a",
                                         const ExportPreset* preset = nullptr) {
    MobileBundlePlan plan;
    plan.appName = appName;
    plan.version = version;
    plan.os = os;
    plan.bundleId = bundleId(appName);
    plan.abi = (os == MobileOs::Android) ? abi : "";
    plan.bundleRoot = (os == MobileOs::iOS) ? (appName + ".app") : "";

    // Prefix a bundle-relative path with the iOS .app root (Android's apk root has no prefix).
    auto rooted = [&](const std::string& p) {
        return plan.bundleRoot.empty() ? p : plan.bundleRoot + "/" + p;
    };
    auto shipsResource = [&](const std::string& path) {
        return preset == nullptr || preset->includes(path);
    };

    for (const SourceFile& s : sources) {
        BundleFile bf;
        bf.source = s.path;
        bf.size = s.size;
        bf.executable = false; // nothing in a mobile bundle is a chmod +x launcher
        switch (s.kind) {
            case FileKind::Executable:
                if (os == MobileOs::Android) {
                    // The game is a shared library loaded by SDLActivity: lib/<abi>/lib<app>.so.
                    bf.dest = "lib/" + abi + "/lib" + appName + ".so";
                } else {
                    // On iOS the Mach-O binary sits at the .app root, named after the app.
                    bf.dest = rooted(appName);
                }
                break;
            case FileKind::Library:
                if (os == MobileOs::Android) {
                    bf.dest = "lib/" + abi + "/" + detail::baseName(s.path);
                } else {
                    bf.dest = rooted("Frameworks/" + detail::baseName(s.path));
                }
                break;
            case FileKind::Shader:
            case FileKind::Asset:
                if (!shipsResource(s.path)) {
                    continue;
                }
                // Android bundles assets under assets/; iOS lays them at the .app root (bundle resources).
                bf.dest = (os == MobileOs::Android) ? ("assets/" + s.path) : rooted(s.path);
                break;
        }
        plan.files.push_back(bf);
    }

    // The generated manifest / plist is itself a bundle file.
    plan.manifestDest = (os == MobileOs::Android) ? std::string("AndroidManifest.xml")
                                                  : rooted("Info.plist");
    plan.manifest = (os == MobileOs::Android) ? androidManifest(appName, version)
                                              : iosInfoPlist(appName, version);
    BundleFile manifestFile;
    manifestFile.source = "<generated>";
    manifestFile.dest = plan.manifestDest;
    manifestFile.size = static_cast<uint64_t>(plan.manifest.size());
    manifestFile.executable = false;
    plan.files.push_back(manifestFile);

    // Deterministic file listing, sorted by destination path.
    std::vector<const BundleFile*> ordered;
    ordered.reserve(plan.files.size());
    for (const BundleFile& f : plan.files) {
        ordered.push_back(&f);
        plan.totalSize += f.size;
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const BundleFile* a, const BundleFile* b) { return a->dest < b->dest; });
    for (const BundleFile* f : ordered) {
        plan.listing += std::to_string(f->size) + "\t" + f->dest + "\n";
    }
    return plan;
}

// Convenience: does the plan place a file at this destination? (for verification / tests)
inline const BundleFile* findMobileDest(const MobileBundlePlan& plan, const std::string& dest) {
    for (const BundleFile& f : plan.files) {
        if (f.dest == dest) {
            return &f;
        }
    }
    return nullptr;
}

} // namespace maz::io
