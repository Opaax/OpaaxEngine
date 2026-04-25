#pragma once

#include "Assets/Loader/IAssetLoader.hpp"
#include "RHI/OpenGL/OpenGLTexture2D.h"

namespace Opaax
{
    /**
     * @class TextureLoader
     *
     * Loads OpenGLTexture2D from disk via stb_image (inside OpenGLTexture2D ctor).
     */
    class OPAAX_API TextureLoader final : public IAssetLoader<OpenGLTexture2D>
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin IAssetLoader interface
    public:
        OpenGLTexture2D* Load(const char* InAbsolutePath) override;
        bool IsValid(OpenGLTexture2D* InAsset) override;
        //~ End IAssetLoader interface
    };

} // namespace Opaax