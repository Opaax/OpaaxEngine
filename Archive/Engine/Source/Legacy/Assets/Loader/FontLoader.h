#pragma once

#include "Assets/Loader/IAssetLoader.hpp"

namespace Opaax
{
    class FontAsset;

    // =============================================================================
    // FontLoader
    //
    // Constructs a FontAsset from an absolute TTF/OTF path. The FontAsset ctor
    // drives the TTF parse + R8 atlas bake (stb_truetype) + kerning LUT build.
    // =============================================================================
    /**
     * @class FontLoader
     * IAssetLoader specialization for FontAsset. Mirrors TextureLoader.
     */
    class OPAAX_API FontLoader final : public IAssetLoader<FontAsset>
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin IAssetLoader interface
    public:
        FontAsset* Load(const char* InAbsPath, OpaaxStringID InCanonicalID) override;
        bool       IsValid(FontAsset* InAsset)                              override;
        //~ End IAssetLoader interface
    };

} // namespace Opaax
