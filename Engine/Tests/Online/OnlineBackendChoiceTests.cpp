// Suite: the choice of a store (Online/OnlineBackendChoice.h), OnlineAPI's fallback to none, and the
// online config's file.
#include <doctest.h>

#include "Core/Algo/Algo.h"
#include "Core/String/OpaaxStringJson.h"
#include "Online/Config/OnlineConfigData.h"
#include "Online/OnlineAPI.h"
#include "Online/OnlineBackendChoice.h"

using namespace Opaax;

namespace
{
    using EB = EOnlineBackend;

    OnlineLaunch Launch(const char* InCommandLine, const bool bInEditor, const bool bInBySteam, const bool bInSteamBuilt)
    {
        OnlineLaunch lLaunch;
        lLaunch.CommandLine      = InCommandLine;
        lLaunch.bEditor          = bInEditor;
        lLaunch.bLaunchedBySteam = bInBySteam;
        lLaunch.BuiltIn          = { EB::Fake };
        if (bInSteamBuilt)
        {
            lLaunch.BuiltIn.push_back(EB::Steam);
        }
        return lLaunch;
    }
}

TEST_CASE("ChooseOnlineBackends: the command line, then the editor, then the config")
{
    struct Row
    {
        const char*         CommandLine;
        bool                bEditor;
        bool                bBySteam;
        bool                bSteamBuilt;
        TDynArray<EB>       Configured;
        TDynArray<EB>       Expected;
        EFakeStoreProfile   Profile;
        const char*         ReasonHas;
    };

    const Row lRows[] = {
        // A game: the configured stores it has.
        { "",           false, false, true,  { EB::Steam },           { EB::Steam },           EFakeStoreProfile::Full,  "online config" },
        { "",           false, false, false, { EB::Steam },           {},                      EFakeStoreProfile::Full,  "Steam not built in" },
        { "",           false, false, true,  {},                      {},                      EFakeStoreProfile::Full,  "no store" },
        { "",           false, false, true,  { EB::None, EB::Steam }, { EB::Steam },           EFakeStoreProfile::Full,  "online config" },
        { "",           false, false, true,  { EB::Steam, EB::Steam },{ EB::Steam },           EFakeStoreProfile::Full,  "online config" },
        { "",           false, false, true,  { EB::Fake, EB::Steam }, { EB::Fake, EB::Steam }, EFakeStoreProfile::Full,  "online config" },

        // Started by Steam: Steam first, whatever the order.
        { "",           false, true,  true,  { EB::Fake, EB::Steam }, { EB::Steam, EB::Fake }, EFakeStoreProfile::Full,  "launched by Steam" },

        // The editor: none, unless the command line says.
        { "",           true,  false, true,  { EB::Steam },           {},                      EFakeStoreProfile::Full,  "editor" },
        { "fake",       true,  false, true,  { EB::Steam },           { EB::Fake },            EFakeStoreProfile::Full,  "full profile" },

        // The command line wins.
        { "none",       false, true,  true,  { EB::Steam },           {},                      EFakeStoreProfile::Full,  "--online none" },
        { "fake:epic",  false, false, true,  { EB::Steam },           { EB::Fake },            EFakeStoreProfile::Epic,  "epic profile" },
        { "FAKE:Gog",   false, false, false, {},                      { EB::Fake },            EFakeStoreProfile::Gog,   "gog profile" },
        { "fake:xbox",  false, false, true,  { EB::Steam },           {},                      EFakeStoreProfile::Full,  "no such fake profile" },
        { "steam",      true,  false, true,  {},                      { EB::Steam },           EFakeStoreProfile::Full,  "--online" },
        { "steam",      false, false, false, { EB::Steam },           {},                      EFakeStoreProfile::Full,  "not built in" },
        { "epic",       false, false, true,  { EB::Steam },           {},                      EFakeStoreProfile::Full,  "not none, fake" },
    };

    for (const Row& lRow : lRows)
    {
        CAPTURE(lRow.CommandLine);
        CAPTURE(lRow.bEditor);
        CAPTURE(lRow.bBySteam);
        CAPTURE(lRow.bSteamBuilt);

        const OnlineBackendChoice lChoice = ChooseOnlineBackends(lRow.Configured,
            Launch(lRow.CommandLine, lRow.bEditor, lRow.bBySteam, lRow.bSteamBuilt));

        CHECK(lChoice.Candidates == lRow.Expected);
        CHECK(lChoice.FakeProfile == lRow.Profile);
        CAPTURE(lChoice.Reason.CStr());
        CHECK(lChoice.Reason.Contains(lRow.ReasonHas));
    }
}

TEST_CASE("OnlineAPI: the first store that starts, or none with each store's reason")
{
    const OnlineConfigData lConfig;

    OnlineBackendChoice lFake;
    lFake.Candidates  = { EB::Fake };
    lFake.FakeProfile = EFakeStoreProfile::Epic;
    lFake.Reason      = "from --online";

    OpaaxString lReason;
    TUniquePtr<IOnlineBackend> lBackend = OnlineAPI::CreateFirstBackend(lFake, lConfig, lReason);
    REQUIRE(lBackend != nullptr);
    CHECK(lBackend->GetType() == EB::Fake);
    CHECK_FALSE(lBackend->GetCapabilities().bLeaderboardFiles);
    CHECK(lReason == "from --online");

    // A store that does not start (Steam in this build) falls through to the next, or to none.
    OnlineBackendChoice lSteamThenFake;
    lSteamThenFake.Candidates = { EB::Steam, EB::Fake };
    lSteamThenFake.Reason     = "from the online config";
    lBackend = OnlineAPI::CreateFirstBackend(lSteamThenFake, lConfig, lReason);
    CHECK(lBackend->GetType() == EB::Fake);
    CHECK(lReason.Contains("Steam"));

    OnlineBackendChoice lSteamOnly;
    lSteamOnly.Candidates = { EB::Steam };
    lBackend = OnlineAPI::CreateFirstBackend(lSteamOnly, lConfig, lReason);
    CHECK(lBackend->GetType() == EB::None);
    CHECK(lReason.Contains("Steam"));

    OnlineBackendChoice lNone;
    lNone.Reason = "the editor never uses a store";
    lBackend = OnlineAPI::CreateFirstBackend(lNone, lConfig, lReason);
    CHECK(lBackend->GetType() == EB::None);
    CHECK(lReason == "the editor never uses a store");

    // The fake store is in every build.
    CHECK(Algo::Contains(OnlineAPI::GetBuiltInBackends(), EB::Fake));
}

TEST_CASE("OnlineConfigData: Steam by default, leaderboards and the app id read back from the file")
{
    const OnlineConfigData lDefault;
    CHECK(lDefault.Backends == TDynArray<EB>{ EB::Steam });
    CHECK(lDefault.Leaderboards.empty());
    CHECK(lDefault.Steam.AppId == 0);

    // An older file without the online keys keeps the defaults.
    const OnlineConfigData lEmpty = nlohmann::json::object().get<OnlineConfigData>();
    CHECK(lEmpty.Backends == TDynArray<EB>{ EB::Steam });

    OnlineConfigData lData;
    lData.Backends    = { EB::Fake, EB::Steam };
    lData.Steam.AppId = 480;
    OnlineLeaderboardSettings lBoard;
    lBoard.Name = "Level1";
    lBoard.Sort = ELeaderboardSort::Descending;
    lData.Leaderboards.push_back(lBoard);

    const OnlineConfigData lRead = nlohmann::json(lData).get<OnlineConfigData>();
    CHECK(lRead.Backends == lData.Backends);
    CHECK(lRead.Steam.AppId == 480);
    REQUIRE(lRead.Leaderboards.size() == 1);
    CHECK(lRead.Leaderboards[0].Name == "Level1");
    CHECK(lRead.Leaderboards[0].Sort == ELeaderboardSort::Descending);

    const TDynArray<LeaderboardDefinition> lDefinitions = ToLeaderboardDefinitions(lRead);
    REQUIRE(lDefinitions.size() == 1);
    CHECK(lDefinitions[0].Name == "Level1");
    CHECK(lDefinitions[0].Display == ELeaderboardDisplay::TimeMilliseconds);
}
