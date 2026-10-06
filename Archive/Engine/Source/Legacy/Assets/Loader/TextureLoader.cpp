#include "TextureLoader.h"

#include "Renderer/Texture2D.h"

namespace Opaax
{
    Texture2D* TextureLoader::Load(const char* InAbsPath, OpaaxStringID InCanonicalID)
    {
        return new Texture2D(OpaaxString(InAbsPath), InCanonicalID);
    }

    bool TextureLoader::IsValid(Texture2D* InAsset)
    {
        return InAsset && InAsset->IsLoaded();
    }
}
