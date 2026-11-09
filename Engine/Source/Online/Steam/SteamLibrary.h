#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Platform/DynamicLibrary.h"

#include "steam/steam_api.h"
#include "steam/steam_api_flat.h"

// =============================================================================
// The Steamworks functions the engine calls, by their flat (C) names. Each is found in steam_api while
// the game runs, typed by the SDK's own declaration: a signature the SDK changes fails to compile here.
// SteamAPI_RestartAppIfNecessary is left out on purpose: the game runs without Steam (DRM-free), so
// it can never be called.
// =============================================================================
#define OPAAX_STEAM_FUNCTIONS(X)                                   \
    X(SteamAPI_InitFlat)                                           \
    X(SteamAPI_Shutdown)                                           \
    X(SteamAPI_GetHSteamPipe)                                      \
    X(SteamAPI_ManualDispatch_Init)                                \
    X(SteamAPI_ManualDispatch_RunFrame)                            \
    X(SteamAPI_ManualDispatch_GetNextCallback)                     \
    X(SteamAPI_ManualDispatch_FreeLastCallback)                    \
    X(SteamAPI_ManualDispatch_GetAPICallResult)                    \
    X(SteamAPI_SteamUser_v023)                                     \
    X(SteamAPI_ISteamUser_GetSteamID)                              \
    X(SteamAPI_ISteamUser_BLoggedOn)                               \
    X(SteamAPI_SteamFriends_v018)                                  \
    X(SteamAPI_ISteamFriends_GetPersonaName)                       \
    X(SteamAPI_ISteamFriends_GetFriendPersonaName)                 \
    X(SteamAPI_ISteamFriends_GetFriendCount)                       \
    X(SteamAPI_ISteamFriends_GetFriendByIndex)                     \
    X(SteamAPI_ISteamFriends_RequestUserInformation)               \
    X(SteamAPI_ISteamFriends_ActivateGameOverlay)                  \
    X(SteamAPI_ISteamFriends_ActivateGameOverlayToUser)            \
    X(SteamAPI_SteamUtils_v011)                                    \
    X(SteamAPI_ISteamUtils_GetAppID)                               \
    X(SteamAPI_ISteamUtils_IsOverlayEnabled)                       \
    X(SteamAPI_SteamUserStats_v013)                                \
    X(SteamAPI_ISteamUserStats_FindOrCreateLeaderboard)            \
    X(SteamAPI_ISteamUserStats_GetLeaderboardSortMethod)           \
    X(SteamAPI_ISteamUserStats_UploadLeaderboardScore)             \
    X(SteamAPI_ISteamUserStats_DownloadLeaderboardEntries)         \
    X(SteamAPI_ISteamUserStats_DownloadLeaderboardEntriesForUsers) \
    X(SteamAPI_ISteamUserStats_GetDownloadedLeaderboardEntry)

namespace Opaax
{
    // =============================================================================
    // SteamLibrary — steam_api opened while the game runs, and its functions. Nothing links it, so a
    //   game without the file starts and plays offline.
    // =============================================================================
    struct SteamLibrary
    {
        /** steam_api's file name on this platform. */
        static const char* GetFileName() noexcept;

        /**
         * Opens InPath and finds every function of OPAAX_STEAM_FUNCTIONS.
         * @param OutError Why not: the file is missing, or lacks a function (a library older than the SDK)
         */
        bool Load(const OpaaxString& InPath, OpaaxString& OutError);

        DynamicLibrary Library;

#define OPAAX_STEAM_FUNCTION_POINTER(Name) decltype(&::Name) Name = nullptr;
        OPAAX_STEAM_FUNCTIONS(OPAAX_STEAM_FUNCTION_POINTER)
#undef OPAAX_STEAM_FUNCTION_POINTER
    };
}
