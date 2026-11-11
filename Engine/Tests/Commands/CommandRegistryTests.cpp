// Suite: CommandRegistry (commands by name, the newest registration of a name runs),
// CommandHandle (a registration removed with its handle) and cheats (the flag, CheatUsed, dev-only
// commands refused by a ship build).
#include <doctest.h>

#include "Automation/AutomationRunner.h"
#include "Commands/CommandRegistry.h"
#include "Core/Build/BuildInfo.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringJson.h"

using namespace Opaax;

namespace
{
    /** A handler that answers {"from": InTag}. */
    CommandHandler Answering(const char* InTag)
    {
        return [InTag](const nlohmann::json&) { return CommandResult::Ok(nlohmann::json{ { "from", InTag } }); };
    }

    /** What the command InName answers when run with no params, or "" when there is none. */
    OpaaxString RunFrom(const CommandRegistry& InRegistry, const char* InName)
    {
        const TSharedPtr<const RegisteredCommand> lCommand = InRegistry.Find(InName);
        if (lCommand == nullptr)
        {
            return OpaaxString();
        }
        return lCommand->Handler(nlohmann::json::object()).Value.at("from").get<OpaaxString>();
    }
}

TEST_CASE("CommandRegistry: commands.list and frames.wait are built in")
{
    CommandRegistry lRegistry;
    CHECK(lRegistry.Has("commands.list"));
    CHECK(lRegistry.Has("frames.wait"));

    const CommandResult lWait = lRegistry.Find("frames.wait")->Handler(nlohmann::json{ { "count", 4 } });
    CHECK(lWait.WaitFrames == 4);
}

TEST_CASE("CommandRegistry: commands.has answers whether a name is a command now")
{
    CommandRegistry lRegistry;
    const auto lHas = [&lRegistry](const char* InName)
    {
        return lRegistry.Find("commands.has")->Handler(nlohmann::json{ { "name", InName } });
    };

    CHECK(lHas("frames.wait").Value.at("has") == true);
    CHECK(lHas("game.ping").Value.at("has") == false);

    CommandHandle lHandle = lRegistry.Register(CommandSpec("game.ping", "Answers."), Answering("scoped"));
    CHECK(lHas("game.ping").Value.at("has") == true);

    CHECK_FALSE(lRegistry.Find("commands.has")->Handler(nlohmann::json::object()).bOk);
}

TEST_CASE("CommandRegistry: Add keeps a command for the registry's lifetime")
{
    CommandRegistry lRegistry;
    lRegistry.Add("game.hello", "Says hello.", Answering("add"));

    REQUIRE(lRegistry.Has("game.hello"));
    CHECK(RunFrom(lRegistry, "game.hello") == "add");
    CHECK_FALSE(lRegistry.Has("game.other"));
    CHECK(lRegistry.Find("game.other") == nullptr);
}

TEST_CASE("CommandHandle: dropping the handle removes its command")
{
    CommandRegistry lRegistry;
    {
        CommandHandle lHandle = lRegistry.Register(CommandSpec("game.ping", "Answers."), Answering("scoped"));
        CHECK(lHandle.IsValid());
        CHECK(lRegistry.Has("game.ping"));
    }
    CHECK_FALSE(lRegistry.Has("game.ping"));

    CommandHandle lHandle = lRegistry.Register(CommandSpec("game.ping", "Answers."), Answering("scoped"));
    lHandle.Reset();
    CHECK_FALSE(lHandle.IsValid());
    CHECK_FALSE(lRegistry.Has("game.ping"));
}

TEST_CASE("CommandHandle: a moved handle owns the registration; the moved-from one removes nothing")
{
    CommandRegistry lRegistry;
    CommandHandle   lFirst = lRegistry.Register(CommandSpec("game.ping", "Answers."), Answering("moved"));
    CommandHandle   lSecond(Move(lFirst));

    lFirst.Reset();
    CHECK(lRegistry.Has("game.ping"));

    CommandHandle lThird;
    lThird = Move(lSecond);
    CHECK(lRegistry.Has("game.ping"));

    lThird.Reset();
    CHECK_FALSE(lRegistry.Has("game.ping"));
}

TEST_CASE("CommandRegistry: the newest registration of a name runs; an older handle removes only its own")
{
    CommandRegistry lRegistry;

    // A level change: the new world registers before the old world's handle dies.
    CommandHandle lOld = lRegistry.Register(CommandSpec("game.teleport", "Old world."), Answering("old"));
    CommandHandle lNew = lRegistry.Register(CommandSpec("game.teleport", "New world."), Answering("new"));
    CHECK(RunFrom(lRegistry, "game.teleport") == "new");

    lOld.Reset();
    REQUIRE(lRegistry.Has("game.teleport"));
    CHECK(RunFrom(lRegistry, "game.teleport") == "new");

    lNew.Reset();
    CHECK_FALSE(lRegistry.Has("game.teleport"));
}

TEST_CASE("CommandRegistry: dropping the newest gives back the one under it")
{
    CommandRegistry lRegistry;
    lRegistry.Add("level.play", "The engine's.", Answering("engine"));

    CommandHandle lRefined = lRegistry.Register(CommandSpec("level.play", "A refinement."), Answering("refined"));
    CHECK(RunFrom(lRegistry, "level.play") == "refined");

    lRefined.Reset();
    CHECK(RunFrom(lRegistry, "level.play") == "engine");
}

TEST_CASE("CommandHandle: a handle that outlives its registry is safe")
{
    CommandHandle lHandle;
    {
        CommandRegistry lRegistry;
        lHandle = lRegistry.Register(CommandSpec("game.ping", "Answers."), Answering("gone"));
    }
    CHECK(lHandle.IsValid());
    lHandle.Reset();
    CHECK_FALSE(lHandle.IsValid());
}

TEST_CASE("CommandRegistry: a command found stays alive while held, even once removed")
{
    CommandRegistry lRegistry;
    CommandHandle   lHandle = lRegistry.Register(CommandSpec("game.ping", "Answers."), Answering("held"));

    const TSharedPtr<const RegisteredCommand> lHeld = lRegistry.Find("game.ping");
    lHandle.Reset();

    CHECK_FALSE(lRegistry.Has("game.ping"));
    REQUIRE(lHeld != nullptr);
    CHECK(lHeld->Handler(nlohmann::json::object()).Value.at("from") == "held");
}

TEST_CASE("CommandRegistry: Describe lists commands by name with their parameters")
{
    CommandRegistry lRegistry;
    lRegistry.Add("b.plain", "No parameters.", Answering("b"));
    lRegistry.Add(CommandSpec("a.typed", "Two parameters.")
                      .Param("x", ECommandParamType::Any, "where").Choices({ "start", "finish" })
                      .Param("y", ECommandParamType::Float, "the y position").Optional(),
                  Answering("a"));

    const nlohmann::json lList = lRegistry.Describe();
    REQUIRE(lList.is_array());
    REQUIRE(lList.size() == 6);   // with the built-in commands

    // Sorted: a.typed, b.plain, cheats.info, commands.has, commands.list, frames.wait.
    CHECK(lList[0]["name"] == "a.typed");
    CHECK(lList[1]["name"] == "b.plain");
    CHECK(lList[2]["name"] == "cheats.info");
    CHECK(lList[3]["name"] == "commands.has");
    CHECK(lList[4]["name"] == "commands.list");
    CHECK(lList[5]["name"] == "frames.wait");

    CHECK(lList[1]["help"] == "No parameters.");
    CHECK_FALSE(lList[1].contains("params"));

    const nlohmann::json& lParams = lList[0]["params"];
    REQUIRE(lParams.size() == 2);
    CHECK(lParams[0] == nlohmann::json{ { "name", "x" }, { "type", "any" }, { "help", "where" },
                                        { "choices", { "start", "finish" } } });
    CHECK(lParams[1] == nlohmann::json{ { "name", "y" }, { "type", "float" }, { "help", "the y position" },
                                        { "optional", true } });
}

TEST_CASE("CommandRegistry: Describe shows the registration that runs")
{
    CommandRegistry lRegistry;
    lRegistry.Add("game.hello", "Old help.", Answering("old"));
    CommandHandle lHandle = lRegistry.Register(CommandSpec("game.hello", "New help."), Answering("new"));

    nlohmann::json lHello;
    for (const nlohmann::json& lItem : lRegistry.Describe())
    {
        if (lItem["name"] == "game.hello")
        {
            CHECK(lHello.is_null());   // listed once
            lHello = lItem;
        }
    }
    CHECK(lHello["help"] == "New help.");
}

TEST_CASE("ICommandRegistry: the app's registry is used through its interface")
{
    CommandRegistry   lConcrete;
    ICommandRegistry& lRegistry = lConcrete;

    lRegistry.Add("game.hello", "Says hello.", Answering("service"));
    CHECK(lRegistry.Has("game.hello"));

    {
        CommandHandle lHandle = lRegistry.Register(CommandSpec("game.ping", "Answers."), Answering("scoped"));
        CHECK(lRegistry.Has("game.ping"));
    }
    CHECK_FALSE(lRegistry.Has("game.ping"));
    CHECK_FALSE(lRegistry.IsNull());
}

TEST_CASE("ICommandRegistry: the null registry keeps nothing")
{
    ICommandRegistry& lNull = ICommandRegistry::Null();
    CHECK(lNull.IsNull());

    lNull.Add("game.hello", "Says hello.", Answering("null"));
    CommandHandle lHandle = lNull.Register(CommandSpec("game.ping", "Answers."), Answering("null"));

    CHECK_FALSE(lHandle.IsValid());
    CHECK_FALSE(lNull.Has("game.hello"));
    CHECK_FALSE(lNull.Has("commands.list"));
    CHECK(lNull.Describe().empty());
}

// =============================================================================
// Cheats
// =============================================================================
namespace
{
    /** A registry with a runner over it, and the CheatUsed it sent. */
    struct CheatFixture
    {
        CommandRegistry                 Commands;
        AutomationRunner                Runner{ Commands };
        TDynArray<OpaaxString>          Cheats;

        CheatFixture()
        {
            Commands.SetCheatListener([this](const CheatUsed& InCheat) { Cheats.push_back(InCheat.Command.ToString()); });
            Commands.Add(CommandSpec("game.god", "A cheat.").Cheat(), Answering("god"));
            Commands.Add(CommandSpec("game.bad", "A cheat that refuses.").Cheat(),
                         [](const nlohmann::json&) { return CommandResult::Fail("refused"); });
            Commands.Add("game.info", "A plain command.", Answering("info"));
        }

        void Run(const char* InCommand)
        {
            Runner.Enqueue(AutomationRequest{ InCommand, InCommand });
            Runner.Tick();
        }
    };
}

TEST_CASE("Cheats: a cheat that answers ok sets the flag and is told at once")
{
    CheatFixture lFix;
    CHECK_FALSE(lFix.Commands.HaveCheatsRun());

    lFix.Run("game.god");
    CHECK(lFix.Commands.HaveCheatsRun());
    REQUIRE(lFix.Cheats.size() == 1);
    CHECK(lFix.Cheats[0] == "game.god");
}

TEST_CASE("Cheats: a plain command and a refused cheat do neither")
{
    CheatFixture lFix;

    lFix.Run("game.info");
    lFix.Run("game.bad");
    lFix.Run("game.unknown");

    CHECK_FALSE(lFix.Commands.HaveCheatsRun());
    CHECK(lFix.Cheats.empty());
}

TEST_CASE("Cheats: ClearCheatsRun starts a clean run; cheats.info answers the flag")
{
    CheatFixture lFix;
    const auto lInfo = [&lFix]() { return lFix.Commands.Find("cheats.info")->Handler(nlohmann::json::object()).Value; };

    CHECK(lInfo().at("cheatsRun") == false);
    lFix.Run("game.god");
    CHECK(lInfo().at("cheatsRun") == true);

    lFix.Commands.ClearCheatsRun();
    CHECK_FALSE(lFix.Commands.HaveCheatsRun());
    CHECK(lInfo().at("cheatsRun") == false);
}

TEST_CASE("Cheats: Describe marks cheat and dev-only commands")
{
    CommandRegistry lRegistry(/*bInDevBuild*/ true);
    lRegistry.Add(CommandSpec("game.god", "A cheat.").Cheat().DevOnly(), Answering("god"));

    for (const nlohmann::json& lItem : lRegistry.Describe())
    {
        if (lItem["name"] == "game.god")
        {
            CHECK(lItem["cheat"] == true);
            CHECK(lItem["devOnly"] == true);
        }
        else
        {
            CHECK_FALSE(lItem.contains("cheat"));
            CHECK_FALSE(lItem.contains("devOnly"));
        }
    }
}

TEST_CASE("Cheats: a ship build's registry refuses dev-only commands, and keeps the others")
{
    CommandRegistry lShip(/*bInDevBuild*/ false);

    lShip.Add(CommandSpec("time.scale", "Dev only.").Cheat().DevOnly(), Answering("scale"));
    CommandHandle lHandle = lShip.Register(CommandSpec("game.teleport", "Dev only.").Cheat().DevOnly(), Answering("tp"));
    lShip.Add(CommandSpec("world.step", "A cheat a ship build keeps.").Cheat(), Answering("step"));

    CHECK_FALSE(lShip.Has("time.scale"));
    CHECK_FALSE(lShip.Has("game.teleport"));
    CHECK_FALSE(lHandle.IsValid());
    CHECK(lShip.Has("world.step"));

    CommandRegistry lDev(/*bInDevBuild*/ true);
    lDev.Add(CommandSpec("time.scale", "Dev only.").Cheat().DevOnly(), Answering("scale"));
    CHECK(lDev.Has("time.scale"));
}

TEST_CASE("Cheats: the registry is built for this build by default")
{
    CommandRegistry lRegistry;
    lRegistry.Add(CommandSpec("game.dev", "Dev only.").DevOnly(), Answering("dev"));
    CHECK(lRegistry.Has("game.dev") == Build::IsDevBuild());
}
