// Suite: automation — the request runner (order, frame waits, failures, the built-in commands),
// script and request parsing, and the two transports (an inbox folder, a script file). No window:
// the runner is ticked by hand, a tick standing for a frame.
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "Automation/AutomationRunner.h"
#include "Automation/AutomationSession.h"
#include "Automation/AutomationTransports.h"
#include "Automation/EngineAutomationCommands.h"
#include "UI/UICanvas.h"
#include "UI/UIRect.h"
#include "UI/Widgets/UIButton.h"
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

TEST_CASE("Automation: a long job is answered when its wait is over, with its outcome")
{
    Recorder lRecorder;
    bool     bDone = false;

    lRecorder.Runner.Register("job", "Ends when bDone is set.", [&bDone](const nlohmann::json&)
    {
        AutomationResult lResult;
        lResult.WaitUntil = [&bDone] { return bDone; };
        lResult.Answer    = [&bDone] { return AutomationResult::Ok(nlohmann::json{ { "done", bDone } }); };
        return lResult;
    });
    lRecorder.Runner.Register("broken-job", "Ends badly a frame later.", [](const nlohmann::json&)
    {
        AutomationResult lResult;
        lResult.WaitFrames = 1;
        lResult.Answer     = [] { return AutomationResult::Fail("the job failed"); };
        return lResult;
    });

    lRecorder.Runner.Enqueue(Request("1", "job"));
    lRecorder.Runner.Enqueue(Request("2", "echo"));

    lRecorder.Runner.Tick();
    lRecorder.Runner.Tick();
    CHECK(lRecorder.Responses.empty());   // not answered yet, and the next request waits

    bDone = true;
    lRecorder.Runner.Tick();
    REQUIRE(lRecorder.Responses.size() == 2);
    CHECK(lRecorder.Responses[0].Id == "1");
    CHECK(lRecorder.Responses[0].Result.at("done") == true);
    CHECK(lRecorder.Responses[1].Id == "2");

    lRecorder.Runner.Enqueue(Request("3", "broken-job"));
    lRecorder.Runner.Tick();
    CHECK(lRecorder.Responses.size() == 2);
    lRecorder.Runner.Tick();
    REQUIRE(lRecorder.Responses.size() == 3);
    CHECK_FALSE(lRecorder.Responses[2].bOk);
    CHECK(lRecorder.Responses[2].Error == "the job failed");
    CHECK(lRecorder.Runner.GetFailed() == 1);
}

TEST_CASE("Automation: closing answers what is left: the waiting request, then every request not run")
{
    Recorder lRecorder;
    lRecorder.Runner.Register("job", "Never ends on its own.", [](const nlohmann::json&)
    {
        AutomationResult lResult;
        lResult.WaitUntil = [] { return false; };
        lResult.Answer    = [] { return AutomationResult::Ok(); };
        return lResult;
    });

    lRecorder.Runner.Enqueue(Request("1", "job"));
    lRecorder.Runner.Enqueue(Request("2", "echo"));
    lRecorder.Runner.Enqueue(Request("3", "echo"));
    lRecorder.Runner.Tick();
    CHECK(lRecorder.Responses.empty());

    lRecorder.Runner.Close();

    REQUIRE(lRecorder.Responses.size() == 3);
    CHECK_FALSE(lRecorder.Responses[0].bOk);
    CHECK(lRecorder.Responses[0].Error == "the app closed before this request finished");
    CHECK_FALSE(lRecorder.Responses[1].bOk);
    CHECK(lRecorder.Responses[1].Error == "the app closed before this request ran");
    CHECK(lRecorder.Responses[2].Id == "3");
    CHECK(lRecorder.Runner.GetFailed() == 3);
    CHECK(lRecorder.Runner.IsIdle());
}

TEST_CASE("Automation: a request waiting for the app to close is answered by its OnClose")
{
    Recorder lRecorder;
    lRecorder.Runner.Register("until-closed", "Waits for the app to close.", [](const nlohmann::json&)
    {
        AutomationResult lResult;
        lResult.WaitUntil = [] { return false; };
        lResult.Answer    = [] { return AutomationResult::Fail("still open"); };
        lResult.OnClose   = [] { return AutomationResult::Ok(nlohmann::json{ { "closed", true } }); };
        return lResult;
    });

    lRecorder.Runner.Enqueue(Request("1", "until-closed"));
    lRecorder.Runner.Tick();
    lRecorder.Runner.Close();

    REQUIRE(lRecorder.Responses.size() == 1);
    CHECK(lRecorder.Responses[0].bOk);
    CHECK(lRecorder.Responses[0].Result.at("closed") == true);
    CHECK(lRecorder.Runner.GetFailed() == 0);
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

TEST_CASE("Automation UI: widgets are listed in screen pixels, and a click lands on a widget's centre")
{
    // A 1920x1080 target showing a 1080-unit canvas: a canvas unit is a pixel.
    UICanvas lCanvas(1080.f);
    lCanvas.SetTargetSize(1920, 1080);

    UIWidget* const lPlay = lCanvas.Root().AddChild(MakeUnique<UIButton>());
    lPlay->Name = OpaaxString("Play");
    UIRect lRect;
    lRect.AnchoredPosition = { 100.f, 200.f };   // right of and above the centre
    lRect.SizeDelta        = { 300.f, 80.f };
    lPlay->SetRect(lRect);

    UIWidget* const lSecret = lCanvas.Root().AddChild(MakeUnique<UIButton>());
    lSecret->Name     = OpaaxString("Secret");
    lSecret->bVisible = false;

    lCanvas.Update();

    const nlohmann::json lListed = EngineAutomation::DescribeWidgets(lCanvas, "Play");
    REQUIRE(lListed.size() == 1);
    CHECK(lListed[0].at("type") == "UIButton");
    CHECK(lListed[0].at("visible") == true);
    CHECK(lListed[0].at("x").get<float>() == doctest::Approx(1060.f));   // 960 + 100
    CHECK(lListed[0].at("y").get<float>() == doctest::Approx(340.f));    // 540 - 200: pixels go down
    CHECK(lListed[0].at("width").get<float>() == doctest::Approx(300.f));
    CHECK(lListed[0].at("height").get<float>() == doctest::Approx(80.f));

    // Unfiltered: the root, Play and Secret.
    CHECK(EngineAutomation::DescribeWidgets(lCanvas, "").size() == 3);
    CHECK(EngineAutomation::DescribeWidgets(lCanvas, "Secret")[0].at("visible") == false);

    Vector2F    lPixel{ 0.f, 0.f };
    std::string lError;
    REQUIRE(EngineAutomation::FindWidgetCentre(lCanvas, "Play", lPixel, lError));
    CHECK(lPixel.x == doctest::Approx(1060.f));
    CHECK(lPixel.y == doctest::Approx(340.f));
    CHECK(lCanvas.HitTest(lCanvas.ScreenToCanvas(lPixel)) == lPlay);

    CHECK_FALSE(EngineAutomation::FindWidgetCentre(lCanvas, "Secret", lPixel, lError));
    CHECK(lError == "'Secret' is hidden");
    CHECK_FALSE(EngineAutomation::FindWidgetCentre(lCanvas, "Missing", lPixel, lError));
    CHECK(lError.find("no widget named 'Missing'") != std::string::npos);
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

TEST_CASE("Automation session: a script the app closed on fails the run; a finished one passes")
{
    ScopedTempDir lDir("Session");

    const fs::path lScriptPath = lDir.Path() / "steps.json";
    const fs::path lOutput     = lDir.Path() / "steps.out.json";
    WriteFile(lScriptPath, R"([{"command": "frames.wait", "params": {"count": 2}}, {"command": "commands.list"}])");

    SUBCASE("closed before the end")
    {
        TUniquePtr<AutomationSession> lSession =
            AutomationSession::Create(lScriptPath.string().c_str(), lOutput.string().c_str(), nullptr);
        REQUIRE(lSession != nullptr);

        lSession->BeginFrame();   // the wait holds commands.list
        lSession->Close();

        CHECK(lSession->GetExitCode() == 1);
        const nlohmann::json lAnswers = ReadJson(lOutput);
        CHECK(lAnswers.at("done") == true);
        CHECK(lAnswers.at("failed") == 1);
    }

    SUBCASE("run to the end")
    {
        TUniquePtr<AutomationSession> lSession =
            AutomationSession::Create(lScriptPath.string().c_str(), lOutput.string().c_str(), nullptr);
        REQUIRE(lSession != nullptr);

        for (int lFrame = 0; lFrame < 3; ++lFrame)
        {
            lSession->BeginFrame();
        }
        lSession->Close();

        CHECK(lSession->GetExitCode() == 0);
        CHECK(ReadJson(lOutput).at("failed") == 0);
    }
}

TEST_CASE("Automation session: a script owns the input; an inbox shares it with the window")
{
    ScopedTempDir lDir("Owner");

    const fs::path lScriptPath = lDir.Path() / "steps.json";
    WriteFile(lScriptPath, R"([{"command": "frames.wait"}])");

    TUniquePtr<AutomationSession> lScripted =
        AutomationSession::Create(lScriptPath.string().c_str(), nullptr, nullptr);
    REQUIRE(lScripted != nullptr);
    CHECK(lScripted->OwnsInput());

    TUniquePtr<AutomationSession> lInbox = AutomationSession::Create(nullptr, nullptr, lDir.Path().string().c_str());
    REQUIRE(lInbox != nullptr);
    CHECK_FALSE(lInbox->OwnsInput());
}
