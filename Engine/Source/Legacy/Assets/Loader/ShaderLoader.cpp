#include "ShaderLoader.h"

#include "Renderer/ShaderAsset.h"

namespace Opaax
{
    ShaderAsset* ShaderLoader::Load(const char* InAbsPath, OpaaxStringID InCanonicalID)
    {
        return new ShaderAsset(OpaaxString(InAbsPath), InCanonicalID);
    }

    bool ShaderLoader::IsValid(ShaderAsset* InAsset)
    {
        return InAsset && InAsset->IsLoaded();
    }
}
