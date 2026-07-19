#include "CollisionProfileLoader.h"

#include "Physics/Collision/CollisionProfile.h"

namespace Opaax
{
    CollisionProfile* CollisionProfileLoader::Load(const char* InAbsPath, OpaaxStringID InCanonicalID)
    {
        return new CollisionProfile(OpaaxString(InAbsPath), InCanonicalID);
    }

    bool CollisionProfileLoader::IsValid(CollisionProfile* InAsset)
    {
        // A Failed profile still yields a usable block-all filter, so treat it as valid
        // for registry purposes — a missing file should not strand the handle.
        return InAsset != nullptr;
    }
}
