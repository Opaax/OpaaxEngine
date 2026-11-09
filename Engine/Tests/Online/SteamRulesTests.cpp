// Suite: SteamRules (Online/SteamRules.h), what the Steam backend decides without Steam: ids, names,
// overlay pages, why Steam did not start, when the app id goes in the environment, an entry's file.
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

TEST_CASE("SteamRules: a board's file in the Steam Cloud is named from the board, one per board")
{
    CHECK(SteamRules::BoardFileName("Level1") == "leaderboards/Level1.bin");
    CHECK(SteamRules::BoardFileName("Level1") == SteamRules::BoardFileName("Level1"));
    CHECK(SteamRules::BoardFileName("speed_run-2") == "leaderboards/speed_run-2.bin");

    // Anything else is escaped: no folder of its own, and no two boards on one file.
    CHECK(SteamRules::BoardFileName("World 1/Any%") == "leaderboards/World%201%2FAny%25.bin");
    CHECK(SteamRules::BoardFileName("A.B") != SteamRules::BoardFileName("A_B"));
    CHECK(SteamRules::BoardFileName("A B") != SteamRules::BoardFileName("A%20B"));
    CHECK(SteamRules::BoardFileName("\xC3\xA9") == "leaderboards/%C3%A9.bin");

    CHECK(SteamRules::BoardFileName("").IsEmpty());
}

TEST_CASE("SteamRules: a board's file name fits Steam's, or there is none")
{
    // "leaderboards/" and ".bin" take 17 of the 259 characters.
    OpaaxString lBoard;
    for (Uint64 lIndex = 0; lIndex < SteamRules::BOARD_NAME_SIZE - 1; ++lIndex)
    {
        lBoard.Append("a");
    }
    CHECK(SteamRules::BoardFileName(lBoard).GetLength() == 17 + SteamRules::BOARD_NAME_SIZE - 1);

    // Escaped, the same length of name takes three times the room.
    OpaaxString lSpaces;
    for (Uint64 lIndex = 0; lIndex < 80; ++lIndex)
    {
        lSpaces.Append(" ");
    }
    CHECK(SteamRules::BoardFileName(lSpaces).GetLength() == 17 + 240);
    lSpaces.Append(" ");
    CHECK(SteamRules::BoardFileName(lSpaces).IsEmpty());
}

TEST_CASE("SteamRules: the largest file is the quota or one write, none with the Steam Cloud off")
{
    const Uint64 lQuota = 1024ull * 1024ull;
    CHECK(SteamRules::MaxFileBytes(true, true, lQuota) == lQuota);
    CHECK(SteamRules::MaxFileBytes(true, true, 4 * SteamRules::MAX_FILE_BYTES) == SteamRules::MAX_FILE_BYTES);
    CHECK(SteamRules::MaxFileBytes(true, true, 0) == 0);

    CHECK(SteamRules::MaxFileBytes(false, true, lQuota) == 0);
    CHECK(SteamRules::MaxFileBytes(true, false, lQuota) == 0);
}

TEST_CASE("SteamRules: a file too large or a full cloud is refused before Steam is asked")
{
    CHECK(SteamRules::CheckFileSize(100, 1000, 1000, 0) == EOnlineResult::Ok);
    CHECK(SteamRules::CheckFileSize(1000, 1000, 1000, 0) == EOnlineResult::Ok);
    CHECK(SteamRules::CheckFileSize(0, 0, 0, 0) == EOnlineResult::Ok);

    // Over MaxFileBytes.
    CHECK(SteamRules::CheckFileSize(1001, 1000, 5000, 0) == EOnlineResult::QuotaExceeded);

    // Over the room left in the cloud.
    CHECK(SteamRules::CheckFileSize(500, 1000, 499, 0) == EOnlineResult::QuotaExceeded);
    CHECK(SteamRules::CheckFileSize(1, 1000, 0, 0) == EOnlineResult::QuotaExceeded);

    // The file it replaces gives its room back.
    CHECK(SteamRules::CheckFileSize(500, 1000, 100, 400) == EOnlineResult::Ok);
    CHECK(SteamRules::CheckFileSize(500, 1000, 100, 399) == EOnlineResult::QuotaExceeded);
}

TEST_CASE("SteamRules: Steam's file results as online results")
{
    CHECK(SteamRules::FileResult(1) == EOnlineResult::Ok);
    CHECK(SteamRules::FileResult(25) == EOnlineResult::QuotaExceeded);
    CHECK(SteamRules::FileResult(54) == EOnlineResult::QuotaExceeded);
    CHECK(SteamRules::FileResult(9) == EOnlineResult::NotFound);
    CHECK(SteamRules::FileResult(16) == EOnlineResult::TimedOut);
    CHECK(SteamRules::FileResult(21) == EOnlineResult::NotSignedIn);

    CHECK(SteamRules::FileResult(2) == EOnlineResult::Failed);
    CHECK(SteamRules::FileResult(15) == EOnlineResult::Failed);
    CHECK(SteamRules::FileResult(0) == EOnlineResult::Failed);
}
