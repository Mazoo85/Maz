#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

// maz::core number formatting for HUDs and UI — turn raw numbers into the human-readable strings a game
// actually shows: a score with thousand separators ("1,000,000"), a timer as a clock ("1:23:45") or a
// compact span ("1h 23m 45s"), a big idle-game count abbreviated ("1.2M"), or an asset size in bytes
// ("1.5 MiB"). These are the display helpers every game re-implements; the engine had DateTime::formatTime
// for a wall-clock instant but nothing for elapsed durations or grouped/abbreviated magnitudes. Godot's
// String offers num/pad but not these, so this is parity-or-better. Header-only, std-only, deterministic,
// locale-independent (the separator is an explicit argument, never the C locale).
namespace maz::core {

// Group an integer's digits in threes with `sep` ("1,000,000"). Handles negatives and INT64_MIN.
inline std::string groupThousands(long long value, char sep = ',') {
    const bool neg = value < 0;
    unsigned long long u = neg ? (0ULL - static_cast<unsigned long long>(value))
                               : static_cast<unsigned long long>(value);
    const std::string digits = std::to_string(u);
    std::string out;
    int count = 0;
    for (std::size_t i = digits.size(); i-- > 0;) {
        out.push_back(digits[i]);
        if (++count % 3 == 0 && i != 0) out.push_back(sep);
    }
    if (neg) out.push_back('-');
    std::reverse(out.begin(), out.end());
    return out;
}

// Elapsed seconds as a clock: "H:MM:SS" when there are whole hours, otherwise "M:SS". Negative -> 0.
inline std::string clockDuration(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    const long long total = static_cast<long long>(std::floor(seconds));
    const long long h = total / 3600, m = (total % 3600) / 60, s = total % 60;
    char buf[48];
    if (h > 0)
        std::snprintf(buf, sizeof buf, "%lld:%02lld:%02lld", h, m, s);
    else
        std::snprintf(buf, sizeof buf, "%lld:%02lld", m, s);
    return buf;
}

// Elapsed seconds as a compact span dropping leading zero units: "1h 23m 45s", "45s", "0s". Negative -> 0.
inline std::string compactDuration(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    const long long total = static_cast<long long>(std::floor(seconds));
    const long long h = total / 3600, m = (total % 3600) / 60, s = total % 60;
    std::string out;
    if (h > 0) out += std::to_string(h) + "h ";
    if (h > 0 || m > 0) out += std::to_string(m) + "m ";
    out += std::to_string(s) + "s";
    return out;
}

namespace detail {
inline std::string trimTrailingZeros(std::string s) {
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}
} // namespace detail

// Abbreviate a large magnitude with a K/M/B/T suffix ("1.2M", "1.5K", "2.5B"); values under 1000 print
// as-is. `decimals` controls the fractional places (trailing zeros trimmed). Negatives handled.
inline std::string abbreviateNumber(double value, int decimals = 1) {
    static const char* const suffix[] = {"", "K", "M", "B", "T"};
    const bool neg = value < 0.0;
    double v = neg ? -value : value;
    int k = 0;
    while (v >= 1000.0 && k < 4) {
        v /= 1000.0;
        ++k;
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals < 0 ? 0 : decimals, v);
    return (neg ? "-" : "") + detail::trimTrailingZeros(buf) + suffix[k];
}

// Byte count with a binary IEC suffix ("500 B", "1 KiB", "1.5 MiB", "2 GiB"). `decimals` places, trimmed.
inline std::string formatBytes(unsigned long long bytes, int decimals = 1) {
    static const char* const unit[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(bytes);
    int k = 0;
    while (v >= 1024.0 && k < 5) {
        v /= 1024.0;
        ++k;
    }
    if (k == 0) return std::to_string(bytes) + " B";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals < 0 ? 0 : decimals, v);
    return detail::trimTrailingZeros(buf) + " " + unit[k];
}

} // namespace maz::core
