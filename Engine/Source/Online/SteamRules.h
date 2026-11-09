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
}
