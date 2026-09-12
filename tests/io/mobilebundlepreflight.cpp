// tests/io/mobilebundlepreflight.cpp — verifies the mobile bundle preflight validator
// (io::preflightMobileBundle), which compares a MobileBundlePlan against the paths that really exist under a
// staged bundle root and reports every gap before Gradle/Xcode runs. Pure set logic, deterministic:
//   * a fully-staged tree passes with zero errors;
//   * a missing planned file, a missing manifest, and a plan with no game binary are all errors;
//   * a shader-less bundle and stale/unexpected staged files are warnings (do not block);
//   * mobileExecutableDest names the .so on Android and the .app-root binary on iOS.
#include "maz/io/MobileBundlePreflight.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::io::BundleFile;
using maz::io::FileKind;
using maz::io::MobileOs;
using maz::io::PreflightSeverity;
using maz::io::SourceFile;
using maz::io::mobileExecutableDest;
using maz::io::planMobileBundle;
using maz::io::preflightMobileBundle;

static std::vector<SourceFile> sampleSources() {
    return {
        SourceFile{"zomboid", 2000, FileKind::Executable},
        SourceFile{"lib/libSDL3.so", 500000, FileKind::Library},
        SourceFile{"shaders/sprite.spv", 800, FileKind::Shader},
        SourceFile{"assets/font.ttf", 30000, FileKind::Asset},
    };
}

// Every destination the plan expects on disk — the "perfectly staged" listing.
static std::vector<std::string> stagedFromPlan(const maz::io::MobileBundlePlan& plan) {
    std::vector<std::string> out;
    for (const BundleFile& f : plan.files) {
        out.push_back(f.dest);
    }
    return out;
}

static bool hasIssue(const maz::io::PreflightReport& rep, PreflightSeverity sev, const std::string& needle) {
    for (const auto& i : rep.issues) {
        if (i.severity == sev && i.message.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int main() {
    // --- 1. Executable destination naming (Android .so vs iOS .app-root binary). ---
    {
        const auto apk = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::Android, sampleSources(), "arm64-v8a");
        CHECK(mobileExecutableDest(apk) == "lib/arm64-v8a/libZOMBOID.so", "android exe dest is the .so");
        const auto ipa = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::iOS, sampleSources());
        CHECK(mobileExecutableDest(ipa) == "ZOMBOID.app/ZOMBOID", "ios exe dest is the .app-root binary");
    }

    // --- 2. A perfectly-staged Android tree passes (only the no-shader warning is absent — it has one). ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::Android, sampleSources(), "arm64-v8a");
        const auto rep = preflightMobileBundle(plan, stagedFromPlan(plan));
        CHECK(rep.ok(), "fully-staged bundle is ok()");
        CHECK(rep.errorCount() == 0, "fully-staged bundle has no errors");
        CHECK(rep.warningCount() == 0, "fully-staged bundle (with a .spv) has no warnings");
    }

    // --- 3. A missing planned file is an error; ok() flips to false. ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::Android, sampleSources(), "arm64-v8a");
        auto staged = stagedFromPlan(plan);
        // Drop the font asset from the on-disk listing.
        staged.erase(std::remove(staged.begin(), staged.end(), std::string("assets/assets/font.ttf")),
                     staged.end());
        // The planner stages assets under assets/<path>; the sample path already starts with "assets/".
        // Compute the actual dest to remove instead of guessing.
        std::string fontDest;
        for (const BundleFile& f : plan.files) {
            if (f.source == "assets/font.ttf") {
                fontDest = f.dest;
            }
        }
        staged = stagedFromPlan(plan);
        staged.erase(std::remove(staged.begin(), staged.end(), fontDest), staged.end());

        const auto rep = preflightMobileBundle(plan, staged);
        CHECK(!rep.ok(), "missing file makes ok() false");
        CHECK(hasIssue(rep, PreflightSeverity::Error, "missing planned file: " + fontDest),
              "missing file reported as error");
    }

    // --- 4. A missing manifest is its own dedicated error. ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::Android, sampleSources(), "arm64-v8a");
        auto staged = stagedFromPlan(plan);
        staged.erase(std::remove(staged.begin(), staged.end(), plan.manifestDest), staged.end());
        const auto rep = preflightMobileBundle(plan, staged);
        CHECK(hasIssue(rep, PreflightSeverity::Error, "missing manifest: " + plan.manifestDest),
              "missing manifest reported distinctly");
    }

    // --- 5. A plan built with no Executable source has no game binary -> error. ---
    {
        std::vector<SourceFile> noExe = {
            SourceFile{"shaders/sprite.spv", 800, FileKind::Shader},
            SourceFile{"assets/font.ttf", 30000, FileKind::Asset},
        };
        const auto plan = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::Android, noExe, "arm64-v8a");
        const auto rep = preflightMobileBundle(plan, stagedFromPlan(plan));
        CHECK(hasIssue(rep, PreflightSeverity::Error, "no game binary"), "no-executable plan is an error");
    }

    // --- 6. A shader-less bundle warns but does not block. ---
    {
        std::vector<SourceFile> noShader = {
            SourceFile{"zomboid", 2000, FileKind::Executable},
            SourceFile{"assets/font.ttf", 30000, FileKind::Asset},
        };
        const auto plan = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::Android, noShader, "arm64-v8a");
        const auto rep = preflightMobileBundle(plan, stagedFromPlan(plan));
        CHECK(rep.ok(), "shader-less bundle still ok() (warning only)");
        CHECK(hasIssue(rep, PreflightSeverity::Warning, "no compiled shaders"), "shader-less bundle warns");
    }

    // --- 7. An unexpected staged file (stale artifact) warns. ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::Android, sampleSources(), "arm64-v8a");
        auto staged = stagedFromPlan(plan);
        staged.push_back("assets/OLD_leftover.dat");
        const auto rep = preflightMobileBundle(plan, staged);
        CHECK(rep.ok(), "a stale extra file does not block (warning only)");
        CHECK(hasIssue(rep, PreflightSeverity::Warning, "unexpected staged file (not in plan): assets/OLD_leftover.dat"),
              "unexpected staged file warns");
    }

    // --- 8. The report() string carries a verdict line and issue counts. ---
    {
        const auto plan = planMobileBundle("ZOMBOID", "1.0.0", MobileOs::iOS, sampleSources());
        auto staged = stagedFromPlan(plan);
        staged.pop_back(); // drop one planned file so we have an error
        const auto rep = preflightMobileBundle(plan, staged);
        const std::string text = rep.report();
        CHECK(text.find("preflight FAILED") != std::string::npos, "failing report says FAILED");
        CHECK(text.find("error(s)") != std::string::npos, "report states error count");
    }

    // --- 9. A packed bundle (resources inside game.pck, no loose .spv) does NOT warn about shaders. ---
    {
        std::vector<std::string> planned = {
            "lib/arm64-v8a/libZOMBOID.so",
            "lib/arm64-v8a/libSDL3.so",
            "assets/game.pck",
            "AndroidManifest.xml",
        };
        const auto rep = maz::io::preflightStagedTree(planned, "lib/arm64-v8a/libZOMBOID.so",
                                                      "AndroidManifest.xml", planned);
        CHECK(rep.ok(), "packed bundle is ok()");
        CHECK(!hasIssue(rep, PreflightSeverity::Warning, "no compiled shaders"),
              "a .pck counts as shipped resources (no shader warning)");
    }

    if (g_fail == 0) {
        std::printf("mobilebundlepreflight: all checks passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
