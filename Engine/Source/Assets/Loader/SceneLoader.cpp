#include "SceneLoader.h"

#include "Scene/SceneAsset.h"

namespace Opaax
{
    SceneAsset* SceneLoader::Load(const char* InAbsPath, OpaaxStringID InCanonicalID)
    {
        return new SceneAsset(OpaaxString(InAbsPath), InCanonicalID);
    }

    bool SceneLoader::IsValid(SceneAsset* InAsset)
    {
        return InAsset != nullptr;
    }
}
