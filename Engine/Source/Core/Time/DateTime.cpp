#include "Core/Time/DateTime.h"

#include <chrono>
#include <ctime>

#include "Core/EngineAPI.h"
#include "Core/String/Char.h"

namespace Opaax
{
    // =============================================================================
    // Names
    // =============================================================================

    namespace DateNames
    {
        namespace
        {
            constexpr OpaaxStringView MONTHS[12] = { "January", "February", "March", "April", "May", "June", "July",
                                                     "August", "September", "October", "November", "December" };
            constexpr OpaaxStringView MONTHS_SHORT[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            constexpr OpaaxStringView WEEK_DAYS[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
                                                       "Friday", "Saturday" };
            constexpr OpaaxStringView WEEK_DAYS_SHORT[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        }

        OpaaxStringView Month(const Int32 InMonth) noexcept
        {
            return InMonth >= 1 && InMonth <= 12 ? MONTHS[InMonth - 1] : OpaaxStringView();
        }

        OpaaxStringView MonthShort(const Int32 InMonth) noexcept
        {
            return InMonth >= 1 && InMonth <= 12 ? MONTHS_SHORT[InMonth - 1] : OpaaxStringView();
        }

        OpaaxStringView WeekDay(const Int32 InWeekDay) noexcept
        {
            return InWeekDay >= 0 && InWeekDay <= 6 ? WEEK_DAYS[InWeekDay] : OpaaxStringView();
        }

        OpaaxStringView WeekDayShort(const Int32 InWeekDay) noexcept
        {
            return InWeekDay >= 0 && InWeekDay <= 6 ? WEEK_DAYS_SHORT[InWeekDay] : OpaaxStringView();
        }
    }

    // =============================================================================
    // Calendar
    // =============================================================================

    DateTime DateTime::Now()
    {
        using namespace std::chrono;

        const system_clock::time_point lNow     = system_clock::now();
        const std::time_t              lSeconds = system_clock::to_time_t(lNow);

        std::tm lLocal{};
#if defined(OPAAX_PLATFORM_WINDOWS)
        localtime_s(&lLocal, &lSeconds);
#else
        localtime_r(&lSeconds, &lLocal);
#endif

        DateTime lResult;
        lResult.Year        = lLocal.tm_year + 1900;
        lResult.Month       = lLocal.tm_mon + 1;
        lResult.Day         = lLocal.tm_mday;
        lResult.Hour        = lLocal.tm_hour;
        lResult.Minute      = lLocal.tm_min;
        lResult.Second      = lLocal.tm_sec;
        lResult.Millisecond = static_cast<Int32>(duration_cast<milliseconds>(lNow.time_since_epoch()).count() % 1000);
        return lResult;
    }

    bool DateTime::IsLeapYear(const Int32 InYear) noexcept
    {
        return (InYear % 4 == 0 && InYear % 100 != 0) || InYear % 400 == 0;
    }

    Int32 DateTime::DaysInMonth(const Int32 InYear, const Int32 InMonth) noexcept
    {
        constexpr Int32 DAYS[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (InMonth < 1 || InMonth > 12)
        {
            return 0;
        }
        return InMonth == 2 && IsLeapYear(InYear) ? 29 : DAYS[InMonth - 1];
    }

    bool DateTime::IsValid() const noexcept
    {
        return Month >= 1 && Month <= 12
            && Day >= 1 && Day <= DaysInMonth(Year, Month)
            && Hour >= 0 && Hour <= 23
            && Minute >= 0 && Minute <= 59
            && Second >= 0 && Second <= 60
            && Millisecond >= 0 && Millisecond <= 999;
    }

    Int32 DateTime::GetWeekDay() const noexcept
    {
        // Sakamoto's method: the year counts from March, so February's leap day ends it.
        constexpr Int32 OFFSETS[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
        if (Month < 1 || Month > 12)
        {
            return 0;
        }

        const Int32 lYear = Month < 3 ? Year - 1 : Year;
        const Int32 lDay  = (lYear + lYear / 4 - lYear / 100 + lYear / 400 + OFFSETS[Month - 1] + Day) % 7;
        return lDay < 0 ? lDay + 7 : lDay;
    }

    // =============================================================================
    // Format
    // =============================================================================

    namespace
    {
        /** InName in lower case equals InLower (already lower case). */
        bool EqualsIgnoreCase(const OpaaxStringView InName, const OpaaxStringView InLower) noexcept
        {
            if (InName.GetLength() != InLower.GetLength())
            {
                return false;
            }
            for (Uint32 lIndex = 0; lIndex < InName.GetLength(); ++lIndex)
            {
                if (Char::ToLower(InName[lIndex]) != InLower[lIndex])
                {
                    return false;
                }
            }
            return true;
        }

        /** Appends what the token InName gives for InDate; false when no token has that name. */
        bool AppendToken(const DateTime& InDate, const OpaaxStringView InName, OpaaxString& OutText)
        {
            // Two-letter numbers: the case tells the month (MM) from the minute (mm).
            if (InName == "dd") { OutText += OpaaxString::Format("{:02}", InDate.Day);         return true; }
            if (InName == "MM") { OutText += OpaaxString::Format("{:02}", InDate.Month);       return true; }
            if (InName == "yy") { OutText += OpaaxString::Format("{:02}", InDate.Year % 100);  return true; }
            if (InName == "hh") { OutText += OpaaxString::Format("{:02}", InDate.Hour);        return true; }
            if (InName == "mm") { OutText += OpaaxString::Format("{:02}", InDate.Minute);      return true; }
            if (InName == "ss") { OutText += OpaaxString::Format("{:02}", InDate.Second);      return true; }

            if (EqualsIgnoreCase(InName, "date") || EqualsIgnoreCase(InName, "day"))
            {
                OutText += OpaaxString::FromInt(InDate.Day);
                return true;
            }
            if (EqualsIgnoreCase(InName, "month"))   { OutText += DateNames::Month(InDate.Month);      return true; }
            if (EqualsIgnoreCase(InName, "mon"))     { OutText += DateNames::MonthShort(InDate.Month); return true; }
            if (EqualsIgnoreCase(InName, "year"))    { OutText += OpaaxString::Format("{:04}", InDate.Year); return true; }
            if (EqualsIgnoreCase(InName, "weekday")) { OutText += DateNames::WeekDay(InDate.GetWeekDay());      return true; }
            if (EqualsIgnoreCase(InName, "wd"))      { OutText += DateNames::WeekDayShort(InDate.GetWeekDay()); return true; }
            return false;
        }
    }

    OpaaxString DateTime::Format(const OpaaxStringView InPattern, OpaaxString* OutUnknown) const
    {
        if (OutUnknown != nullptr)
        {
            OutUnknown->Clear();
        }

        OpaaxString lResult;
        lResult.Reserve(InPattern.GetLength() + 16);

        const Uint32 lLength = InPattern.GetLength();
        Uint32       lAt     = 0;
        while (lAt < lLength)
        {
            const char lChar = InPattern[lAt];
            const bool lDoubled = lAt + 1 < lLength && InPattern[lAt + 1] == lChar;

            if (lChar == '}')
            {
                lResult += '}';
                lAt += lDoubled ? 2 : 1;   // }} is one brace; a lone one stays
                continue;
            }
            if (lChar != '{')
            {
                lResult += lChar;
                ++lAt;
                continue;
            }
            if (lDoubled)
            {
                lResult += '{';
                lAt += 2;
                continue;
            }

            // A token: up to the next }, or the brace stays as written when another { comes first.
            const Int32 lClose = InPattern.FindFirstOf("{}", lAt + 1);
            if (lClose < 0 || InPattern[static_cast<Uint32>(lClose)] == '{')
            {
                lResult += '{';
                ++lAt;
                continue;
            }

            const Uint32          lEnd  = static_cast<Uint32>(lClose);
            const OpaaxStringView lName = InPattern.SubString(lAt + 1, lEnd - lAt - 1);
            if (!AppendToken(*this, lName, lResult))
            {
                const OpaaxStringView lWritten = InPattern.SubString(lAt, lEnd - lAt + 1);
                lResult += lWritten;
                if (OutUnknown != nullptr && OutUnknown->IsEmpty())
                {
                    *OutUnknown = OpaaxString(lWritten);
                }
            }
            lAt = lEnd + 1;
        }
        return lResult;
    }
}
