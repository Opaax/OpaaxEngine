// The Resources module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetResource.h"

namespace Opaax
{
    // Every asset class: one extension, the class is in the file.
    OPAAX_REGISTER_NAMED_RESOURCE(AssetResource, "Asset");
    OPAAX_REGISTER_OBJECT(Asset);
}
