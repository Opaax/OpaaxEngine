// The Online module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Online/Config/Config_Online.h"
#include "Online/OnlineSubsystem.h"

namespace Opaax
{
    // Online.config: the stores a game tries, its leaderboards, each store's settings.
    OPAAX_REGISTER_OBJECT(Config_Online);

    // Every game gets one.
    OPAAX_REGISTER_OBJECT(OnlineSubsystem);
}
