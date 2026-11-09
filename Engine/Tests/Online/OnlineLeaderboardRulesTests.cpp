// Suite: OnlineLeaderboardRules (Online/OnlineLeaderboardRules.h), how a board keeps and ranks scores
// and which entries a download covers.
#include <doctest.h>

#include "Online/OnlineLeaderboardRules.h"

using namespace Opaax;

namespace
{
    LeaderboardEntry Entry(const char* InPlayer, const Int32 InScore)
    {
        LeaderboardEntry lEntry;
        lEntry.Player.Id = OnlinePlayerId{ EOnlineBackend::Fake, InPlayer };
        lEntry.Score     = InScore;
        return lEntry;
    }
}

TEST_CASE("OnlineLeaderboardRules: better is smaller on a time board, larger on a points board, and a tie is not")
{
    CHECK(OnlineLeaderboardRules::IsBetter(ELeaderboardSort::Ascending, 900, 1000));
    CHECK_FALSE(OnlineLeaderboardRules::IsBetter(ELeaderboardSort::Ascending, 1100, 1000));
    CHECK_FALSE(OnlineLeaderboardRules::IsBetter(ELeaderboardSort::Ascending, 1000, 1000));

    CHECK(OnlineLeaderboardRules::IsBetter(ELeaderboardSort::Descending, 1100, 1000));
    CHECK_FALSE(OnlineLeaderboardRules::IsBetter(ELeaderboardSort::Descending, 900, 1000));
    CHECK_FALSE(OnlineLeaderboardRules::IsBetter(ELeaderboardSort::Descending, 1000, 1000));
}

TEST_CASE("OnlineLeaderboardRules: best first, ranks from 1 without gaps, a tie to the earlier score")
{
    TDynArray<LeaderboardEntry> lTimes = { Entry("a", 1200), Entry("b", 900), Entry("c", 1000), Entry("d", 900) };
    OnlineLeaderboardRules::SortAndRank(lTimes, ELeaderboardSort::Ascending);

    REQUIRE(lTimes.size() == 4);
    CHECK(lTimes[0].Player.Id.Value == "b");
    CHECK(lTimes[1].Player.Id.Value == "d");
    CHECK(lTimes[2].Player.Id.Value == "c");
    CHECK(lTimes[3].Player.Id.Value == "a");
    for (Uint64 lIndex = 0; lIndex < lTimes.size(); ++lIndex)
    {
        CHECK(lTimes[lIndex].Rank == static_cast<Int32>(lIndex + 1));
    }

    TDynArray<LeaderboardEntry> lPoints = { Entry("a", 10), Entry("b", 30), Entry("c", 20) };
    OnlineLeaderboardRules::SortAndRank(lPoints, ELeaderboardSort::Descending);
    CHECK(lPoints[0].Player.Id.Value == "b");
    CHECK(lPoints[2].Player.Id.Value == "a");
}

TEST_CASE("OnlineLeaderboardRules: the entries a download covers, clipped to the board")
{
    using namespace OnlineLeaderboardRules;
    struct Row { ELeaderboardRange Range; Int32 First; Int32 Last; Int64 Player; Uint64 Begin; Uint64 End; };

    // A board of 10 entries.
    const Row lRows[] = {
        { ELeaderboardRange::World,        1,  10, -1, 0, 10 },  // the whole board
        { ELeaderboardRange::World,        1,   3, -1, 0,  3 },  // the podium
        { ELeaderboardRange::World,        8,  20, -1, 7, 10 },  // past the end: cut
        { ELeaderboardRange::World,        0,   2, -1, 0,  2 },  // rank 0 reads as 1
        { ELeaderboardRange::World,        5,   4, -1, 0,  0 },  // last before first: nothing
        { ELeaderboardRange::World,       11,  20, -1, 0,  0 },  // past the board: nothing
        { ELeaderboardRange::AroundPlayer, -2,  2,  4, 2,  7 },  // five around the 5th
        { ELeaderboardRange::AroundPlayer, -2,  2,  0, 0,  3 },  // the best: cut, not moved
        { ELeaderboardRange::AroundPlayer, -2,  2,  9, 7, 10 },  // the last: cut, not moved
        { ELeaderboardRange::AroundPlayer, -2,  2, -1, 0,  0 },  // no entry: nothing
        { ELeaderboardRange::AroundPlayer,  0,  0,  3, 3,  4 },  // the player alone
        { ELeaderboardRange::Friends,       0,  0, -1, 0, 10 },  // all: the caller keeps the friends
    };

    for (const Row& lRow : lRows)
    {
        CAPTURE(ToString(lRow.Range));
        CAPTURE(lRow.First);
        CAPTURE(lRow.Last);
        CAPTURE(lRow.Player);
        const IndexRange lRange = SelectRange(10, lRow.Range, lRow.First, lRow.Last, lRow.Player);
        if (lRow.Begin == lRow.End)
        {
            CHECK(lRange.IsEmpty());
        }
        else
        {
            CHECK(lRange.Begin == lRow.Begin);
            CHECK(lRange.End == lRow.End);
        }
    }

    CHECK(SelectRange(0, ELeaderboardRange::World, 1, 10, -1).IsEmpty());
}
