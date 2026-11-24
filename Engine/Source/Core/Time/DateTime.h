#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    // =============================================================================
    // DateTime — a calendar date and time of day, in the local time zone: what a clock on the
    //   wall shows. For measures and timeouts, Clock (it never jumps).
    // =============================================================================
    struct DateTime
    {
        Int32 Year        = 1970;
        Int32 Month       = 1;    // 1 - 12
        Int32 Day         = 1;    // 1 - 31
        Int32 Hour        = 0;    // 0 - 23
        Int32 Minute      = 0;
        Int32 Second      = 0;
        Int32 Millisecond = 0;

        /** The local date and time now. */
        static DateTime Now();

        /** Whether InYear has a 29 February. */
        static bool IsLeapYear(Int32 InYear) noexcept;

        /** The days of InMonth (1 - 12) in InYear; 0 for a month that does not exist. */
        static Int32 DaysInMonth(Int32 InYear, Int32 InMonth) noexcept;

        /** Whether every field is in its range (the day within its month, a leap second allowed). */
        bool IsValid() const noexcept;

        /** The day of the week, 0 = Sunday to 6 = Saturday, worked out from the date. */
        Int32 GetWeekDay() const noexcept;

        /**
         * The date written along InPattern, whose {tokens} are replaced:
         *   {date} or {day} 4, {dd} 04, {month} November, {mon} Nov, {MM} 11, {year} 2026, {yy} 26,
         *   {weekday} Tuesday, {wd} Tue, {hh} {mm} {ss} 09 05 07 (24-hour clock).
         * Names ignore case, except the two-letter numbers (dd MM yy hh mm ss): {MM} is the month,
         * {mm} the minute. {{ and }} write a brace. An unknown token stays as written.
         * @param OutUnknown When given, receives the first unknown token as written ("{dat}"), or empty
         */
        OpaaxString Format(OpaaxStringView InPattern, OpaaxString* OutUnknown = nullptr) const;

        bool operator==(const DateTime& InOther) const noexcept
        {
            return Year == InOther.Year && Month == InOther.Month && Day == InOther.Day && Hour == InOther.Hour
                && Minute == InOther.Minute && Second == InOther.Second && Millisecond == InOther.Millisecond;
        }
        bool operator!=(const DateTime& InOther) const noexcept { return !(*this == InOther); }
    };

    // The names a date is written with, in English. One place, to be replaced by localised ones.
    namespace DateNames
    {
        /** "November" for 11; empty outside 1 - 12. */
        OpaaxStringView Month(Int32 InMonth) noexcept;
        /** "Nov" for 11; empty outside 1 - 12. */
        OpaaxStringView MonthShort(Int32 InMonth) noexcept;
        /** "Tuesday" for 2 (0 = Sunday); empty outside 0 - 6. */
        OpaaxStringView WeekDay(Int32 InWeekDay) noexcept;
        /** "Tue" for 2 (0 = Sunday); empty outside 0 - 6. */
        OpaaxStringView WeekDayShort(Int32 InWeekDay) noexcept;
    }
}
