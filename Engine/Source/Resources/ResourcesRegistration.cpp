// The Resources module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Resources/Asset/Asset.h"

namespace Opaax
{
    // The root of every asset class. Its files load as AssetResource, which the engine registers itself.
    OPAAX_REGISTER_OBJECT(Asset);
}
