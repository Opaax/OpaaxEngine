#include "TextureLoader.h"

#include "Renderer/Texture2D.h"

namespace Opaax
{
    Texture2D* TextureLoader::Load(const char* InAbsolutePath)
    {
        // Asset ID derived from the resolved absolute path — same scheme as
        // AssetRegistry's cache key, so handle.GetID() and the registry key match.
        return new Texture2D(OpaaxString(InAbsolutePath), OpaaxStringID(InAbsolutePath));
    }

    bool TextureLoader::IsValid(Texture2D* InAsset)
    {
        return InAsset && InAsset->IsLoaded();
    }
}
