#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Online/Config/OnlineConfigData.h"
#include "Online/IOnlineBackend.h"

namespace Opaax
{
    // =============================================================================
    // SteamOnlineBackend — Steam, through the Steamworks SDK's library (steam_api) loaded while the game
    //   runs: a game without it, or without Steam running, plays offline. Compiled only when the SDK is
    //   found (OPAAX_ONLINE_STEAM); only OnlineBackendFactory.cpp includes this header.
    // =============================================================================
    class SteamOnlineBackend
    {
    public:
        /**
         * Starts Steam for the game.
         * @param OutReason Why it did not start; untouched on success
         * @return Null when it did not start
         */
        static TUniquePtr<IOnlineBackend> Start(const SteamOnlineSettings& InSettings, OpaaxString& OutReason);
    };
}
