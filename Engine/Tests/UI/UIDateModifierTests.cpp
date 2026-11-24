// Suite: UIDateModifier (UI/Modifiers/UIDateModifier.h). A text shows the date of its canvas's
// DateSource along Format, in the text's first {} or instead of it; the widget is rebuilt only when
// that text changes.
#include <doctest.h>

#include "Core/Time/DateSource.h"
#include "UI/Modifiers/UIDateModifier.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIText.h"

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

    /** A ticked canvas reading a pinned date (Tuesday 24 November 2026, 09:05:07). */
    struct DateFixture
    {
        DateFixture()
        {
            Dates.Pin(MakeDate(2026, 11, 24, 9, 5, 7));
            Canvas.SetTargetSize(1920, 1080);
            Canvas.SetDates(&Dates);
            Canvas.Update();
        }

        UIText& AddText(const char* InText)
        {
            auto lOwned = NewObject<UIText>();
            lOwned->Text = InText;
            return static_cast<UIText&>(*Canvas.Root().AddChild(Move(lOwned)));
        }

        /** One frame as a game runs it: modifiers ticked, then what is dirty rebuilt. */
        UICanvasStats Frame()
        {
            Canvas.Tick(1.f / 60.f);
            return Canvas.Update();
        }

        DateSource Dates;
        UICanvas   Canvas{ 1080.f };
    };
}

TEST_CASE("UIDateModifier: the date replaces a text with no {}, and fills its {} when it has one")
{
    DateFixture lF;
    UIText& lPlain = lF.AddText("");
    lPlain.AddModifier<UIDateModifier>();
    UIText& lToday = lF.AddText("Today: {}");
    lToday.AddModifier<UIDateModifier>().SetFormat("{weekday}, {date}/{MM}/{yy}");
    lF.Frame();

    CHECK(lPlain.GetDisplayText() == "24 November 2026");
    CHECK(lToday.GetDisplayText() == "Today: Tuesday, 24/11/26");
    CHECK(lToday.Text == "Today: {}");   // what a save writes
}

TEST_CASE("UIDateModifier: an idle day rebuilds nothing; the next day rebuilds once")
{
    DateFixture lF;
    UIText& lText = lF.AddText("");
    lText.AddModifier<UIDateModifier>();
    lF.Frame();

    Int32 lRebuilds = 0;
    for (Int32 lFrame = 0; lFrame < 100; ++lFrame)
    {
        lRebuilds += lF.Frame().Rebuilds;
    }
    CHECK(lRebuilds == 0);

    // The clock moves within the day: a date-only Format does not change.
    lF.Dates.Pin(MakeDate(2026, 11, 24, 23, 59, 59));
    CHECK(lF.Frame().Rebuilds == 0);

    lF.Dates.Pin(MakeDate(2026, 11, 25));
    CHECK(lF.Frame().Rebuilds == 1);
    CHECK(lText.GetDisplayText() == "25 November 2026");
    CHECK(lF.Frame().Rebuilds == 0);
}

TEST_CASE("UIDateModifier: a Format showing the time rebuilds once per second, not within one")
{
    DateFixture lF;
    UIText& lText = lF.AddText("");
    lText.AddModifier<UIDateModifier>().SetFormat("{hh}:{mm}:{ss}");
    lF.Frame();
    CHECK(lText.GetDisplayText() == "09:05:07");

    DateTime lSame = MakeDate(2026, 11, 24, 9, 5, 7);
    lSame.Millisecond = 500;
    lF.Dates.Pin(lSame);
    CHECK(lF.Frame().Rebuilds == 0);

    lF.Dates.Pin(MakeDate(2026, 11, 24, 9, 5, 8));
    CHECK(lF.Frame().Rebuilds == 1);
    CHECK(lText.GetDisplayText() == "09:05:08");

    CHECK(DateFormatShowsTime("{hh}:{mm}"));
    CHECK(DateFormatShowsTime("{date} at {ss}"));
    CHECK_FALSE(DateFormatShowsTime("{weekday} {date} {month} {year}"));
    CHECK_FALSE(DateFormatShowsTime("{{hh}}"));
}

TEST_CASE("UIDateModifier: an edited Format shows at once, without a tick (the editor's preview)")
{
    DateFixture lF;
    UIText& lText = lF.AddText("");
    UIDateModifier& lDate = lText.AddModifier<UIDateModifier>();
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "24 November 2026");

    lDate.Format = "{year} {date} {month}";
    lDate.PostEditChange();
    CHECK(lF.Canvas.Update().Rebuilds == 1);
    CHECK(lText.GetDisplayText() == "2026 24 November");

    // Off: the text as written; on again: the date.
    lDate.bEnabled = false;
    lDate.PostEditChange();
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText().IsEmpty());

    lDate.bEnabled = true;
    lDate.PostEditChange();
    lF.Canvas.Update();
    CHECK(lText.GetDisplayText() == "2026 24 November");
}

TEST_CASE("UIDateModifier: an unknown token shows as written, a typo is seen on screen")
{
    DateFixture lF;
    UIText& lText = lF.AddText("");
    lText.AddModifier<UIDateModifier>().SetFormat("{dat} {month}");
    lF.Frame();
    CHECK(lText.GetDisplayText() == "{dat} November");
}

TEST_CASE("UIDateModifier: removed, the text is shown as written")
{
    DateFixture lF;
    UIText& lText = lF.AddText("Today: {}");
    UIDateModifier& lDate = lText.AddModifier<UIDateModifier>();
    lF.Frame();
    CHECK(lText.GetDisplayText() == "Today: 24 November 2026");

    TStrongObjectPtr<UIModifier> lBack = lText.RemoveModifier(lDate);
    lF.Frame();
    CHECK(lText.GetDisplayText() == "Today: {}");
}

TEST_CASE("UIDateModifier: without a DateSource on the canvas, the wall clock")
{
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);
    auto lOwned = NewObject<UIText>();
    UIText& lText = static_cast<UIText&>(*lCanvas.Root().AddChild(Move(lOwned)));
    lText.AddModifier<UIDateModifier>().SetFormat("{year}");
    lCanvas.Update();

    CHECK(lText.GetDisplayText() == DateTime::Now().Format("{year}"));
}
