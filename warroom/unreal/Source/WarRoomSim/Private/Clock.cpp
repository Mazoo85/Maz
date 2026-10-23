#include "WarRoomSim/Clock.h"

#include <cstdio>

namespace warroom {

const double kSpeedHoursPerSecond[6] = {0, 1, 2, 6, 12, 48};
static const char* kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char* kMonthsLong[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

bool IsLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int DaysInMonth(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2) return IsLeapYear(y) ? 29 : 28;
    return d[m - 1];
}

static int DaysInYear(int y) { return IsLeapYear(y) ? 366 : 365; }

static long long DateToDay(int year, int month, int day) {
    long long days = 0;
    if (year >= kStartYear) { for (int y = kStartYear; y < year; y++) days += DaysInYear(y); }
    else { for (int y = year; y < kStartYear; y++) days -= DaysInYear(y); }
    for (int m = 1; m < month; m++) days += DaysInMonth(year, m);
    return days + (day - 1);
}

long long DateToHour(int year, int month, int day, int hour) { return DateToDay(year, month, day) * 24 + hour; }

Date HourToDate(long long hourIndex) {
    long long days = hourIndex / 24;
    if (hourIndex < 0 && hourIndex % 24 != 0) days -= 1;   // floor division
    int hour = static_cast<int>(hourIndex - days * 24);
    int year = kStartYear;
    while (days >= DaysInYear(year)) { days -= DaysInYear(year); year++; }
    while (days < 0) { year--; days += DaysInYear(year); }
    Date d;
    d.year = year; d.hour = hour; d.dayOfYear = static_cast<int>(days) + 1;
    int month = 1;
    while (days >= DaysInMonth(year, month)) { days -= DaysInMonth(year, month); month++; }
    d.month = month; d.day = static_cast<int>(days) + 1;
    long long totalDays = hourIndex / 24; if (hourIndex < 0 && hourIndex % 24 != 0) totalDays -= 1;
    d.weekday = static_cast<int>((((totalDays + 2) % 7) + 7) % 7);
    return d;
}

std::string FormatDate(long long hourIndex, bool longMonth) {
    Date d = HourToDate(hourIndex);
    char buf[64];
    snprintf(buf, sizeof buf, "%d %s %d, %02d:00", d.day, (longMonth ? kMonthsLong : kMonths)[d.month - 1], d.year, d.hour);
    return buf;
}

bool IsNewDay(long long h) { return h % 24 == 0; }
bool IsNewMonth(long long h) { return h % 24 == 0 && HourToDate(h).day == 1; }
bool IsNewYear(long long h) { if (h % 24 != 0) return false; Date d = HourToDate(h); return d.day == 1 && d.month == 1; }

}  // namespace warroom
