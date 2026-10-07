// Suite: automation — the request runner (order, frame waits, failures, the built-in commands),
// script and request parsing, and the two transports (an inbox folder, a script file). No window:
// the runner is ticked by hand, a tick standing for a frame.
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "Automation/AutomationRunner.h"
#include "Automation/AutomationTransports.h"
#include "Automation/EngineAutomationCommands.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxAutomationTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);   // a previous crashed run must not poison this one
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        const fs::path& Path() const noexcept { return m_Path; }

    private:
        fs::path m_Path;
    };

    void WriteFile(const fs::path& InPath, const std::string& InText)
    {
        std::ofstream lFile(InPath, std::ios::binary | std::ios::trunc);
        lFile << InText;
    }

    nlohmann::json ReadJson(const fs::path& InPath)
    {
        std::ifstream      lFile(InPath, std::ios::binary);
        std::ostringstream lStream;
        lStream << lFile.rdbuf();
        return nlohmann::json::parse(lStream.str(), nullptr, false);
    }

    AutomationRequest Request(const char* InId, const char* InCommand, nlohmann::json InParams = nlohmann::json::object())
    {
        return AutomationRequest{ InId, InCommand, Move(InParams) };
    }

    /** A runner whose answers are kept in order. */
    struct Recorder
    {
        AutomationRunner              Runner;
        TDynArray<AutomationResponse> Responses;

        Recorder()
        {
            Runner.SetResponseSink([this](const AutomationResponse& InResponse) { Responses.push_back(InResponse); });
            Runner.Register("echo", "Returns its params.",
                            [](const nlohmann::json& InParams) { return AutomationResult::Ok(InParams); });
        }
    };
}

TEST_CASE("Automation: requests run in order at the start of a frame, each answered")
{
    Recorder lRecorder;
    lRecorder.Runner.Enqueue(Request("a", "echo", { { "n", 1 } }));
    lRecorder.Runner.Enqueue(Request("b", "echo", { { "n", 2 } }));
    CHECK_FALSE(lRecorder.Runner.IsIdle());

    lRecorder.Runner.Tick();

    REQUIRE(lRecorder.Responses.size() == 2);
    CHECK(lRecorder.Responses[0].Id == "a");
    CHECK(lRecorder.Responses[0].bOk);
    CHECK(lRecorder.Responses[0].Result.at("n") == 1);
    CHECK(lRecorder.Responses[1].Id == "b");
    CHECK(lRecorder.Runner.IsIdle());
    CHECK(lRecorder.Runner.GetAnswered() == 2);
}

TEST_CASE("Automation: a wait lets exactly its frames run before the next request")
{
    Recorder lRecorder;
    lRecorder.Runner.Enqueue(Request("wait", "frames.wait", { { "count", 3 } }));
    lRecorder.Runner.Enqueue(Request("after", "echo"));

    lRecorder.Runner.Tick();   // frame 1 runs after this
    CHECK(lRecorder.Responses.size() == 1);
    lRecorder.Runner.Tick();   // frame 2
    lRecorder.Runner.Tick();   // frame 3
    CHECK(lRecorder.Responses.size() == 1);
    CHECK_FALSE(lRecorder.Runner.IsIdle());

    lRecorder.Runner.Tick();
    REQUIRE(lRecorder.Responses.size() == 2);
    CHECK(lRecorder.Responses[1].Id == "after");
}

TEST_CASE("Automation: what comes after a wait runs before the next request")
{
    Recorder lRecorder;
    bool     bReleased = false;

    lRecorder.Runner.Register("hold", "Holds a frame, then releases.", [&bReleased](const nlohmann::json&)
    {
        AutomationResult lResult;
        lResult.WaitFrames = 1;
        lResult.AfterWait  = [&bReleased] { bReleased = true; };
        return lResult;
    });
    lRecorder.Runner.Register("check", "Reports the release.", [&bReleased](const nlohmann::json&)
    {
        return AutomationResult::Ok(nlohmann::json{ { "released", bReleased } });
    });

    lRecorder.Runner.Enqueue(Request("1", "hold"));
    lRecorder.Runner.Enqueue(Request("2", "check"));

    lRecorder.Runner.Tick();
    CHECK_FALSE(bReleased);   // the held frame has not run yet

    lRecorder.Runner.Tick();
    REQUIRE(lRecorder.Responses.size() == 2);
    CHECK(lRecorder.Responses[1].Result.at("released") == true);
}

TEST_CASE("Automation: a condition holds the queue, after the frames, until it is true")
{
    Recorder lRecorder;
    Uint32   lClock    = 0;
    bool     bReleased = false;

    lRecorder.Runner.Register("until-3", "Holds until the clock reaches 3.", [&](const nlohmann::json&)
    {
        AutomationResult lResult;
        lResult.WaitFrames = 1;
        lResult.WaitUntil  = [&lClock] { return lClock >= 3; };
        lResult.AfterWait  = [&bReleased] { bReleased = true; };
        return lResult;
    });

    lRecorder.Runner.Enqueue(Request("1", "until-3"));
    lRecorder.Runner.Enqueue(Request("2", "echo"));

    lRecorder.Runner.Tick();   // runs until-3; the clock is 0
    for (lClock = 1; lClock < 3; ++lClock)
    {
        lRecorder.Runner.Tick();
        CHECK(lRecorder.Responses.size() == 1);
        CHECK_FALSE(bReleased);
    }

    lRecorder.Runner.Tick();   // the clock reached 3
    CHECK(bReleased);
    REQUIRE(lRecorder.Responses.size() == 2);
    CHECK(lRecorder.Runner.IsIdle());
}

TEST_CASE("Automation: an unknown command and bad params fail, and the queue goes on")
{
    Recorder lRecorder;
    lRecorder.Runner.Register("needs-x", "Reads an int x.", [](const nlohmann::json& InParams)
    {
        return AutomationResult::Ok(nlohmann::json{ { "x", InParams.at("x").get<int>() } });
    });

    lRecorder.Runner.Enqueue(Request("1", "no.such.command"));
    lRecorder.Runner.Enqueue(Request("2", "needs-x", { { "x", "not a number" } }));
    lRecorder.Runner.Enqueue(Request("3", "needs-x", { { "x", 7 } }));
    lRecorder.Runner.Tick();

    REQUIRE(lRecorder.Responses.size() == 3);
    CHECK_FALSE(lRecorder.Responses[0].bOk);
    CHECK(lRecorder.Responses[0].Error.find("unknown command") != std::string::npos);
    CHECK_FALSE(lRecorder.Responses[1].bOk);
    CHECK(lRecorder.Responses[1].Error.find("bad params") != std::string::npos);
    CHECK(lRecorder.Responses[2].bOk);
    CHECK(lRecorder.Runner.GetFailed() == 2);
}

TEST_CASE("Automation: commands.list describes every command, sorted, with its help")
{
    Recorder lRecorder;
    lRecorder.Runner.Enqueue(Request("1", "commands.list"));
    lRecorder.Runner.Tick();

    REQUIRE(lRecorder.Responses.size() == 1);
    const nlohmann::json& lList = lRecorder.Responses[0].Result.at("commands");
    REQUIRE(lList.size() == 3);
    CHECK(lList[0].at("name") == "commands.list");
    CHECK(lList[1].at("name") == "echo");
    CHECK(lList[2].at("name") == "frames.wait");
    CHECK(lList[1].at("help") == "Returns its params.");
}

TEST_CASE("Automation: a script is an array or {requests}; ids default to positions; errors name the request")
{
    TDynArray<AutomationRequest> lRequests;
    std::string                  lError;

    REQUIRE(ParseAutomationScript(R"([{"command": "a"}, {"id": "named", "command": "b", "params": {"k": 1}}])",
                                  lRequests, lError));
    REQUIRE(lRequests.size() == 2);
    CHECK(lRequests[0].Id == "0");
    CHECK(lRequests[1].Id == "named");
    CHECK(lRequests[1].Params.at("k") == 1);

    REQUIRE(ParseAutomationScript(R"({"requests": [{"command": "a"}]})", lRequests, lError));
    CHECK(lRequests.size() == 1);

    CHECK_FALSE(ParseAutomationScript(R"([{"command": "a"}, {"params": {}}])", lRequests, lError));
    CHECK(lError.find("request 1") != std::string::npos);
    CHECK(lRequests.empty());

    CHECK_FALSE(ParseAutomationScript(R"([{"command": "a", "params": [1, 2]}])", lRequests, lError));
    CHECK_FALSE(ParseAutomationScript("not json", lRequests, lError));
    CHECK_FALSE(ParseAutomationScript(R"({"other": []})", lRequests, lError));
}

TEST_CASE("Automation inbox: request files run in name order and are answered beside them")
{
    ScopedTempDir lDir("Inbox");
    Recorder      lRecorder;

    AutomationInbox lInbox(lDir.Path() / "inbox");
    REQUIRE(lInbox.Open());
    lRecorder.Runner.SetResponseSink([&](const AutomationResponse& InResponse)
    {
        lRecorder.Responses.push_back(InResponse);
        lInbox.Answer(InResponse);
    });

    WriteFile(lInbox.GetFolder() / "002.request.json", R"({"command": "echo", "params": {"n": 2}})");
    WriteFile(lInbox.GetFolder() / "001.request.json", R"({"command": "echo", "params": {"n": 1}})");
    WriteFile(lInbox.GetFolder() / "bad.request.json", "{ not json");
    WriteFile(lInbox.GetFolder() / "notes.txt", "ignored");

    CHECK(lInbox.Poll(lRecorder.Runner) == 2);
    lRecorder.Runner.Tick();

    // The file name is the id, and name order is run order.
    REQUIRE(lRecorder.Responses.size() == 2);
    CHECK(lRecorder.Responses[0].Id == "001");
    CHECK(lRecorder.Responses[1].Id == "002");

    const nlohmann::json lFirst = ReadJson(lInbox.GetFolder() / "001.response.json");
    CHECK(lFirst.at("ok") == true);
    CHECK(lFirst.at("result").at("n") == 1);

    // A malformed request is answered at once; requests are consumed.
    const nlohmann::json lBad = ReadJson(lInbox.GetFolder() / "bad.response.json");
    CHECK(lBad.at("ok") == false);
    CHECK_FALSE(fs::exists(lInbox.GetFolder() / "001.request.json"));
    CHECK(fs::exists(lInbox.GetFolder() / "notes.txt"));

    CHECK(lInbox.Poll(lRecorder.Runner) == 0);
}

// =============================================================================
// The engine commands' entity helpers, on a bare world.
// =============================================================================

TEST_CASE("Automation entities: found by id or by a unique name, with a reason when not")
{
    World        lWorld("Automation");
    const Entity lHero  = lWorld.CreateEntity("Hero");
    const Entity lCrate = lWorld.CreateEntity("Crate");
    lWorld.CreateEntity("Crate");

    std::string lError;
    CHECK(EngineAutomation::FindEntity(lWorld, nlohmann::json{ { "entity", "Hero" } }, lError).GetHandle()
          == lHero.GetHandle());

    const std::string lCrateId = lCrate.GetGuid().ToString().CStr();
    CHECK(EngineAutomation::FindEntity(lWorld, nlohmann::json{ { "entity", lCrateId } }, lError).GetHandle()
          == lCrate.GetHandle());

    CHECK_FALSE(EngineAutomation::FindEntity(lWorld, nlohmann::json{ { "entity", "Crate" } }, lError).IsValid());
    CHECK(lError.find("2 entities") != std::string::npos);

    CHECK_FALSE(EngineAutomation::FindEntity(lWorld, nlohmann::json{ { "entity", "Ghost" } }, lError).IsValid());
    CHECK(lError.find("no entity named") != std::string::npos);

    CHECK_FALSE(EngineAutomation::FindEntity(lWorld, nlohmann::json::object(), lError).IsValid());
}

TEST_CASE("Automation entities: described with their components, and patched field by field")
{
    ComponentRegistry lTypes;
    REQUIRE(lTypes.Register<TransformComponent>("Transform", /*bInEssential*/ true));

    World  lWorld("Automation");
    Entity lHero = lWorld.CreateEntity("Hero");

    const nlohmann::json lBrief = EngineAutomation::DescribeEntity(lHero, lTypes, false);
    CHECK(lBrief.at("name") == "Hero");
    CHECK(lBrief.at("id") == lHero.GetGuid().ToString().CStr());
    CHECK(lBrief.at("parent").is_null());
    CHECK(lBrief.at("components") == nlohmann::json::array({ "Transform" }));

    // Only the fields given change.
    lHero.Get<TransformComponent>().Rotation = 45.f;
    std::string lError;
    REQUIRE(EngineAutomation::PatchComponent(
        lHero, lTypes, nlohmann::json{ { "type", "Transform" }, { "value", { { "Position", { { "x", 12.0 } } } } } },
        lError));

    const TransformComponent& lTransform = lHero.Get<TransformComponent>();
    CHECK(lTransform.Position.x == doctest::Approx(12.f));
    CHECK(lTransform.Rotation == doctest::Approx(45.f));

    const nlohmann::json lFull = EngineAutomation::DescribeEntity(lHero, lTypes, true);
    CHECK(lFull.at("components").at("Transform").at("Position").at("x") == 12.0);

    CHECK_FALSE(EngineAutomation::PatchComponent(lHero, lTypes, nlohmann::json{ { "type", "Nope" } }, lError));
    CHECK(lError.find("unknown component") != std::string::npos);
    CHECK_FALSE(EngineAutomation::PatchComponent(
        lHero, lTypes, nlohmann::json{ { "type", "Transform" }, { "value", 3 } }, lError));
}

TEST_CASE("Automation expectations: equals, near, greater, less and between, with what was found")
{
    std::string lError;

    // equals: numbers within 1e-4, objects on the fields given, arrays element by element.
    CHECK(EngineAutomation::CheckExpectation(1.00001, nlohmann::json{ { "equals", 1.0 } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation(1.01, nlohmann::json{ { "equals", 1.0 } }, lError));
    CHECK(lError == "expected 1.0, found 1.01");

    const nlohmann::json lPosition{ { "x", 3.0 }, { "y", 4.0 } };
    CHECK(EngineAutomation::CheckExpectation(lPosition, nlohmann::json{ { "equals", { { "x", 3.0 } } } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation(lPosition, nlohmann::json{ { "equals", { { "z", 0.0 } } } }, lError));
    CHECK(EngineAutomation::CheckExpectation("Spot", nlohmann::json{ { "equals", "Spot" } }, lError));
    CHECK(EngineAutomation::CheckExpectation(nlohmann::json::array({ 1, 2 }),
                                             nlohmann::json{ { "equals", nlohmann::json::array({ 1, 2 }) } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation(nlohmann::json::array({ 1, 2 }),
                                                   nlohmann::json{ { "equals", nlohmann::json::array({ 1 }) } }, lError));

    // near: a tolerance, 0.01 by default.
    CHECK(EngineAutomation::CheckExpectation(10.005, nlohmann::json{ { "near", 10.0 } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation(10.5, nlohmann::json{ { "near", 10.0 } }, lError));
    CHECK(EngineAutomation::CheckExpectation(10.5, nlohmann::json{ { "near", 10.0 }, { "tolerance", 1.0 } }, lError));

    // Comparisons, together or alone; they need a number.
    CHECK(EngineAutomation::CheckExpectation(5, nlohmann::json{ { "greater", 4 }, { "less", 6 } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation(5, nlohmann::json{ { "greater", 5 } }, lError));
    CHECK(lError == "expected more than 5, found 5");
    CHECK_FALSE(EngineAutomation::CheckExpectation(5, nlohmann::json{ { "less", 2 } }, lError));
    CHECK(EngineAutomation::CheckExpectation(5, nlohmann::json{ { "between", { 5, 9 } } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation(10, nlohmann::json{ { "between", { 5, 9 } } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation(5, nlohmann::json{ { "between", 3 } }, lError));
    CHECK_FALSE(EngineAutomation::CheckExpectation("text", nlohmann::json{ { "greater", 1 } }, lError));
    CHECK(lError == "expected a number, found \"text\"");

    // Nothing expected: anything passes.
    CHECK(EngineAutomation::CheckExpectation(nullptr, nlohmann::json::object(), lError));
}

TEST_CASE("Automation script: answers are written as they come, then marked done")
{
    ScopedTempDir lDir("Script");
    Recorder      lRecorder;

    const fs::path lScriptPath = lDir.Path() / "steps.json";
    WriteFile(lScriptPath, R"([{"command": "echo"}, {"command": "frames.wait"}, {"command": "nope"}])");

    AutomationScript lScript;
    lRecorder.Runner.SetResponseSink([&](const AutomationResponse& InResponse) { lScript.Answer(InResponse); });

    const fs::path lOutput = AutomationScript::DefaultOutput(lScriptPath);
    CHECK(lOutput.filename() == "steps.out.json");
    REQUIRE(lScript.Load(lScriptPath, lOutput, lRecorder.Runner));
    CHECK(ReadJson(lOutput).at("done") == false);

    lRecorder.Runner.Tick();   // echo, then the wait holds
    CHECK_FALSE(lScript.IsDone());
    CHECK(ReadJson(lOutput).at("responses").size() == 2);

    lRecorder.Runner.Tick();   // the failing request
    CHECK(lScript.IsDone());
    CHECK(lScript.GetFailed() == 1);

    const nlohmann::json lDone = ReadJson(lOutput);
    CHECK(lDone.at("done") == true);
    CHECK(lDone.at("failed") == 1);
    REQUIRE(lDone.at("responses").size() == 3);
    CHECK(lDone.at("responses")[2].at("ok") == false);
}
