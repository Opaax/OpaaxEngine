// Suite: OnlineAutomation (Automation/OnlineAutomationCommands.h) — the online.* answers, as pure
// functions of the services: a store request holds the queue until the store answers, then answers
// what it said.
#include <doctest.h>

#include "Application/Services/IOnlineServices.h"
#include "Automation/OnlineAutomationCommands.h"
#include "Online/Config/OnlineConfigData.h"
#include "Online/Fake/FakeOnlineBackend.h"
#include "Online/Null/NullOnlineBackend.h"

using namespace Opaax;

namespace
{
    OnlineConfigData ConfigWithBoard()
    {
        OnlineConfigData lConfig;
        OnlineLeaderboardSettings lBoard;
        lBoard.Name = "Level1";
        lConfig.Leaderboards.push_back(lBoard);
        return lConfig;
    }

    /** Pumps until InResult's wait is over (a few frames at most), then gives its answer. */
    AutomationResult Settle(OnlineServices& InOnline, AutomationResult InResult)
    {
        if (!InResult.WaitUntil)
        {
            return InResult;
        }
        for (Uint32 lFrame = 0; lFrame < 10 && !InResult.WaitUntil(); ++lFrame)
        {
            InOnline.Pump();
        }
        REQUIRE(InResult.WaitUntil());
        REQUIRE(InResult.Answer);
        return InResult.Answer();
    }
}

TEST_CASE("OnlineAutomation: info names the store and why, and use switches to another")
{
    const OnlineConfigData lConfig = ConfigWithBoard();
    OnlineServices lOnline(MakeUnique<NullOnlineBackend>(), "no store built in");
    lOnline.SetLeaderboards(ToLeaderboardDefinitions(lConfig));

    const AutomationResult lNone = OnlineAutomation::Info(lOnline);
    REQUIRE(lNone.bOk);
    CHECK(lNone.Value["backend"] == "None");
    CHECK(lNone.Value["reason"] == "no store built in");
    CHECK(lNone.Value["available"] == false);
    CHECK(lNone.Value["leaderboards"] == nlohmann::json::array({ "Level1" }));
    CHECK_FALSE(lNone.Value.contains("profile"));

    const AutomationResult lEpic = OnlineAutomation::Use(lOnline, lConfig, { { "backend", "fake" }, { "profile", "epic" } });
    REQUIRE(lEpic.bOk);
    CHECK(lEpic.Value["backend"] == "Fake");
    CHECK(lEpic.Value["profile"] == "epic");
    CHECK(lEpic.Value["capabilities"]["leaderboardFiles"] == false);

    // The game's boards exist in the fake portal: an Epic upload finds them.
    const AutomationResult lUpload = Settle(lOnline, OnlineAutomation::Upload(lOnline, { { "board", "Level1" }, { "score", 900 } }));
    REQUIRE(lUpload.bOk);
    CHECK(lUpload.Value["result"] == "Ok");
    CHECK(lUpload.Value["scoreChanged"] == true);

    CHECK_FALSE(OnlineAutomation::Use(lOnline, lConfig, { { "backend", "fake" }, { "profile", "xbox" } }).bOk);
    CHECK_FALSE(OnlineAutomation::Use(lOnline, lConfig, { { "backend", "steam" } }).bOk);

    const AutomationResult lOff = OnlineAutomation::Use(lOnline, lConfig, { { "backend", "none" } });
    CHECK(lOff.Value["backend"] == "None");
}

TEST_CASE("OnlineAutomation: upload, entries, file and achievements answer what the store said")
{
    const OnlineConfigData lConfig = ConfigWithBoard();
    OnlineServices lOnline(MakeUnique<NullOnlineBackend>(), "test");
    lOnline.SetLeaderboards(ToLeaderboardDefinitions(lConfig));
    OnlineAutomation::Use(lOnline, lConfig, { { "backend", "fake" } });
    static_cast<FakeOnlineBackend&>(lOnline.GetBackendObject()).SetLatency(2);

    REQUIRE(OnlineAutomation::AddFakePlayer(lOnline, { { "name", "Ada" }, { "friend", true }, { "board", "Level1" }, { "score", 800 } }).bOk);

    // The request holds the queue until the store answers.
    AutomationResult lPending = OnlineAutomation::Upload(lOnline, { { "board", "Level1" }, { "score", 1000 },
                                                                    { "details", { 1, 2 } }, { "file", "ghost" } });
    REQUIRE(lPending.WaitUntil);
    CHECK_FALSE(lPending.WaitUntil());
    const AutomationResult lUpload = Settle(lOnline, Move(lPending));
    CHECK(lUpload.Value["result"] == "Ok");
    CHECK(lUpload.Value["rank"] == 2);

    const AutomationResult lEntries = Settle(lOnline, OnlineAutomation::Entries(lOnline, { { "board", "Level1" } }));
    REQUIRE(lEntries.Value["entries"].size() == 2);
    CHECK(lEntries.Value["entries"][0]["name"] == "Ada");
    CHECK(lEntries.Value["entries"][1]["name"] == "Player");
    CHECK(lEntries.Value["entries"][1]["details"] == nlohmann::json::array({ 1, 2 }));
    const OpaaxString lFile = lEntries.Value["entries"][1]["file"].get<OpaaxString>();
    CHECK_FALSE(lFile.IsEmpty());

    const AutomationResult lGhost = Settle(lOnline, OnlineAutomation::GetFile(lOnline, { { "file", lFile } }));
    CHECK(lGhost.Value["result"] == "Ok");
    CHECK(lGhost.Value["text"] == "ghost");
    CHECK(lGhost.Value["size"] == 5);

    const AutomationResult lUnlocked = Settle(lOnline, OnlineAutomation::Achievements(lOnline, { { "unlock", "A" } }));
    CHECK(lUnlocked.Value["result"] == "Ok");
    CHECK(lUnlocked.Value["achievements"] == nlohmann::json::array({ { { "name", "A" }, { "unlocked", true } } }));

    const AutomationResult lNever = Settle(lOnline, OnlineAutomation::Achievements(lOnline, { { "clear", "Never" } }));
    CHECK(lNever.Value["result"] == "NotFound");

    // Bad requests fail at once, saying what to give.
    CHECK_FALSE(OnlineAutomation::Upload(lOnline, { { "score", 1 } }).bOk);
    CHECK_FALSE(OnlineAutomation::Upload(lOnline, { { "board", "Level1" }, { "score", "fast" } }).bOk);
    CHECK_FALSE(OnlineAutomation::Entries(lOnline, { { "board", "Level1" }, { "range", "Galaxy" } }).bOk);
    CHECK_FALSE(OnlineAutomation::GetFile(lOnline, nlohmann::json::object()).bOk);
    CHECK_FALSE(OnlineAutomation::AddFakePlayer(lOnline, { { "name", "Cy" }, { "board", "Nope" }, { "score", 1 } }).bOk);
}

TEST_CASE("OnlineAutomation: the overlay and fake players need the fake store")
{
    OnlineServices lNone(MakeUnique<NullOnlineBackend>(), "test");
    CHECK_FALSE(OnlineAutomation::Overlay(lNone, { { "active", true } }).bOk);
    CHECK_FALSE(OnlineAutomation::AddFakePlayer(lNone, { { "name", "Ada" } }).bOk);

    OnlineServices lFake(MakeUnique<FakeOnlineBackend>(), "test");
    bool bHeard = false;
    lFake.OnOverlayChanged().Add([&bHeard](const bool bInActive) { bHeard = bInActive; });
    const AutomationResult lOpen = OnlineAutomation::Overlay(lFake, { { "active", true } });
    REQUIRE(lOpen.bOk);
    CHECK(lOpen.Value["overlay"] == true);
    CHECK(bHeard);
    CHECK_FALSE(OnlineAutomation::Overlay(lFake, nlohmann::json::object()).bOk);

    // A store answer is still a success: Unavailable is what the store said.
    const AutomationResult lUnavailable = Settle(lNone, OnlineAutomation::Upload(lNone, { { "board", "Level1" }, { "score", 1 } }));
    REQUIRE(lUnavailable.bOk);
    CHECK(lUnavailable.Value["result"] == "Unavailable");
}
