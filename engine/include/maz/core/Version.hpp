#pragma once

#include <cstdint>
#include <string>
#include <tuple>

// maz::Version — the engine's semantic version, as compile-time macros AND a parseable/comparable
// runtime type. Games and tools use this to gate features on the engine version, to stamp save
// files and crash reports with the exact build, and to compare a save's authoring version against
// the running one. Follows semver (MAJOR.MINOR.PATCH); a leading "vX.Y.Z" and any
// "-prerelease"/"+build" suffix are accepted on parse and ignored for the numeric comparison.
// Header-only, no deps.

// Keep these in sync with the CMake project() version (single source: the numbers below).
#define MAZ_VERSION_MAJOR 0
#define MAZ_VERSION_MINOR 1
#define MAZ_VERSION_PATCH 0
#define MAZ_VERSION_STRING "0.1.0"

// A single integer for cheap >= comparisons: MAJOR*1000000 + MINOR*1000 + PATCH.
#define MAZ_VERSION_NUMBER                                                                         \
    (MAZ_VERSION_MAJOR * 1000000 + MAZ_VERSION_MINOR * 1000 + MAZ_VERSION_PATCH)

namespace maz {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    // A monotonic integer form for quick ordering / storage.
    int64_t number() const {
        return static_cast<int64_t>(major) * 1000000 + static_cast<int64_t>(minor) * 1000 + patch;
    }

    // Parse "MAJOR.MINOR.PATCH", tolerating a leading 'v' and a trailing "-pre"/"+build" suffix.
    // Missing minor/patch default to 0 ("1" -> 1.0.0, "1.2" -> 1.2.0). Returns false on a malformed
    // major component. On failure `out` is left unchanged.
    static bool parse(const std::string& s, Version& out) {
        size_t i = 0;
        if (i < s.size() && (s[i] == 'v' || s[i] == 'V')) {
            ++i;
        }
        // Trim any pre-release/build metadata.
        std::string core = s.substr(i);
        const size_t cut = core.find_first_of("-+");
        if (cut != std::string::npos) {
            core = core.substr(0, cut);
        }
        int parts[3] = {0, 0, 0};
        int idx = 0;
        size_t pos = 0;
        bool anyDigit = false;
        while (pos < core.size() && idx < 3) {
            size_t dot = core.find('.', pos);
            const std::string seg =
                core.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
            if (seg.empty() || !allDigits(seg)) {
                return false;
            }
            parts[idx++] = std::stoi(seg);
            anyDigit = true;
            if (dot == std::string::npos) {
                break;
            }
            pos = dot + 1;
        }
        if (!anyDigit) {
            return false;
        }
        out.major = parts[0];
        out.minor = parts[1];
        out.patch = parts[2];
        return true;
    }

    bool operator==(const Version& o) const {
        return std::tie(major, minor, patch) == std::tie(o.major, o.minor, o.patch);
    }
    bool operator!=(const Version& o) const { return !(*this == o); }
    bool operator<(const Version& o) const {
        return std::tie(major, minor, patch) < std::tie(o.major, o.minor, o.patch);
    }
    bool operator>(const Version& o) const { return o < *this; }
    bool operator<=(const Version& o) const { return !(o < *this); }
    bool operator>=(const Version& o) const { return !(*this < o); }

    // True if this version satisfies "at least" `min` — the common "requires engine >= X.Y.Z"
    // check.
    bool atLeast(const Version& min) const { return *this >= min; }

  private:
    static bool allDigits(const std::string& s) {
        for (char c : s) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return !s.empty();
    }
};

// The version of this engine build.
inline Version engineVersion() {
    return Version{MAZ_VERSION_MAJOR, MAZ_VERSION_MINOR, MAZ_VERSION_PATCH};
}

} // namespace maz
