#include "AssetRegistry.h"
#include "IAsset.hpp"

namespace Opaax
{
    UnorderedMap<Uint32, AssetRegistry::AssetEntry> AssetRegistry::s_Assets;

    namespace Internal
    {
        void* AssetRegistry_TryResolveTyped(Uint32 InKey, const std::type_index& InExpected) noexcept
        {
            auto& lAssets = AssetRegistry::s_Assets;
            const auto lIt = lAssets.find(InKey);
            if (lIt == lAssets.end())                  { return nullptr; }
            if (lIt->second.Type != InExpected)        { return nullptr; }
            return lIt->second.Ptr;
        }
    }
}
