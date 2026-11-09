#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/CString.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Online/OnlineTypes.h"

// =============================================================================
// SteamRules — what the Steam backend decides without Steam: how ids and names read, which overlay
//   page a request opens, why Steam did not start. Outside Online/Steam so a build without the SDK
//   (CI) tests them too.
// =============================================================================
namespace Opaax::SteamRules
{
    /** A Steam id as an OnlinePlayerId's text: the 64-bit number in decimal. */
    inline OpaaxString IdText(const Uint64 InSteamId)
    {
        return OpaaxString::Format("{}", InSteamId);
    }

    /** The Steam id InText holds, or 0 when it holds none (Steam's ids are never 0). */
    inline Uint64 ParseId(const OpaaxString& InText)
    {
        // Digits only: the C parser would also take a sign or spaces.
        if (InText.IsEmpty() || InText.CStr()[0] < '0' || InText.CStr()[0] > '9')
        {
            return 0;
        }

        const char* lEnd = nullptr;
        const Uint64 lId = CString::ToUint64(InText.CStr(), &lEnd);
        return (lEnd != nullptr && *lEnd == '\0') ? lId : 0;
    }

    /** True when Steam knows the player's name: it answers "" or "[unknown]" until it does. */
    inline bool IsKnownName(const char* InName)
    {
        return InName != nullptr && InName[0] != '\0' && OpaaxStringView(InName) != OpaaxStringView("[unknown]");
    }

    /** The dialog Steam's overlay opens for InPage (ActivateGameOverlay); a profile goes to a player instead. */
    constexpr const char* OverlayDialog(const EOverlayPage InPage) noexcept
    {
        switch (InPage)
        {
            case EOverlayPage::Friends:       return "friends";
            case EOverlayPage::Achievements:  return "achievements";
            case EOverlayPage::Leaderboards:  return "stats";
            case EOverlayPage::PlayerProfile: return "steamid";
        }
        return "friends";
    }

    /**
     * Why Steam did not start, for the log and online.info, from SteamAPI_InitFlat's result (1: other
     * failure, 2: no Steam client, 3: client out of date) and its message.
     */
    inline OpaaxString InitFailure(const Int32 InResult, const char* InMessage)
    {
        const char* lWhy = "Steam refused to start";
        switch (InResult)
        {
            case 2: lWhy = "Steam is not running"; break;
            case 3: lWhy = "the Steam client is older than the game's SDK"; break;
            default: break;
        }

        if (InMessage == nullptr || InMessage[0] == '\0')
        {
            return OpaaxString(lWhy);
        }
        return OpaaxString::Format("{}: {}", lWhy, InMessage);
    }

    /** True when the config's app id goes in the environment: one is set and Steam did not set its own. */
    constexpr bool ShouldSetAppId(const Uint32 InConfigured, const bool bInAlreadySet) noexcept
    {
        return InConfigured != 0 && !bInAlreadySet;
    }

    // =========================================================================
    // Leaderboards — the numbers below are Steam's enum values; the backend checks each against the
    //   SDK at compile time.
    // =========================================================================

    /** Detail numbers Steam keeps with a score (k_cLeaderboardDetailsMax). */
    inline constexpr Uint32 MAX_DETAILS = 64;

    /** Players one DownloadLeaderboardEntriesForUsers asks for at most. */
    inline constexpr Uint64 MAX_PLAYERS_PER_DOWNLOAD = 100;

    /** Room for a leaderboard name, its end included (k_cchLeaderboardNameMax). */
    inline constexpr Uint64 BOARD_NAME_SIZE = 128;

    /** True when Steam takes InName as a leaderboard name: not empty, and fits with its end. */
    inline bool IsValidBoardName(const OpaaxString& InName)
    {
        return !InName.IsEmpty() && InName.GetLength() < BOARD_NAME_SIZE;
    }

    /** ELeaderboardSortMethod for InSort. */
    constexpr Int32 SortMethod(const ELeaderboardSort InSort) noexcept
    {
        return InSort == ELeaderboardSort::Ascending ? 1 : 2;
    }

    /** ELeaderboardDisplayType for InDisplay. */
    constexpr Int32 DisplayType(const ELeaderboardDisplay InDisplay) noexcept
    {
        switch (InDisplay)
        {
            case ELeaderboardDisplay::Numeric:          return 1;
            case ELeaderboardDisplay::TimeSeconds:      return 2;
            case ELeaderboardDisplay::TimeMilliseconds: return 3;
        }
        return 1;
    }

    /** ELeaderboardDataRequest for InRange: Steam reads First and Last the same way (ranks, or offsets around the player). */
    constexpr Int32 DataRequest(const ELeaderboardRange InRange) noexcept
    {
        switch (InRange)
        {
            case ELeaderboardRange::World:        return 0;
            case ELeaderboardRange::AroundPlayer: return 1;
            case ELeaderboardRange::Friends:      return 2;
        }
        return 0;
    }

    /** The sort Steam reports for a board (ELeaderboardSortMethod), or nothing for one it does not know. */
    constexpr TOptional<ELeaderboardSort> SortFromMethod(const Int32 InMethod) noexcept
    {
        switch (InMethod)
        {
            case 1: return ELeaderboardSort::Ascending;
            case 2: return ELeaderboardSort::Descending;
            default: return NullOpt;
        }
    }
}
