#pragma once

#include "Assets/Loader/IAssetLoader.hpp"

namespace Opaax
{
    class Texture2D;

    /**
     * @class TextureLoader
     *
     * Constructs a Texture2D asset from an absolute file path. The Texture2D
     * ctor drives the underlying GPU upload via stb_image inside OpenGLTexture2D.
     */
    class OPAAX_API TextureLoader final : public IAssetLoader<Texture2D>
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin IAssetLoader interface
    public:
        Texture2D* Load(const char* InAbsPath, OpaaxStringID InCanonicalID) override;
        bool       IsValid(Texture2D* InAsset)                              override;
        //~ End IAssetLoader interface
    };

} // namespace Opaax
