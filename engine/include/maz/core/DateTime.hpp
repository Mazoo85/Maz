#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// maz::core — deterministic date/time utilities, Maz's answer to Godot's Time singleton. The whole
// point is DETERMINISM: nothing here reads the system clock. You pass in an epoch value (Unix
// seconds) or advance a GameClock by your fixed-step dt, and get back exact calendar fields — so an
// in-game clock, a day/night counter, a "day survived" tally, or a save-file timestamp all stay
// bit-reproducible across a replay and across machines (pair with core::Replay). Converting a real
// wall-clock reading into these fields is the caller's job (platform code), keeping the engine core
// clock-free.
//
// The civil<->days conversion is the standard proleptic-Gregorian algorithm (Howard Hinnant's
// days_from_civil / civil_from_days), correct for any year, with no lookup tables. Header-only.
namespace maz::core {

struct DateTime {
    int year = 1970;
    int month = 1;   // 1..12
    int day = 1;     // 1..31
    int hour = 0;    // 0..23
    int minute = 0;  // 0..59
    int second = 0;  // 0..59
    int weekday = 4; // 0=Sun..6=Sat (1970-01-01 was a Thursday)
};

inline bool isLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

inline int daysInMonth(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && isLeapYear(y)) {
        return 29;
    }
    return (m >= 1 && m <= 12) ? d[m - 1] : 0;
}

namespace detail {
// Days since 1970-01-01 for a civil (y, m, d). Valid for the proleptic Gregorian calendar.
inline int64_t daysFromCivil(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}
// Inverse: civil (y, m, d) from days since 1970-01-01.
inline void civilFromDays(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yy = static_cast<int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp + (mp < 10 ? 3 : -9));
    y = static_cast<int>(yy + (m <= 2));
}
} // namespace detail

// Break a Unix timestamp (seconds since 1970-01-01T00:00:00Z) into UTC calendar fields.
inline DateTime fromUnix(int64_t seconds) {
    int64_t days = seconds / 86400;
    int64_t rem = seconds % 86400;
    if (rem < 0) { // floor division for negative timestamps
        rem += 86400;
        --days;
    }
    DateTime dt;
    detail::civilFromDays(days, dt.year, dt.month, dt.day);
    dt.hour = static_cast<int>(rem / 3600);
    dt.minute = static_cast<int>((rem % 3600) / 60);
    dt.second = static_cast<int>(rem % 60);
    // 1970-01-01 (day 0) was Thursday = 4; weekday is a 7-cycle from there.
    int wd = static_cast<int>((days + 4) % 7);
    if (wd < 0) {
        wd += 7;
    }
    dt.weekday = wd;
    return dt;
}

// Assemble a Unix timestamp from UTC calendar fields (inverse of fromUnix).
inline int64_t toUnix(const DateTime& dt) {
    const int64_t days = detail::daysFromCivil(dt.year, static_cast<unsigned>(dt.month),
                                               static_cast<unsigned>(dt.day));
    return days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

// ISO-8601 UTC string, "YYYY-MM-DDThh:mm:ssZ".
inline std::string formatIso(const DateTime& dt) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
    return buf;
}

// "YYYY-MM-DD" (Godot's Time.get_date_string_from_unix_time / get_date_string_from_datetime_dict).
inline std::string formatDate(const DateTime& dt) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", dt.year, dt.month, dt.day);
    return buf;
}

// "HH:MM:SS" (Godot's Time.get_time_string_from_unix_time).
inline std::string formatTime(const DateTime& dt) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
    return buf;
}

// "YYYY-MM-DD HH:MM:SS" (useSpace=true) or "...T..." (useSpace=false), no trailing 'Z' — Godot's
// Time.get_datetime_string_from_unix_time(use_space). (formatIso is the always-'T', 'Z'-suffixed form.)
inline std::string formatDateTime(const DateTime& dt, bool useSpace = true) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d%c%02d:%02d:%02d", dt.year, dt.month, dt.day,
                  useSpace ? ' ' : 'T', dt.hour, dt.minute, dt.second);
    return buf;
}

// Timezone offset in minutes -> "+HH:MM" / "-HH:MM" (Godot's Time.get_offset_string_from_offset_minutes).
inline std::string offsetString(int offsetMinutes) {
    const char sign = offsetMinutes < 0 ? '-' : '+';
    const int mag = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%c%02d:%02d", sign, mag / 60, mag % 60);
    return buf;
}

// Parse an ISO-8601 UTC string into `out` (the inverse of formatIso; Godot's
// Time.get_datetime_dict_from_datetime_string). Accepts a date-only "YYYY-MM-DD" (time = 00:00:00),
// or a full datetime with a 'T' or ' ' separator and an optional trailing 'Z' — e.g.
// "2026-07-18T09:30:00Z" or "2026-07-18 09:30:00". Validates field ranges (incl. leap-year day
// counts). Returns false and leaves `out` unchanged on a malformed string. The parsed weekday is
// filled in (via the Unix round-trip) so `out.weekday` is always correct.
inline bool parseIso(const std::string& s, DateTime& out) {
    int y = 0, mo = 0, da = 0, h = 0, mi = 0, se = 0;
    // %*c skips the T/space separator; n==3 for date-only, n==6 for full datetime.
#if defined(_MSC_VER)
#pragma warning(push)
// sscanf: the bounds-checked sscanf_s is MSVC-only, and every conversion here is a plain %d into
// an int, so there is no buffer to overrun. The return count is validated immediately below.
#pragma warning(disable : 4996)
#endif
    const int n = std::sscanf(s.c_str(), "%d-%d-%d%*c%d:%d:%d", &y, &mo, &da, &h, &mi, &se);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    if (n != 3 && n != 6) {
        return false;
    }
    if (mo < 1 || mo > 12 || da < 1 || da > daysInMonth(y, mo)) {
        return false;
    }
    if (h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 60) { // allow leap second 60
        return false;
    }
    DateTime dt;
    dt.year = y;
    dt.month = mo;
    dt.day = da;
    dt.hour = h;
    dt.minute = mi;
    dt.second = se;
    dt.weekday = fromUnix(toUnix(dt)).weekday; // normalise weekday deterministically
    out = dt;
    return true;
}

// Convenience: parse an ISO-8601 string straight to a Unix timestamp. Returns false on malformed
// input (Godot's Time.get_unix_time_from_datetime_string).
inline bool unixFromIso(const std::string& s, int64_t& outSeconds) {
    DateTime dt;
    if (!parseIso(s, dt)) {
        return false;
    }
    outSeconds = toUnix(dt);
    return true;
}

// A deterministic in-game clock: advance() by your fixed-step dt (scaled by a time-scale for fast-
// forward / slow-mo), then read the accumulated game time as days/hours or a 0..1 time-of-day.
// Never touches the system clock, so it replays identically.
class GameClock {
  public:
    void advance(double dt) { m_seconds += dt * m_scale; }
    void reset(double seconds = 0.0) { m_seconds = seconds; }

    void setTimeScale(double s) { m_scale = s; }
    double timeScale() const { return m_scale; }

    double totalSeconds() const { return m_seconds; }
    int64_t totalDays() const { return static_cast<int64_t>(m_seconds / 86400.0); }
    int hourOfDay() const {
        double s = secondsIntoDay();
        return static_cast<int>(s / 3600.0);
    }
    int minuteOfHour() const {
        double s = secondsIntoDay();
        return static_cast<int>(s / 60.0) % 60;
    }
    // Fraction through the current day, 0 at midnight -> 1 at the next midnight.
    double timeOfDay01() const { return secondsIntoDay() / 86400.0; }

  private:
    double secondsIntoDay() const {
        double s = m_seconds - static_cast<double>(totalDays()) * 86400.0;
        if (s < 0) {
            s += 86400.0;
        }
        return s;
    }
    double m_seconds = 0.0;
    double m_scale = 1.0;
};

} // namespace maz::core
