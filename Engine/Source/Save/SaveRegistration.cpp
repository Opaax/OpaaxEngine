// The Save module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Save/SaveGame.h"
#include "Save/SaveGameSubsystem.h"

namespace Opaax
{
    // The root of every save class: the games' derive from it.
    OPAAX_REGISTER_OBJECT(SaveGame);

    // Every game gets one.
    OPAAX_REGISTER_OBJECT(SaveGameSubsystem);
}
