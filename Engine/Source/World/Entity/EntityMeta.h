#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/GUID/Guid.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // EntityMeta — per-entity identity: Guid, name, owning map and parent.
    //   Added by World::CreateEntity to every entity. Not a regular component
    //   (not registered; saved by hand as part of EntityData).
    // =============================================================================
    struct EntityMeta
    {
        Guid        Id;
        OpaaxString Name;
        MapId       OwnerMap; // invalid = runtime-spawned, never saved
        Guid        Parent;   // invalid = root
    };
}
