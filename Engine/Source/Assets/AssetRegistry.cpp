#include "AssetRegistry.h"
#include "IAsset.hpp" //Future

namespace Opaax
{
    UnorderedMap<Uint32, AssetRegistry::AssetEntry> AssetRegistry::s_Assets;
}