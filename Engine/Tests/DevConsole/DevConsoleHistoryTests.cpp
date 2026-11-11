// Suite: DevConsoleHistory — the lines run, capped; Up and Down walk them with the line being typed
// kept as a draft; the text it is saved as reads back the same.
#include <doctest.h>

#include "DevConsole/DevConsoleHistory.h"

using namespace Opaax;

TEST_CASE("DevConsoleHistory: Up walks from the newest to the oldest, Down back to the draft")
{
    DevConsoleHistory lHistory;
    lHistory.Add("help");
    lHistory.Add("time.scale 0.25");
    lHistory.Add("world.pause true");

    OpaaxString lLine;
    REQUIRE(lHistory.Up("half typ", lLine));
    CHECK(lLine == "world.pause true");
    REQUIRE(lHistory.Up(lLine, lLine));
    CHECK(lLine == "time.scale 0.25");
    REQUIRE(lHistory.Up(lLine, lLine));
    CHECK(lLine == "help");
    CHECK_FALSE(lHistory.Up(lLine, lLine));   // the oldest
    CHECK(lLine == "help");

    REQUIRE(lHistory.Down(lLine));
    CHECK(lLine == "time.scale 0.25");
    REQUIRE(lHistory.Down(lLine));
    CHECK(lLine == "world.pause true");
    REQUIRE(lHistory.Down(lLine));
    CHECK(lLine == "half typ");               // the draft, given back
    CHECK_FALSE(lHistory.Down(lLine));        // not walking any more
}

TEST_CASE("DevConsoleHistory: an empty line or the same as the newest is not kept; spaces are trimmed")
{
    DevConsoleHistory lHistory;
    lHistory.Add("  help ");
    lHistory.Add("help");
    lHistory.Add("   ");
    lHistory.Add("");

    REQUIRE(lHistory.GetLines().size() == 1);
    CHECK(lHistory.GetLines()[0] == "help");

    OpaaxString lLine;
    CHECK_FALSE(DevConsoleHistory().Up("", lLine));   // nothing to walk
}

TEST_CASE("DevConsoleHistory: past the cap the oldest lines go")
{
    DevConsoleHistory lHistory(3);
    for (const char* lLine : { "a", "b", "c", "d", "e" })
    {
        lHistory.Add(lLine);
    }
    REQUIRE(lHistory.GetLines().size() == 3);
    CHECK(lHistory.GetLines()[0] == "c");
    CHECK(lHistory.GetLines()[2] == "e");

    lHistory.SetCapacity(1);
    REQUIRE(lHistory.GetLines().size() == 1);
    CHECK(lHistory.GetLines()[0] == "e");

    lHistory.SetCapacity(0);   // at least one
    CHECK(lHistory.GetCapacity() == 1);
}

TEST_CASE("DevConsoleHistory: running a line ends the walk, so the next Up starts from the newest")
{
    DevConsoleHistory lHistory;
    lHistory.Add("a");
    lHistory.Add("b");

    OpaaxString lLine;
    lHistory.Up("", lLine);
    lHistory.Up(lLine, lLine);
    CHECK(lLine == "a");

    lHistory.Add("a");
    REQUIRE(lHistory.Up("", lLine));
    CHECK(lLine == "a");   // the newest is now "a"
    REQUIRE(lHistory.Up(lLine, lLine));
    CHECK(lLine == "b");
}

TEST_CASE("DevConsoleHistory: the text round trip keeps the lines, and the cap, the newest kept")
{
    DevConsoleHistory lHistory;
    lHistory.Add("help");
    lHistory.Add("component.set Hero Transform {\"Position\": {\"x\": 5}}");

    const OpaaxString lText = lHistory.ToText();
    CHECK(lText == "help\ncomponent.set Hero Transform {\"Position\": {\"x\": 5}}\n");

    DevConsoleHistory lRead;
    lRead.FromText(lText);
    CHECK(lRead.GetLines() == lHistory.GetLines());

    DevConsoleHistory lSmall(2);
    lSmall.FromText("one\r\ntwo\n\n  three  \n");
    REQUIRE(lSmall.GetLines().size() == 2);
    CHECK(lSmall.GetLines()[0] == "two");
    CHECK(lSmall.GetLines()[1] == "three");
}
