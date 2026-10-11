#pragma once

#include "Assets/Loader/IAssetLoader.hpp"

namespace Opaax
{
    class ShaderAsset;

    // =============================================================================
    // ShaderLoader
    //
    // Constructs a ShaderAsset from an on-disk single-file shader (.glsl with
    // #type vertex / #type fragment sections). The ShaderAsset ctor reads + splits
    // the file and builds the backend IShader. Mirrors FontLoader / TextureLoader.
    // =============================================================================
    /**
     * @class ShaderLoader
     * IAssetLoader specialization for ShaderAsset.
     */
    class OPAAX_API ShaderLoader final : public IAssetLoader<ShaderAsset>
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin IAssetLoader interface
    public:
        ShaderAsset* Load(const char* InAbsPath, OpaaxStringID InCanonicalID) override;
        bool         IsValid(ShaderAsset* InAsset)                            override;
        //~ End IAssetLoader interface
    };

} // namespace Opaax
