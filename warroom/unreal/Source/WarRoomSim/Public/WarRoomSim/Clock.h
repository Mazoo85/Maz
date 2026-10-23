// The game clock: one tick is one hour, hour 0 is 1 January 1936, 00:00.
#pragma once
#include "Api.h"
#include <string>

namespace warroom {

struct Date {
    int year = 1936, month = 1, day = 1, hour = 0;
    int dayOfYear = 1;
    int weekday = 2;   // Monday = 0; 1 Jan 1936 was a Wednesday
};

constexpr int kStartYear = 1936;
extern WARROOMSIM_API const double kSpeedHoursPerSecond[6];   // index = speed 0..5

WARROOMSIM_API bool IsLeapYear(int year);
WARROOMSIM_API int DaysInMonth(int year, int month);
WARROOMSIM_API long long DateToHour(int year, int month, int day, int hour = 0);
WARROOMSIM_API Date HourToDate(long long hour);
WARROOMSIM_API std::string FormatDate(long long hour, bool longMonth = false);
WARROOMSIM_API bool IsNewDay(long long hour);
WARROOMSIM_API bool IsNewMonth(long long hour);
WARROOMSIM_API bool IsNewYear(long long hour);

}  // namespace warroom
