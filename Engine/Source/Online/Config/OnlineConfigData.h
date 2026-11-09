#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Online/OnlineTypes.h"

namespace Opaax
{
    // =============================================================================
    // OnlineLeaderboardSettings — one of the game's leaderboards, the same for every store.
    // =============================================================================
    struct OnlineLeaderboardSettings
    {
        OpaaxString         Name;
        ELeaderboardSort    Sort    = ELeaderboardSort::Ascending;
        ELeaderboardDisplay Display = ELeaderboardDisplay::TimeMilliseconds;

        LeaderboardDefinition ToDefinition() const { return LeaderboardDefinition{ Name, Sort, Display }; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(OnlineLeaderboardSettings, Name, Sort, Display)

        OPAAX_PROPERTIES(OnlineLeaderboardSettings,
                         OPAAX_PROP(Name).SetTooltip("The name the game uploads to (\"Level1\"), and the store's "
                                                     "board name."),
                         OPAAX_PROP(Sort).SetTooltip("Ascending: the smallest is best (times). Descending: the "
                                                     "largest (points)."),
                         OPAAX_PROP(Display).SetTooltip("How the store's own pages show the score."))
    };

    // =============================================================================
    // SteamOnlineSettings — what Steam needs to know about the game.
    // =============================================================================
    struct SteamOnlineSettings
    {
        /** The game's Steam app id. 0: Steam's launch or steam_appid.txt names it. */
        Uint32 AppId = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SteamOnlineSettings, AppId)

        OPAAX_PROPERTIES(SteamOnlineSettings,
                         OPAAX_PROP(AppId).SetFlags(EPropertyFlags::NeedRestart)
                             .SetTooltip("The game's Steam app id (480 is Valve's test app).\n"
                                         "0: the game runs on Steam only when Steam starts it."))
    };

    // =============================================================================
    // OnlineConfigData — <ProjectRoot>/Configs/Online.config: which stores a game tries, its
    //   leaderboards, each store's settings. Read at startup.
    // =============================================================================
    struct OnlineConfigData
    {
        /** The stores tried, in order; the first that starts is used. The store that launched the game goes first. */
        TDynArray<EOnlineBackend> Backends = { EOnlineBackend::Steam };

        TDynArray<OnlineLeaderboardSettings> Leaderboards;

        SteamOnlineSettings Steam;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(OnlineConfigData, Backends, Leaderboards, Steam)

        OPAAX_PROPERTIES(OnlineConfigData,
                         OPAAX_PROP(Backends).SetFlags(EPropertyFlags::NeedRestart)
                             .SetTooltip("The stores a game tries, in order. None of them: the game plays "
                                         "offline. The editor never uses one (--online fake for a fake store)."),
                         OPAAX_PROP(Leaderboards).SetTooltip("The game's leaderboards. A store that makes boards "
                                                             "makes them from this; another finds them by name."),
                         OPAAX_PROP(Steam))
    };

    /** The definitions of InData's leaderboards, for IOnlineServices::SetLeaderboards. */
    inline TDynArray<LeaderboardDefinition> ToLeaderboardDefinitions(const OnlineConfigData& InData)
    {
        TDynArray<LeaderboardDefinition> lDefinitions;
        for (const OnlineLeaderboardSettings& lBoard : InData.Leaderboards)
        {
            lDefinitions.push_back(lBoard.ToDefinition());
        }
        return lDefinitions;
    }
}
