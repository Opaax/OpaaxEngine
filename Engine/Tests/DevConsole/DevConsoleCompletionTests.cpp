// Suite: CompleteCommandText — Tab in the console: a command's name first, then a choice of the
// parameter the word fills; one match written whole, several as far as they agree, and listed.
#include <doctest.h>

#include "DevConsole/DevConsoleCompletion.h"

using namespace Opaax;

namespace
{
    TDynArray<CommandSpec> Specs()
    {
        TDynArray<CommandSpec> lSpecs;
        lSpecs.push_back(CommandSpec("help", "Lists the commands."));
        lSpecs.push_back(CommandSpec("input.key", "Feeds a key.")
                             .Param("key", ECommandParamType::String, "the key")
                             .Param("action", ECommandParamType::String, "what it does")
                                 .Choices({ "press", "release", "tap" }).Optional());
        lSpecs.push_back(CommandSpec("input.keys", "Lists the keys."));
        lSpecs.push_back(CommandSpec("input.text", "Types text."));
        lSpecs.push_back(CommandSpec("online.use", "Runs a store.")
                             .Param("backend", ECommandParamType::String, "the store").Choices({ "none", "fake" })
                             .Param("profile", ECommandParamType::String, "its profile")
                                 .Choices({ "full", "steam", "epic", "gog" }).Optional());
        lSpecs.push_back(CommandSpec("time.pace", "Paces the frames."));
        lSpecs.push_back(CommandSpec("time.scale", "Slows the game."));
        return lSpecs;
    }
}

TEST_CASE("CompleteCommandText: one name that fits is written whole, with a space after it")
{
    const DevConsoleCompletion lOne = CompleteCommandText("hel", Specs());
    CHECK(lOne.Line == "help ");
    CHECK(lOne.Matches.empty());

    const DevConsoleCompletion lScale = CompleteCommandText("time.s", Specs());
    CHECK(lScale.Line == "time.scale ");
}

TEST_CASE("CompleteCommandText: several names are completed as far as they agree, and listed")
{
    const DevConsoleCompletion lInput = CompleteCommandText("in", Specs());
    CHECK(lInput.Line == "input.");
    REQUIRE(lInput.Matches.size() == 3);
    CHECK(lInput.Matches[0] == "input.key");

    const DevConsoleCompletion lKey = CompleteCommandText("input.k", Specs());
    CHECK(lKey.Line == "input.key");   // input.key and input.keys agree that far
    CHECK(lKey.Matches.size() == 2);

    // Nothing typed: every command listed, the line unchanged.
    const DevConsoleCompletion lAll = CompleteCommandText("", Specs());
    CHECK(lAll.Line == "");
    CHECK(lAll.Matches.size() == 7);
}

TEST_CASE("CompleteCommandText: case does not matter; the name's own case is written")
{
    CHECK(CompleteCommandText("TIME.SC", Specs()).Line == "time.scale ");
    CHECK(CompleteCommandText("online.use fake St", Specs()).Line == "online.use fake steam ");
}

TEST_CASE("CompleteCommandText: a later word is completed among the choices of the parameter it fills")
{
    CHECK(CompleteCommandText("online.use f", Specs()).Line == "online.use fake ");
    CHECK(CompleteCommandText("online.use fake g", Specs()).Line == "online.use fake gog ");

    // The key parameter has no choices; the action after it has.
    CHECK(CompleteCommandText("input.key Space t", Specs()).Line == "input.key Space tap ");

    const DevConsoleCompletion lChoices = CompleteCommandText("online.use fake ", Specs());
    CHECK(lChoices.Line == "online.use fake ");
    CHECK(lChoices.Matches.size() == 4);
}

TEST_CASE("CompleteCommandText: key=value completes that key's choices, and a named key is skipped by the plain words")
{
    CHECK(CompleteCommandText("online.use profile=e", Specs()).Line == "online.use profile=epic ");

    // backend named: the first plain word fills profile.
    CHECK(CompleteCommandText("online.use backend=fake s", Specs()).Line == "online.use backend=fake steam ");
}

TEST_CASE("CompleteCommandText: nothing that fits leaves the line as it is")
{
    for (const char* lLine : { "zzz", "unknown.command a", "time.scale 0.2", "input.key Space x",
                               "input.key Space tap extra", "say \"quo", "component.set {\"a" })
    {
        CAPTURE(lLine);
        const DevConsoleCompletion lNone = CompleteCommandText(lLine, Specs());
        CHECK(lNone.Line == lLine);
        CHECK(lNone.Matches.empty());
    }
}
