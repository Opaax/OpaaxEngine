// Suite: DevConsoleOutput — the console's rows, each with its kind, capped; a text of several lines
// is a row each and an answer is indented JSON; the revision tells a view when to rebuild.
#include <doctest.h>

#include "DevConsole/DevConsoleOutput.h"

using namespace Opaax;

TEST_CASE("DevConsoleOutput: rows keep their kind, oldest first")
{
    DevConsoleOutput lOutput;
    lOutput.Add(EDevConsoleRow::Input, "> time.scale 0.25");
    lOutput.Add(EDevConsoleRow::Error, "time.scale needs scale");

    REQUIRE(lOutput.GetRows().size() == 2);
    CHECK(lOutput.GetRows()[0].Kind == EDevConsoleRow::Input);
    CHECK(lOutput.GetRows()[1].Kind == EDevConsoleRow::Error);
    CHECK(lOutput.GetRows()[1].Text == "time.scale needs scale");
    CHECK(OpaaxString(ToString(EDevConsoleRow::Warning)) == "warning");
}

TEST_CASE("DevConsoleOutput: a text of several lines is a row each; CR and tabs are cleaned")
{
    DevConsoleOutput lOutput;
    lOutput.Add(EDevConsoleRow::Info, "one\r\ntwo\n\tthree");

    REQUIRE(lOutput.GetRows().size() == 3);
    CHECK(lOutput.GetRows()[0].Text == "one");
    CHECK(lOutput.GetRows()[1].Text == "two");
    CHECK(lOutput.GetRows()[2].Text == "    three");

    lOutput.Add(EDevConsoleRow::Info, "");
    CHECK(lOutput.GetRows().size() == 4);   // an empty row: a gap
}

TEST_CASE("DevConsoleOutput: an answer is indented JSON over several rows; a string is text; an empty object nothing")
{
    DevConsoleOutput lOutput;
    lOutput.AddAnswer(nlohmann::json::object());
    lOutput.AddAnswer(nlohmann::json());
    CHECK(lOutput.GetRows().empty());

    lOutput.AddAnswer(nlohmann::json("done"));
    REQUIRE(lOutput.GetRows().size() == 1);
    CHECK(lOutput.GetRows()[0].Text == "done");
    CHECK(lOutput.GetRows()[0].Kind == EDevConsoleRow::Answer);

    lOutput.Clear();
    lOutput.AddAnswer(nlohmann::json{ { "scale", 0.25 }, { "paused", false } });
    REQUIRE(lOutput.GetRows().size() == 4);
    CHECK(lOutput.GetRows()[0].Text == "{");
    CHECK(lOutput.GetRows()[1].Text == "  \"paused\": false,");
    CHECK(lOutput.GetRows()[2].Text == "  \"scale\": 0.25");
    CHECK(lOutput.GetRows()[3].Text == "}");
}

TEST_CASE("DevConsoleOutput: past the most rows the oldest go")
{
    DevConsoleOutput lOutput(3);
    for (const char* lText : { "a", "b", "c", "d" })
    {
        lOutput.Add(EDevConsoleRow::Info, lText);
    }
    REQUIRE(lOutput.GetRows().size() == 3);
    CHECK(lOutput.GetRows().front().Text == "b");

    lOutput.SetMaxRows(1);
    REQUIRE(lOutput.GetRows().size() == 1);
    CHECK(lOutput.GetRows().front().Text == "d");

    lOutput.SetMaxRows(0);
    CHECK(lOutput.GetMaxRows() == 1);
}

TEST_CASE("DevConsoleOutput: every change bumps the revision")
{
    DevConsoleOutput lOutput;
    const Uint64     lStart = lOutput.GetRevision();

    lOutput.Add(EDevConsoleRow::Info, "a");
    const Uint64 lAfterAdd = lOutput.GetRevision();
    CHECK(lAfterAdd > lStart);

    lOutput.Clear();
    CHECK(lOutput.GetRevision() > lAfterAdd);
}
