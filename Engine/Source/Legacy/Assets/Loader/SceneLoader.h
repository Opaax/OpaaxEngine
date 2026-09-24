#pragma once

#include "Assets/Loader/IAssetLoader.hpp"

namespace Opaax
{
    class SceneAsset;

    // =============================================================================
    // SceneLoader
    // =============================================================================
    /**
     * @class SceneLoader
     *
     * Constructs a SceneAsset descriptor from an absolute *.scene.json path.
     * SceneAsset is metadata-only (path + ID) — runtime Scene instantiation
     * stays SceneManager's responsibility, so this loader does not deserialize
     * the scene file; it just stamps a descriptor into AssetRegistry.
     */
    class OPAAX_API SceneLoader final : public IAssetLoader<SceneAsset>
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin IAssetLoader interface
    public:
        SceneAsset* Load(const char* InAbsPath, OpaaxStringID InCanonicalID) override;
        bool        IsValid(SceneAsset* InAsset)                             override;
        //~ End IAssetLoader interface
    };

} // namespace Opaax
