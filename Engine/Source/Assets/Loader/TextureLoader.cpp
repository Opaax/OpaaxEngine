#include "TextureLoader.h"

namespace Opaax
{
    OpenGLTexture2D* TextureLoader::Load(const char* InAbsolutePath)
    {
        return new OpenGLTexture2D(InAbsolutePath);
    }

    bool TextureLoader::IsValid(OpenGLTexture2D* InAsset)
    {
        return InAsset && InAsset->IsLoaded();
    }
}