#include "FontLoader.h"

#include "Renderer/Text/FontAsset.h"

namespace Opaax
{
    FontAsset* FontLoader::Load(const char* InAbsPath, OpaaxStringID InCanonicalID)
    {
        return new FontAsset(OpaaxString(InAbsPath), InCanonicalID);
    }

    bool FontLoader::IsValid(FontAsset* InAsset)
    {
        return InAsset && InAsset->IsLoaded();
    }
}
