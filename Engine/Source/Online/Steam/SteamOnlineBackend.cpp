#include "Online/Steam/SteamOnlineBackend.h"

#include "steam/steam_api.h"
#include "steam/steam_api_flat.h"

namespace Opaax
{
    namespace
    {
        // The SDK's own declarations type the functions loaded from steam_api: nothing links them.
        // SteamAPI_InitFlat (SDK 1.59 and later) says why Steam did not start.
        using FSteamInitFlat = decltype(&SteamAPI_InitFlat);
    }

    TUniquePtr<IOnlineBackend> SteamOnlineBackend::Start(const SteamOnlineSettings& InSettings, OpaaxString& OutReason)
    {
        (void)InSettings;

        OutReason = "the Steam backend is not written yet";
        return nullptr;
    }
}
