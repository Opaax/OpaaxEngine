#include "AssetLoaderRegistry.h"

namespace Opaax
{
    UnorderedMap<std::type_index, AssetLoaderRegistry::LoaderEntry> AssetLoaderRegistry::s_Loaders;
}