// Suite: CommandText — a typed line read into a command and its JSON params: words in order,
// key=value, quotes, JSON values and a JSON object; each error names what is wrong.
#include <doctest.h>

#include "Commands/CommandRegistry.h"
#include "Commands/CommandText.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringJson.h"

using namespace Opaax;

namespace
{
    CommandResult Nothing(const nlohmann::json&) { return CommandResult::Ok(); }

    /** A registry with commands shaped like the engine's. */
    struct Fixture
    {
        CommandRegistry Commands;

        Fixture()
        {
            Commands.Add(CommandSpec("time.scale", "Slows the game.")
                             .Param("scale", ECommandParamType::Float, "game seconds per real second"), Nothing);
            Commands.Add(CommandSpec("world.pause", "Pauses.")
                             .Param("paused", ECommandParamType::Bool, "true pauses"), Nothing);
            Commands.Add(CommandSpec("level.play", "Plays a level.")
                             .Param("path", ECommandParamType::String, "the level's path"), Nothing);
            Commands.Add(CommandSpec("input.key", "Feeds a key.")
                             .Param("key", ECommandParamType::String, "the key")
                             .Param("action", ECommandParamType::String, "what it does")
                                 .Choices({ "press", "release", "tap" }).Optional()
                             .Param("frames", ECommandParamType::Int, "how long").Optional(), Nothing);
            Commands.Add(CommandSpec("bhop.teleport", "Moves the rabbit.")
                             .Param("x", ECommandParamType::Any, "where, or the x position")
                                 .Choices({ "start", "finish", "cursor" })
                             .Param("y", ECommandParamType::Float, "the y position").Optional(), Nothing);
            Commands.Add(CommandSpec("component.set", "Sets fields.")
                             .Param("entity", ECommandParamType::String, "the entity")
                             .Param("type", ECommandParamType::String, "the component")
                             .Param("value", ECommandParamType::Any, "the fields"), Nothing);
            Commands.Add("game.free", "Declares nothing.", Nothing);
        }

        CommandTextResult Parse(const char* InLine) const { return ParseCommandText(InLine, Commands); }
    };
}

// =============================================================================
// Words
// =============================================================================
TEST_CASE("CommandText: words split on spaces; quotes and brackets stay whole")
{
    TDynArray<CommandWord> lWords;
    OpaaxString            lError;
    REQUIRE(SplitCommandText(R"(  say  "two words"  {"a": 1, "b": [2, 3]}  key=value  key2="x y" )", lWords, lError));

    REQUIRE(lWords.size() == 5);
    CHECK(lWords[0].Text == "say");
    CHECK(lWords[1].Text == "two words");
    CHECK(lWords[1].bQuoted);
    CHECK(lWords[2].Text == R"({"a": 1, "b": [2, 3]})");
    CHECK(lWords[3].Key == "key");
    CHECK(lWords[3].Text == "value");
    CHECK(lWords[4].Key == "key2");
    CHECK(lWords[4].Text == "x y");
    CHECK(lWords[4].bQuoted);
}

TEST_CASE("CommandText: a quote reads JSON escapes; an open quote or bracket is an error that shows it")
{
    TDynArray<CommandWord> lWords;
    OpaaxString            lError;
    const OpaaxString lEscaped(R"(say "a \"quoted\" word")");
    REQUIRE(SplitCommandText(lEscaped, lWords, lError));
    const OpaaxString lUnescaped(R"(a "quoted" word)");
    CHECK(lWords[1].Text == lUnescaped);

    CHECK_FALSE(SplitCommandText(R"(say "never closed)", lWords, lError));
    CHECK(lError.Contains("quote is not closed"));

    CHECK_FALSE(SplitCommandText(R"(say {"a": 1)", lWords, lError));
    CHECK(lError.Contains("not closed"));

    CHECK_FALSE(SplitCommandText("say }", lWords, lError));
    CHECK(lError.Contains("closes nothing"));
}

// =============================================================================
// Params
// =============================================================================
TEST_CASE("CommandText: words fill the declared parameters in order, each read as its type")
{
    const Fixture lFix;

    const CommandTextResult lScale = lFix.Parse("time.scale 0.25");
    REQUIRE(lScale.bOk);
    CHECK(lScale.Command == "time.scale");
    CHECK(lScale.Params == nlohmann::json{ { "scale", 0.25 } });

    const CommandTextResult lPause = lFix.Parse("world.pause true");
    REQUIRE(lPause.bOk);
    CHECK(lPause.Params == nlohmann::json{ { "paused", true } });

    const CommandTextResult lKey = lFix.Parse("input.key Space tap 4");
    REQUIRE(lKey.bOk);
    CHECK(lKey.Params == nlohmann::json{ { "key", "Space" }, { "action", "tap" }, { "frames", 4 } });
}

TEST_CASE("CommandText: a String parameter keeps the word as text, even one that reads as JSON")
{
    const Fixture lFix;

    const CommandTextResult lPath = lFix.Parse("level.play Levels/Main.opaaxlevel");
    REQUIRE(lPath.bOk);
    CHECK(lPath.Params == nlohmann::json{ { "path", "Levels/Main.opaaxlevel" } });

    const CommandTextResult lNumber = lFix.Parse("level.play 42");
    REQUIRE(lNumber.bOk);
    CHECK(lNumber.Params.at("path") == "42");
}

TEST_CASE("CommandText: key=value names a parameter; the plain words fill the others in order")
{
    const Fixture lFix;

    const CommandTextResult lNamed = lFix.Parse("input.key frames=3 Space");
    REQUIRE(lNamed.bOk);
    CHECK(lNamed.Params == nlohmann::json{ { "key", "Space" }, { "frames", 3 } });
}

TEST_CASE("CommandText: an Any parameter takes a choice or a value; a quoted word is always text")
{
    const Fixture lFix;

    const CommandTextResult lChoice = lFix.Parse("bhop.teleport finish");
    REQUIRE(lChoice.bOk);
    CHECK(lChoice.Params == nlohmann::json{ { "x", "finish" } });

    const CommandTextResult lPosition = lFix.Parse("bhop.teleport 120 40");
    REQUIRE(lPosition.bOk);
    CHECK(lPosition.Params == nlohmann::json{ { "x", 120 }, { "y", 40 } });

    const CommandTextResult lQuoted = lFix.Parse(R"(bhop.teleport "120")");
    REQUIRE(lQuoted.bOk);
    CHECK(lQuoted.Params.at("x") == "120");
}

TEST_CASE("CommandText: JSON values and a lone JSON object, as a script sends them")
{
    const Fixture lFix;

    const CommandTextResult lValue = lFix.Parse(R"(component.set Hero Transform {"Position": {"x": 5}})");
    REQUIRE(lValue.bOk);
    CHECK(lValue.Params.at("value") == nlohmann::json{ { "Position", { { "x", 5 } } } });

    const CommandTextResult lWhole = lFix.Parse(R"(component.set {"entity": "Hero", "type": "Transform", "value": {}})");
    REQUIRE(lWhole.bOk);
    CHECK(lWhole.Params == nlohmann::json{ { "entity", "Hero" }, { "type", "Transform" }, { "value", nlohmann::json::object() } });
}

TEST_CASE("CommandText: a command that declares nothing takes any key=value, and no plain word")
{
    const Fixture lFix;

    const CommandTextResult lNamed = lFix.Parse(R"(game.free count=3 label="a b" flag=true)");
    REQUIRE(lNamed.bOk);
    CHECK(lNamed.Params == nlohmann::json{ { "count", 3 }, { "label", "a b" }, { "flag", true } });

    const CommandTextResult lPlain = lFix.Parse("game.free 3");
    CHECK_FALSE(lPlain.bOk);
    CHECK(lPlain.Error.Contains("declares no parameters"));
}

// =============================================================================
// Errors
// =============================================================================
TEST_CASE("CommandText: an unknown command is named")
{
    const Fixture           lFix;
    const CommandTextResult lUnknown = lFix.Parse("time.scael 0.25");
    CHECK_FALSE(lUnknown.bOk);
    CHECK(lUnknown.Error.Contains("unknown command 'time.scael'"));

    CHECK_FALSE(lFix.Parse("   ").bOk);
    CHECK_FALSE(lFix.Parse("x=1").bOk);
}

TEST_CASE("CommandText: a missing parameter is named with its help and the usage")
{
    const Fixture           lFix;
    const CommandTextResult lMissing = lFix.Parse("time.scale");
    CHECK_FALSE(lMissing.bOk);
    CHECK(lMissing.Error.Contains("needs scale"));
    CHECK(lMissing.Error.Contains("game seconds per real second"));
    CHECK(lMissing.Error.Contains("time.scale <scale:float>"));
}

TEST_CASE("CommandText: a wrong type, a wrong choice, an unknown key and one word too many are named")
{
    const Fixture lFix;

    const CommandTextResult lType = lFix.Parse("time.scale slow");
    CHECK_FALSE(lType.bOk);
    CHECK(lType.Error.Contains("scale is a number, not slow"));

    const CommandTextResult lBool = lFix.Parse("world.pause yes");
    CHECK_FALSE(lBool.bOk);
    CHECK(lBool.Error.Contains("true or false"));

    const CommandTextResult lInt = lFix.Parse("input.key Space tap 2.5");
    CHECK_FALSE(lInt.bOk);
    CHECK(lInt.Error.Contains("frames is a whole number"));

    const CommandTextResult lChoice = lFix.Parse("input.key Space hold");
    CHECK_FALSE(lChoice.bOk);
    CHECK(lChoice.Error.Contains("action is one of press, release, tap, not hold"));

    const CommandTextResult lKey = lFix.Parse("time.scale speed=2");
    CHECK_FALSE(lKey.bOk);
    CHECK(lKey.Error.Contains("has no parameter speed: it takes scale"));

    const CommandTextResult lTwice = lFix.Parse("time.scale 1 scale=2");
    CHECK_FALSE(lTwice.bOk);

    const CommandTextResult lExtra = lFix.Parse("time.scale 1 2");
    CHECK_FALSE(lExtra.bOk);
    CHECK(lExtra.Error.Contains("2 is one too many"));
}

TEST_CASE("CommandText: the usage shows each parameter, its type and whether it may be left out")
{
    const Fixture lFix;
    CHECK(FormatCommandUsage(lFix.Commands.Find("input.key")->Spec) == "input.key <key:string> [action:string] [frames:int]");
    CHECK(FormatCommandUsage(lFix.Commands.Find("bhop.teleport")->Spec) == "bhop.teleport <x> [y:float]");
    CHECK(FormatCommandUsage(lFix.Commands.Find("game.free")->Spec) == "game.free");
}

// =============================================================================
// console.run
// =============================================================================
TEST_CASE("CommandText: console.run runs a line as the console reads it, and a cheat it runs counts")
{
    CommandRegistry lCommands(/*bInDevBuild*/ true);
    nlohmann::json  lGot;
    lCommands.Add(CommandSpec("game.speed", "A cheat.").Param("scale", ECommandParamType::Float, "the scale").Cheat(),
                  [&lGot](const nlohmann::json& InParams)
                  {
                      lGot = InParams;
                      CommandResult lResult = CommandResult::Ok(nlohmann::json{ { "done", true } });
                      lResult.WaitFrames    = 2;
                      return lResult;
                  });

    const CommandHandler& lRun = lCommands.Find("console.run")->Handler;

    const CommandResult lOk = lRun(nlohmann::json{ { "line", "game.speed 0.5" } });
    REQUIRE(lOk.bOk);
    CHECK(lGot == nlohmann::json{ { "scale", 0.5 } });
    CHECK(lOk.Value == nlohmann::json{ { "done", true } });
    CHECK(lOk.WaitFrames == 2);   // the command's own wait
    CHECK(lCommands.HaveCheatsRun());

    lCommands.ClearCheatsRun();
    const CommandResult lBad = lRun(nlohmann::json{ { "line", "game.speed fast" } });
    CHECK_FALSE(lBad.bOk);
    CHECK(lBad.Error.Contains("scale is a number"));
    CHECK_FALSE(lCommands.HaveCheatsRun());
}

TEST_CASE("CommandText: console.run is dev only")
{
    CHECK(CommandRegistry(/*bInDevBuild*/ true).Has("console.run"));
    CHECK_FALSE(CommandRegistry(/*bInDevBuild*/ false).Has("console.run"));
}
