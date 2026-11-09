// Suite: SteamRules (Online/SteamRules.h), what the Steam backend decides without Steam: ids, names,
// overlay pages, why Steam did not start, when the app id goes in the environment.
#include <doctest.h>

#include "Online/SteamRules.h"

using namespace Opaax;

TEST_CASE("SteamRules: a Steam id reads back from its text, and nothing else reads as one")
{
    const Uint64 lId = 76561197960287930ull;
    CHECK(SteamRules::IdText(lId) == "76561197960287930");
    CHECK(SteamRules::ParseId(SteamRules::IdText(lId)) == lId);
    CHECK(SteamRules::ParseId("18446744073709551615") == 18446744073709551615ull);

    CHECK(SteamRules::ParseId("") == 0);
    CHECK(SteamRules::ParseId("fake:1") == 0);
    CHECK(SteamRules::ParseId("123abc") == 0);
    CHECK(SteamRules::ParseId("-1") == 0);
    CHECK(SteamRules::ParseId(" 12") == 0);
}

TEST_CASE("SteamRules: a name Steam does not know yet is no name")
{
    CHECK(SteamRules::IsKnownName("Ada"));
    CHECK_FALSE(SteamRules::IsKnownName(""));
    CHECK_FALSE(SteamRules::IsKnownName(nullptr));
    CHECK_FALSE(SteamRules::IsKnownName("[unknown]"));
}

TEST_CASE("SteamRules: each overlay page opens a Steam dialog")
{
    CHECK(CString::Compare(SteamRules::OverlayDialog(EOverlayPage::Friends), "friends") == 0);
    CHECK(CString::Compare(SteamRules::OverlayDialog(EOverlayPage::Achievements), "achievements") == 0);
    CHECK(CString::Compare(SteamRules::OverlayDialog(EOverlayPage::Leaderboards), "stats") == 0);
    CHECK(CString::Compare(SteamRules::OverlayDialog(EOverlayPage::PlayerProfile), "steamid") == 0);
}

TEST_CASE("SteamRules: why Steam did not start, from its result and message")
{
    CHECK(SteamRules::InitFailure(2, "") == "Steam is not running");
    CHECK(SteamRules::InitFailure(3, nullptr) == "the Steam client is older than the game's SDK");
    CHECK(SteamRules::InitFailure(1, "No appID found") == "Steam refused to start: No appID found");
    CHECK(SteamRules::InitFailure(2, "Could not connect").Contains("Steam is not running: Could not connect"));
}

TEST_CASE("SteamRules: the configured app id goes in the environment only when Steam set none")
{
    CHECK(SteamRules::ShouldSetAppId(480, false));
    CHECK_FALSE(SteamRules::ShouldSetAppId(480, true));
    CHECK_FALSE(SteamRules::ShouldSetAppId(0, false));
}

TEST_CASE("SteamRules: a leaderboard name Steam takes")
{
    CHECK(SteamRules::IsValidBoardName("Level1"));
    CHECK_FALSE(SteamRules::IsValidBoardName(""));

    OpaaxString lLongest;
    for (Uint64 lIndex = 0; lIndex + 1 < SteamRules::BOARD_NAME_SIZE; ++lIndex)
    {
        lLongest.Append("a");
    }
    CHECK(SteamRules::IsValidBoardName(lLongest));
    lLongest.Append("a");
    CHECK_FALSE(SteamRules::IsValidBoardName(lLongest));
}

TEST_CASE("SteamRules: a leaderboard's sort, display and range as Steam's values")
{
    CHECK(SteamRules::SortMethod(ELeaderboardSort::Ascending) == 1);
    CHECK(SteamRules::SortMethod(ELeaderboardSort::Descending) == 2);

    CHECK(SteamRules::DisplayType(ELeaderboardDisplay::Numeric) == 1);
    CHECK(SteamRules::DisplayType(ELeaderboardDisplay::TimeSeconds) == 2);
    CHECK(SteamRules::DisplayType(ELeaderboardDisplay::TimeMilliseconds) == 3);

    CHECK(SteamRules::DataRequest(ELeaderboardRange::World) == 0);
    CHECK(SteamRules::DataRequest(ELeaderboardRange::AroundPlayer) == 1);
    CHECK(SteamRules::DataRequest(ELeaderboardRange::Friends) == 2);
}

TEST_CASE("SteamRules: the sort Steam reports reads back, and an unknown one reads as none")
{
    for (const ELeaderboardSort lSort : { ELeaderboardSort::Ascending, ELeaderboardSort::Descending })
    {
        CHECK(SteamRules::SortFromMethod(SteamRules::SortMethod(lSort)) == lSort);
    }
    CHECK_FALSE(SteamRules::SortFromMethod(0).has_value());
    CHECK_FALSE(SteamRules::SortFromMethod(7).has_value());
}
