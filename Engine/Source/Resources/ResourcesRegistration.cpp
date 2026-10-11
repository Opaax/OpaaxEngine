// The Resources module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Resources/DataAsset/DataAssetResource.h"

namespace Opaax
{
    // Any struct registered with OPAAX_REGISTER_DATA_ASSET: one extension, the type is in the file.
    OPAAX_REGISTER_NAMED_RESOURCE(DataAssetResource, "DataAsset");
}
