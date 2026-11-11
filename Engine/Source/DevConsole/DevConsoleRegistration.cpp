// The dev console's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "DevConsole/DevConsoleSubsystem.h"

namespace Opaax
{
    // Created before the UI: it takes the keys first while open.
    OPAAX_REGISTER_OBJECT(DevConsoleSubsystem);
}
