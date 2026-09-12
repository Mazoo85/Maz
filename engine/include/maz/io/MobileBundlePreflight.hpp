#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include "maz/io/MobileBundlePlan.hpp"

// maz::io mobile bundle preflight — the "is this staged tree actually shippable?" check that runs between
// io::planMobileBundle (which decides the layout) and the on-device Gradle/Xcode step (which assembles the
// .apk/.ipa). tools/package_mobile.sh copies the planned files onto disk; a typo, a filtered-out shader, a
// half-finished copy, or a stale artifact from a previous run all produce a tree that Gradle will happily
// package into a broken app that only fails on the device. Preflight compares the plan against the paths that
// really exist under the bundle root and reports every gap BEFORE the slow toolchain runs. Pure set logic over
// bundle-relative path strings, no filesystem calls, so it unit-tests headlessly like the planner; the shell
// (or an in-editor "Export" button) supplies the on-disk listing. Errors mean "will not run"; warnings mean
// "probably a mistake, but legal".
namespace maz::io {

enum class PreflightSeverity { Error, Warning };

struct PreflightIssue {
    PreflightSeverity severity = PreflightSeverity::Error;
    std::string message;
};

struct PreflightReport {
    std::vector<PreflightIssue> issues;

    std::size_t errorCount() const {
        std::size_t n = 0;
        for (const PreflightIssue& i : issues) {
            if (i.severity == PreflightSeverity::Error) {
                ++n;
            }
        }
        return n;
    }
    std::size_t warningCount() const { return issues.size() - errorCount(); }

    // Shippable when nothing is an Error. Warnings are advisory and do not block.
    bool ok() const { return errorCount() == 0; }

    // A stable, human-readable one-line-per-issue summary (in append order: the required-file errors first,
    // then the advisory warnings). Ends with a verdict line carrying the counts.
    std::string report() const {
        std::string out;
        for (const PreflightIssue& i : issues) {
            out += (i.severity == PreflightSeverity::Error ? "ERROR: " : "warning: ");
            out += i.message;
            out += '\n';
        }
        out += ok() ? "preflight OK" : "preflight FAILED";
        out += " (" + std::to_string(errorCount()) + " error(s), " + std::to_string(warningCount()) +
               " warning(s))\n";
        return out;
    }
};

// The lower-level checker: validate a set of staged (on-disk, bundle-relative) paths against the set of
// destinations a plan expects, given the two files whose absence is fatal — the game binary and the manifest.
// `resourceHint` names a substring that, if present in any planned dest, satisfies the "ships resources"
// check (".pck" for a packed bundle, ".spv" for loose shaders — either counts). Kept plan-struct-agnostic so
// both the single-plan wrapper below and the CLI's merged multi-ABI plan can call it.
inline PreflightReport preflightStagedTree(const std::vector<std::string>& plannedDests,
                                           const std::string& executableDest,
                                           const std::string& manifestDest,
                                           const std::vector<std::string>& stagedPaths) {
    PreflightReport rep;

    const std::unordered_set<std::string> staged(stagedPaths.begin(), stagedPaths.end());
    const std::unordered_set<std::string> planned(plannedDests.begin(), plannedDests.end());

    // 1) The game binary must be planned — without it there is nothing to run.
    if (planned.find(executableDest) == planned.end()) {
        rep.issues.push_back({PreflightSeverity::Error,
                              "plan has no game binary at " + executableDest +
                                  " (no Executable source was given to the planner)"});
    }

    // 2) The generated manifest / plist must be staged — Gradle/Xcode need it to build the package.
    if (!manifestDest.empty() && staged.find(manifestDest) == staged.end()) {
        rep.issues.push_back({PreflightSeverity::Error, "missing manifest: " + manifestDest});
    }

    // 3) Every other planned file must exist on disk (the manifest was already reported above).
    std::vector<std::string> missing;
    for (const std::string& d : plannedDests) {
        if (d == manifestDest) {
            continue;
        }
        if (staged.find(d) == staged.end()) {
            missing.push_back(d);
        }
    }
    std::sort(missing.begin(), missing.end());
    missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
    for (const std::string& d : missing) {
        rep.issues.push_back({PreflightSeverity::Error, "missing planned file: " + d});
    }

    // 4) A shipped tree carrying neither loose .spv shaders nor a .pck resource pack is almost always a
    //    staging mistake for a Vulkan/Metal game (the resources got filtered out or never copied).
    bool shipsResources = false;
    for (const std::string& d : plannedDests) {
        if (d.find(".spv") != std::string::npos || d.find(".pck") != std::string::npos) {
            shipsResources = true;
            break;
        }
    }
    if (!shipsResources) {
        rep.issues.push_back(
            {PreflightSeverity::Warning, "bundle ships no compiled shaders or .pck resource pack"});
    }

    // 5) Staged files the plan does not know about — usually stale artifacts from a previous package run
    //    that should be cleaned so they do not bloat (or corrupt) the final package.
    std::vector<std::string> extra;
    for (const std::string& s : stagedPaths) {
        if (planned.find(s) == planned.end()) {
            extra.push_back(s);
        }
    }
    std::sort(extra.begin(), extra.end());
    extra.erase(std::unique(extra.begin(), extra.end()), extra.end());
    for (const std::string& s : extra) {
        rep.issues.push_back({PreflightSeverity::Warning, "unexpected staged file (not in plan): " + s});
    }

    return rep;
}

// The bundle-relative destination of the game binary for this plan (the .so on Android, the Mach-O at the
// .app root on iOS). This is the one file whose absence guarantees the app cannot launch.
inline std::string mobileExecutableDest(const MobileBundlePlan& plan) {
    if (plan.os == MobileOs::Android) {
        return "lib/" + plan.abi + "/lib" + plan.appName + ".so";
    }
    // iOS: the binary sits at the .app root, named after the app.
    return plan.bundleRoot.empty() ? plan.appName : plan.bundleRoot + "/" + plan.appName;
}

// Validate a staged bundle tree against a single MobileBundlePlan. `stagedPaths` is the set of bundle-relative
// paths that actually exist on disk (files only — the caller walks the bundle root).
inline PreflightReport preflightMobileBundle(const MobileBundlePlan& plan,
                                             const std::vector<std::string>& stagedPaths) {
    std::vector<std::string> planned;
    planned.reserve(plan.files.size());
    for (const BundleFile& f : plan.files) {
        planned.push_back(f.dest);
    }
    return preflightStagedTree(planned, mobileExecutableDest(plan), plan.manifestDest, stagedPaths);
}

} // namespace maz::io
