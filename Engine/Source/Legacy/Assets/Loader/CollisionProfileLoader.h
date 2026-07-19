#pragma once

#include "Assets/Loader/IAssetLoader.hpp"

namespace Opaax
{
    class CollisionProfile;

    // =============================================================================
    // CollisionProfileLoader
    //
    // IAssetLoader specialization for CollisionProfile. The CollisionProfile ctor
    // reads + parses the JSON file. Mirrors FontLoader.
    // =============================================================================
    class OPAAX_API CollisionProfileLoader final : public IAssetLoader<CollisionProfile>
    {
        //~ Begin IAssetLoader interface
    public:
        CollisionProfile* Load(const char* InAbsPath, OpaaxStringID InCanonicalID) override;
        bool              IsValid(CollisionProfile* InAsset)                       override;
        //~ End IAssetLoader interface
    };

} // namespace Opaax
