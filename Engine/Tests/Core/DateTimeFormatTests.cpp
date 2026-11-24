// Suite: DateTime::Format (a date written along a pattern of {tokens}), the week day and the
// calendar helpers it rests on.
#include <doctest.h>

#include "Core/OpaaxTypes.h"
#include "Core/Time/DateTime.h"

using namespace Opaax;

namespace
{
    DateTime MakeDate(const Int32 InYear, const Int32 InMonth, const Int32 InDay,
                      const Int32 InHour = 0, const Int32 InMinute = 0, const Int32 InSecond = 0)
    {
        DateTime lDate;
        lDate.Year   = InYear;
        lDate.Month  = InMonth;
        lDate.Day    = InDay;
        lDate.Hour   = InHour;
        lDate.Minute = InMinute;
        lDate.Second = InSecond;
        return lDate;
    }

    // Tuesday 24 November 2026, 09:05:07.
    const DateTime TUESDAY = MakeDate(2026, 11, 24, 9, 5, 7);
}

TEST_CASE("DateTime::Format: each token")
{
    CHECK(TUESDAY.Format("{date}")    == "24");
    CHECK(TUESDAY.Format("{day}")     == "24");
    CHECK(TUESDAY.Format("{dd}")      == "24");
    CHECK(TUESDAY.Format("{month}")   == "November");
    CHECK(TUESDAY.Format("{mon}")     == "Nov");
    CHECK(TUESDAY.Format("{MM}")      == "11");
    CHECK(TUESDAY.Format("{year}")    == "2026");
    CHECK(TUESDAY.Format("{yy}")      == "26");
    CHECK(TUESDAY.Format("{weekday}") == "Tuesday");
    CHECK(TUESDAY.Format("{wd}")      == "Tue");
    CHECK(TUESDAY.Format("{hh}")      == "09");
    CHECK(TUESDAY.Format("{mm}")      == "05");
    CHECK(TUESDAY.Format("{ss}")      == "07");
}

TEST_CASE("DateTime::Format: patterns in any order, with text between")
{
    CHECK(TUESDAY.Format("{date} {month} {year}")         == "24 November 2026");
    CHECK(TUESDAY.Format("{year} {date} {month}")         == "2026 24 November");
    CHECK(TUESDAY.Format("{weekday}, {date}/{MM}/{yy}")   == "Tuesday, 24/11/26");
    CHECK(TUESDAY.Format("{hh}:{mm}:{ss}")                == "09:05:07");
    CHECK(TUESDAY.Format("Today is {wd} the {dd}.")       == "Today is Tue the 24.");
    CHECK(TUESDAY.Format("no token at all")               == "no token at all");
}

TEST_CASE("DateTime::Format: numbers padded to two digits, the day name alone is not")
{
    const DateTime lEarly = MakeDate(2009, 3, 4, 0, 0, 0);
    CHECK(lEarly.Format("{date}") == "4");
    CHECK(lEarly.Format("{dd}")   == "04");
    CHECK(lEarly.Format("{MM}")   == "03");
    CHECK(lEarly.Format("{yy}")   == "09");
    CHECK(lEarly.Format("{hh}:{mm}:{ss}") == "00:00:00");
}

TEST_CASE("DateTime::Format: names ignore case, the two-letter numbers do not")
{
    CHECK(TUESDAY.Format("{MONTH}")   == "November");   // the name is matched, not the output's case
    CHECK(TUESDAY.Format("{Weekday}") == "Tuesday");
    CHECK(TUESDAY.Format("{WD}")      == "Tue");
    CHECK(TUESDAY.Format("{Date}")    == "24");

    // MM is the month and mm the minute: any other spelling is no token.
    CHECK(TUESDAY.Format("{MM}") == "11");
    CHECK(TUESDAY.Format("{mm}") == "05");
    CHECK(TUESDAY.Format("{Mm}") == "{Mm}");
    CHECK(TUESDAY.Format("{DD}") == "{DD}");
    CHECK(TUESDAY.Format("{SS}") == "{SS}");
}

TEST_CASE("DateTime::Format: doubled braces write one, lone ones stay")
{
    CHECK(TUESDAY.Format("{{date}}")        == "{date}");
    CHECK(TUESDAY.Format("{{{date}}}")      == "{24}");
    CHECK(TUESDAY.Format("a } b")           == "a } b");
    CHECK(TUESDAY.Format("open { {date}")   == "open { 24");
    CHECK(TUESDAY.Format("{date")           == "{date");
    CHECK(TUESDAY.Format("{a{date}")        == "{a24");
}

TEST_CASE("DateTime::Format: an unknown token stays as written and is reported")
{
    OpaaxString lUnknown;
    CHECK(TUESDAY.Format("{dat} {month} {yaer}", &lUnknown) == "{dat} November {yaer}");
    CHECK(lUnknown == "{dat}");   // the first one

    CHECK(TUESDAY.Format("{date} {month}", &lUnknown) == "24 November");
    CHECK(lUnknown.IsEmpty());    // cleared when all are known

    CHECK(TUESDAY.Format("{}", &lUnknown) == "{}");
    CHECK(lUnknown == "{}");
}

TEST_CASE("DateTime::Format: an empty pattern gives an empty text")
{
    OpaaxString lUnknown("left over");
    CHECK(TUESDAY.Format("", &lUnknown).IsEmpty());
    CHECK(lUnknown.IsEmpty());
}

TEST_CASE("DateTime::Format: 29 February of a leap year")
{
    const DateTime lLeapDay = MakeDate(2028, 2, 29);
    CHECK(lLeapDay.IsValid());
    CHECK(lLeapDay.Format("{weekday} {date} {month} {year}") == "Tuesday 29 February 2028");
    CHECK(lLeapDay.Format("{dd}/{MM}/{yy}") == "29/02/28");

    CHECK_FALSE(MakeDate(2026, 2, 29).IsValid());   // not a leap year
}

TEST_CASE("DateTime::Format: the last second of a year and the first of the next")
{
    const DateTime lLast  = MakeDate(2026, 12, 31, 23, 59, 59);
    const DateTime lFirst = MakeDate(2027, 1, 1, 0, 0, 0);

    CHECK(lLast.Format("{weekday} {date} {month} {year} {hh}:{mm}:{ss}")  == "Thursday 31 December 2026 23:59:59");
    CHECK(lFirst.Format("{weekday} {date} {month} {year} {hh}:{mm}:{ss}") == "Friday 1 January 2027 00:00:00");
    CHECK(lFirst.Format("{dd}/{MM}/{yy}") == "01/01/27");
}

TEST_CASE("DateTime: week days and month lengths")
{
    CHECK(MakeDate(1970, 1, 1).GetWeekDay()   == 4);   // Thursday
    CHECK(MakeDate(2000, 1, 1).GetWeekDay()   == 6);   // Saturday
    CHECK(MakeDate(2000, 2, 29).GetWeekDay()  == 2);   // Tuesday
    CHECK(MakeDate(2000, 3, 1).GetWeekDay()   == 3);   // Wednesday: right after a leap day
    CHECK(MakeDate(2026, 11, 22).GetWeekDay() == 0);   // Sunday
    CHECK(TUESDAY.GetWeekDay()                == 2);

    CHECK(DateTime::IsLeapYear(2024));
    CHECK(DateTime::IsLeapYear(2000));
    CHECK_FALSE(DateTime::IsLeapYear(1900));
    CHECK_FALSE(DateTime::IsLeapYear(2026));

    CHECK(DateTime::DaysInMonth(2026, 2)  == 28);
    CHECK(DateTime::DaysInMonth(2028, 2)  == 29);
    CHECK(DateTime::DaysInMonth(2026, 4)  == 30);
    CHECK(DateTime::DaysInMonth(2026, 12) == 31);
    CHECK(DateTime::DaysInMonth(2026, 13) == 0);

    CHECK_FALSE(MakeDate(2026, 4, 31).IsValid());
    CHECK_FALSE(MakeDate(2026, 0, 1).IsValid());
    CHECK_FALSE(MakeDate(2026, 1, 1, 24).IsValid());
    CHECK(DateTime::Now().IsValid());
}
