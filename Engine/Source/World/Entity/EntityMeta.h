#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/GUID/Guid.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // EntityMeta — per-entity identity stored in the World's registry: the stable Guid,
    //   a debug name, and the Map that authored the entity. Added automatically by
    //   World::CreateEntity, so every entity is addressable by Guid and self-describing
    //   and Each<EntityMeta> is the complete all-entities view.
    //
    //   This is IDENTITY, not user data: it deliberately does NOT satisfy CComponent and
    //   must never be registered as an ordinary component. The snapshot core writes these
    //   three fields by hand, because they are what an EntityData IS — a component entry
    //   round-tripping them would nest the entity's identity inside its own payload.
    // =============================================================================
    struct EntityMeta
    {
        Guid        Id;
        OpaaxString Name;
        MapId       OwnerMap; // invalid => runtime-spawned, never captured
    };
}
