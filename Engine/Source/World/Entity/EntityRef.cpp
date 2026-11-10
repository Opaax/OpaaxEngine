#include "World/Entity/EntityRef.h"

#include "World/Entity/Entity.h"
#include "World/World.h"

namespace Opaax
{
    Entity EntityRef::Resolve(World& InWorld) const
    {
        return IsEmpty() ? Entity{} : InWorld.FindByGuid(Id);
    }
}
