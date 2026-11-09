#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // EOnlineResult — how an online request ended. The same names for every store, so a game handles
    //   them once.
    // =============================================================================
    enum class EOnlineResult : Uint8
    {
        Ok,

        /** No store runs: offline, a build without one, the editor. */
        Unavailable,

        /** The store runs but has no such feature (a file on a leaderboard entry, on some stores). */
        Unsupported,

        /** No such leaderboard, file or achievement. */
        NotFound,

        /** The store refused or the network failed; its message is in the log. */
        Failed,

        /** Cancelled before it ended (OnlineServices::Cancel, the game ended). */
        Cancelled,

        /** The store did not answer in time. */
        TimedOut,

        /** The player's storage on the store is full. */
        QuotaExceeded,

        /** The store runs but the player is not signed in to it. */
        NotSignedIn
    };

    constexpr const char* ToString(const EOnlineResult InResult) noexcept
    {
        switch (InResult)
        {
            case EOnlineResult::Ok:            return "Ok";
            case EOnlineResult::Unavailable:   return "Unavailable";
            case EOnlineResult::Unsupported:   return "Unsupported";
            case EOnlineResult::NotFound:      return "NotFound";
            case EOnlineResult::Failed:        return "Failed";
            case EOnlineResult::Cancelled:     return "Cancelled";
            case EOnlineResult::TimedOut:      return "TimedOut";
            case EOnlineResult::QuotaExceeded: return "QuotaExceeded";
            case EOnlineResult::NotSignedIn:   return "NotSignedIn";
        }
        return "Unknown";
    }

    // =============================================================================
    // EOnlineBackend — the store behind the online services.
    // =============================================================================
    enum class EOnlineBackend : Uint8
    {
        /** No store: every request answers Unavailable. */
        None,

        /** An in-memory store, for tests and for making a game's online screens without a store. */
        Fake,

        Steam
    };

    constexpr const char* ToString(const EOnlineBackend InBackend) noexcept
    {
        switch (InBackend)
        {
            case EOnlineBackend::None:  return "None";
            case EOnlineBackend::Fake:  return "Fake";
            case EOnlineBackend::Steam: return "Steam";
        }
        return "Unknown";
    }

    // =============================================================================
    // Ids — opaque text, read only by the backend that made them (a Steam id is a number, an Epic one
    //   is not). Shown as text in logs and automation.
    // =============================================================================

    /** A player on a store. */
    struct OnlinePlayerId
    {
        EOnlineBackend Backend = EOnlineBackend::None;
        OpaaxString    Value;

        bool IsValid() const noexcept { return !Value.IsEmpty(); }
        bool operator==(const OnlinePlayerId& InOther) const { return Backend == InOther.Backend && Value == InOther.Value; }
    };

    /** A player and the name the store shows for them (empty while the store has not said). */
    struct OnlinePlayer
    {
        OnlinePlayerId Id;
        OpaaxString    Name;
    };

    /** A file kept by the store, such as a ghost attached to a leaderboard entry. */
    struct OnlineFileId
    {
        OpaaxString Value;

        bool IsValid() const noexcept { return !Value.IsEmpty(); }
        bool operator==(const OnlineFileId& InOther) const { return Value == InOther.Value; }
    };

    /** A leaderboard as the store knows it, once found. */
    struct OnlineLeaderboardHandle
    {
        OpaaxString Value;

        bool IsValid() const noexcept { return !Value.IsEmpty(); }
    };

    /** One request, to cancel it. 0 is no request. */
    using OnlineRequestId = Uint64;
    inline constexpr OnlineRequestId INVALID_ONLINE_REQUEST = 0;

    // =============================================================================
    // Leaderboards
    // =============================================================================

    /** Which end of a leaderboard is the best. */
    enum class ELeaderboardSort : Uint8
    {
        /** Smallest first: times. */
        Ascending,

        /** Largest first: points. */
        Descending
    };

    constexpr const char* ToString(const ELeaderboardSort InSort) noexcept
    {
        switch (InSort)
        {
            case ELeaderboardSort::Ascending:  return "Ascending";
            case ELeaderboardSort::Descending: return "Descending";
        }
        return "Unknown";
    }

    /** How a store's own pages show the score. */
    enum class ELeaderboardDisplay : Uint8
    {
        Numeric,
        TimeSeconds,
        TimeMilliseconds
    };

    constexpr const char* ToString(const ELeaderboardDisplay InDisplay) noexcept
    {
        switch (InDisplay)
        {
            case ELeaderboardDisplay::Numeric:          return "Numeric";
            case ELeaderboardDisplay::TimeSeconds:      return "TimeSeconds";
            case ELeaderboardDisplay::TimeMilliseconds: return "TimeMilliseconds";
        }
        return "Unknown";
    }

    /** Which entries a download asks for. */
    enum class ELeaderboardRange : Uint8
    {
        /** By rank, from the top: First and Last are ranks (1 is the best). */
        World,

        /** The player and their friends: First and Last are ignored. */
        Friends,

        /** Around the player: First and Last are offsets (-5 to 5 is eleven entries). */
        AroundPlayer
    };

    constexpr const char* ToString(const ELeaderboardRange InRange) noexcept
    {
        switch (InRange)
        {
            case ELeaderboardRange::World:        return "World";
            case ELeaderboardRange::Friends:      return "Friends";
            case ELeaderboardRange::AroundPlayer: return "AroundPlayer";
        }
        return "Unknown";
    }

    /** A leaderboard of the game, the same for every store: a store makes or finds its own from it. */
    struct LeaderboardDefinition
    {
        OpaaxString         Name;
        ELeaderboardSort    Sort    = ELeaderboardSort::Ascending;
        ELeaderboardDisplay Display = ELeaderboardDisplay::TimeMilliseconds;
    };

    /** One line of a leaderboard. */
    struct LeaderboardEntry
    {
        OnlinePlayer     Player;

        /** 1 is the best. */
        Int32            Rank  = 0;
        Int32            Score = 0;

        /** Numbers the game kept with the score (a checkpoint split, a character). */
        TDynArray<Int32> Details;

        /** The file attached to the entry (a ghost); invalid when none. */
        OnlineFileId     File;
    };

    /** What an upload did. */
    struct ScoreUploadResult
    {
        /** False when the player's best was better: the entry and its file are unchanged. */
        bool  bScoreChanged = false;

        /** The player's rank after the upload, and before it (0: no entry before). */
        Int32 Rank         = 0;
        Int32 PreviousRank = 0;
    };

    // =============================================================================
    // Achievements and the overlay
    // =============================================================================

    /** An achievement, and whether the player has it. */
    struct OnlineAchievement
    {
        OpaaxString Name;
        bool        bUnlocked = false;
    };

    /** A page of the store's overlay. */
    enum class EOverlayPage : Uint8
    {
        Friends,
        Achievements,
        Leaderboards,

        /** A player's profile: needs the player. */
        PlayerProfile
    };

    // =============================================================================
    // OnlineCapabilities — what the store behind the services can do. A game asks once and hides what
    //   it lacks, rather than learning it from Unsupported answers.
    // =============================================================================
    struct OnlineCapabilities
    {
        bool bIdentity     = false;
        bool bLeaderboards = false;

        /** The store makes a leaderboard the first time it is asked for; otherwise it must exist in its portal. */
        bool bLeaderboardsMadeAtRunTime = false;

        /** The store gives the friends range itself; otherwise the services build it from the friends list. */
        bool bFriendsRange = false;

        /** A file attached to a leaderboard entry. */
        bool   bLeaderboardFiles = false;
        Uint64 MaxFileBytes      = 0;

        /** Detail numbers kept with a score. */
        Uint32 MaxDetails = 0;

        bool bAchievements = false;
        bool bOverlay      = false;
    };
}

OPAAX_ENUM_VALUES(Opaax::EOnlineBackend, None, Fake, Steam)
OPAAX_ENUM_VALUES(Opaax::ELeaderboardSort, Ascending, Descending)
OPAAX_ENUM_VALUES(Opaax::ELeaderboardDisplay, Numeric, TimeSeconds, TimeMilliseconds)
