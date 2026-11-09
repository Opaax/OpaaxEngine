#pragma once

#include "Core/Algo/Algo.h"
#include "Core/OpaaxTypes.h"
#include "Online/OnlineTypes.h"

// =============================================================================
// OnlineLeaderboardRules — how a leaderboard keeps and ranks scores, as the stores do it: one entry
//   per player at their best, best first, a tie going to the earlier score, ranks 1, 2, 3 with no
//   gaps. The fake store keeps its boards with them; a game can rank a local board the same way.
// =============================================================================
namespace Opaax::OnlineLeaderboardRules
{
    /** True when InScore beats InBest on a board sorted InSort (a tie does not). */
    constexpr bool IsBetter(const ELeaderboardSort InSort, const Int32 InScore, const Int32 InBest) noexcept
    {
        return (InSort == ELeaderboardSort::Ascending) ? (InScore < InBest) : (InScore > InBest);
    }

    /**
     * Orders InOutEntries best first and numbers their ranks from 1. Equal scores keep the order they
     * came in: give the entries in upload order and the earlier score ranks first.
     */
    inline void SortAndRank(TDynArray<LeaderboardEntry>& InOutEntries, const ELeaderboardSort InSort)
    {
        Algo::StableSort(InOutEntries, [InSort](const LeaderboardEntry& InA, const LeaderboardEntry& InB)
        {
            return IsBetter(InSort, InA.Score, InB.Score);
        });

        for (Uint64 lIndex = 0; lIndex < InOutEntries.size(); ++lIndex)
        {
            InOutEntries[lIndex].Rank = static_cast<Int32>(lIndex + 1);
        }
    }

    /** A run of entries [Begin, End) in a sorted board. */
    struct IndexRange
    {
        Uint64 Begin = 0;
        Uint64 End   = 0;

        bool   IsEmpty() const noexcept { return End <= Begin; }
        Uint64 Count() const noexcept   { return IsEmpty() ? 0 : End - Begin; }
    };

    /**
     * Which entries of a sorted board of InCount a download covers, clipped to the board.
     *  - World: ranks InFirst to InLast (1 is the best).
     *  - AroundPlayer: InFirst to InLast places from the player's (-2 to 2 is five), nothing when the
     *    player has no entry (InPlayerIndex < 0). Near an end the run is cut, not moved.
     *  - Friends: the whole board; the caller keeps the friends' entries.
     */
    constexpr IndexRange SelectRange(const Uint64 InCount, const ELeaderboardRange InRange, const Int32 InFirst,
                                     const Int32 InLast, const Int64 InPlayerIndex) noexcept
    {
        const Int64 lCount = static_cast<Int64>(InCount);
        Int64 lBegin = 0;
        Int64 lEnd   = 0;

        switch (InRange)
        {
            case ELeaderboardRange::World:
                lBegin = (InFirst < 1 ? 1 : static_cast<Int64>(InFirst)) - 1;
                lEnd   = static_cast<Int64>(InLast);
                break;

            case ELeaderboardRange::AroundPlayer:
                if (InPlayerIndex < 0 || InPlayerIndex >= lCount)
                {
                    return {};
                }
                lBegin = InPlayerIndex + InFirst;
                lEnd   = InPlayerIndex + InLast + 1;
                break;

            case ELeaderboardRange::Friends:
                lEnd = lCount;
                break;
        }

        lBegin = (lBegin < 0) ? 0 : lBegin;
        lEnd   = (lEnd > lCount) ? lCount : lEnd;
        if (lEnd <= lBegin)
        {
            return {};
        }
        return IndexRange{ static_cast<Uint64>(lBegin), static_cast<Uint64>(lEnd) };
    }
}
